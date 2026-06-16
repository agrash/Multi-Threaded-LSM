#include "DB.h"

namespace lsm {

	DB::DB() : readers_at_level(MAX_LEVEL + 1, std::make_shared<LevelReaders>()) {
		compactor_thread = std::thread(&DB::compactor, this);
		flush_thread = std::thread(&DB::memtableFlush, this);

		wal_log[active].open(0);
		writer_thread = std::thread(&DB::writer, this);

		//for (auto& thread : reader_threads) thread = std::thread(&DB::reader, this);
	}

	DB::~DB() {

		run_writer.store(false, std::memory_order_relaxed);
		//run_readers.store(false, std::memory_order_relaxed);

		writer_buffer.stop(1);
		//reader_buffer.stop(NUM_READER_THREADS);

		if (writer_thread.joinable()) writer_thread.join();
		/*for (auto& thread : reader_threads) {
		    if (thread.joinable()) thread.join();
		}*/

		run_memtable_flusher.store(false, std::memory_order_relaxed);
		start_flush.release();

		if (flush_thread.joinable()) {
			flush_thread.join();
		}

		run_compactor.store(false, std::memory_order_relaxed);
		start_compactor.release();

		if (compactor_thread.joinable()) {
			compactor_thread.join();
		}

		for (auto& readers : readers_at_level) {
			for (auto& reader : *readers) reader->preserveTable();
		}

	}

	void DB::memtableFlush() {
	// variable active is not going to change after flusher is sent a signal by writer until flusher completes its job.

		while (run_memtable_flusher.load(std::memory_order_relaxed)) {
			start_flush.acquire();


			std::cout<<"Trigerring Flush!"<<std::endl;

			BloomFilter new_filter(bloom_filter_size, num_hashes);

			const std::string flush_name = prefix + "0_" + std::to_string(sstable_counter.fetch_add(1, std::memory_order_relaxed)) + ".db";

			SSTableBuilder builder(flush_name, new_filter);
			builder.flush(memtable[1 - active]); 

			{
				std::unique_lock<std::mutex> lock(level0_lock);
				auto old = std::atomic_load_explicit(&readers_at_level[0], std::memory_order_acquire);
				auto neu = make_shared<LevelReaders>(*old);
				neu->push_back(std::make_shared<SSTableReader>(flush_name, new_filter));

				std::atomic_store_explicit(&readers_at_level[0], neu, std::memory_order_release);
			}


			{
				std::unique_lock<std::shared_mutex> lock(inactive_memtable_lock);
				memtable[1 - active].clear(); 
			}

			{
				std::unique_lock<std::mutex> lock(writer_flusher);
				inactive_flushing = false;
				if (writer_waiting) {
					writer_waiting = false;
					resume_writes.release();
				}
			}

			std::cout<<"Finished Flush!"<<std::endl;

			{
				std::unique_lock<std::mutex> lock(level0_lock);
				if (readers_at_level[0]->size() >= COMPACTION_TRIGGER) { start_compactor.release(); }
			}
		}

		std::cout<<"Stopped Flusher"<<std::endl;

	}

	void DB::compactor() {
		while (run_compactor.load(std::memory_order_relaxed)) {
			start_compactor.acquire();

			size_t curr_level = 0;
			size_t filter_size = COMPACTION_TRIGGER	* bloom_filter_size;

			bool continue_compaction = false;
			{
				std::unique_lock<std::mutex> lock(level0_lock);
				continue_compaction = readers_at_level[0]->size() >= COMPACTION_TRIGGER;

				if (readers_at_level[0]->size() >= 2 * COMPACTION_TRIGGER) { start_compactor.release(); }
			}

			while (curr_level < MAX_LEVEL && continue_compaction) {

				std::cout<<"Trigerring Compaction"<<std::endl;

				size_t num_files = COMPACTION_TRIGGER;

				std::vector<std::shared_ptr<SSTableReader>> files;
				{
					auto readers = std::atomic_load_explicit(&readers_at_level[curr_level], (std::memory_order_acquire));
					for (size_t i=0; i<num_files; ++i) {
						files.push_back((*readers)[i]);
					}
				}

				bool remove_tombstones = (curr_level == curr_max_level);

				BloomFilter new_filter(filter_size, num_hashes);

				const std::string merged_file_path = prefix + std::to_string(curr_level + 1) + "_" + std::to_string(sstable_counter.fetch_add(1, std::memory_order_relaxed)) + ".db"; // This line is safe to run without a lock.

				SSTableMerger merge(remove_tombstones, files, merged_file_path, new_filter); //Don't need locks for this. (Only compactor adds or remove bloom filters and the files are only made irrelevant by compactor.)	

				{
					auto old = std::atomic_load_explicit(&readers_at_level[curr_level + 1], std::memory_order_acquire);
					auto neu = make_shared<LevelReaders>(*old);
					neu->push_back(std::make_shared<SSTableReader>(merged_file_path, new_filter));

					std::atomic_store_explicit(&readers_at_level[curr_level + 1], neu, std::memory_order_release);
				}

				if (curr_level == 0) {
					std::unique_lock<std::mutex> lock(level0_lock);
					auto old = std::atomic_load_explicit(&readers_at_level[curr_level], std::memory_order_acquire);
					auto neu = make_shared<LevelReaders>(*old);
					neu->erase(neu->begin(), neu->begin() + num_files);

					std::atomic_store_explicit(&readers_at_level[curr_level], neu, std::memory_order_release);
				}
				else {
					auto old = std::atomic_load_explicit(&readers_at_level[curr_level], std::memory_order_acquire);
					auto neu = make_shared<LevelReaders>(*old);
					neu->erase(neu->begin(), neu->begin() + num_files);

					std::atomic_store_explicit(&readers_at_level[curr_level], neu, std::memory_order_release);
				}

				filter_size *= COMPACTION_TRIGGER;
				++curr_level;
				if (curr_level > curr_max_level) { curr_max_level = curr_level; }

				continue_compaction = readers_at_level[curr_level]->size() >= COMPACTION_TRIGGER;

				std::cout<<"Finished Compaction"<<std::endl;
			}

		}

		std::cout<<"Stopped Compactor"<<std::endl;
	}



	void DB::writer() {
		std::vector<writeBufferElem> container;
		container.reserve(RING_BUFFER_SIZE / 2);
		while (run_writer.load(std::memory_order_relaxed)) {
			container.clear();
			writer_buffer.consume(container, highest_write_completed.load(std::memory_order_relaxed));

			if (!run_writer.load(std::memory_order_relaxed)) break;

			uint64_t highest_completed;
			for (auto& job : container) {
				insert(job.key, job.val, job.tombstone);
				highest_completed = job.sequence_number;
			}

			wal_log[active].flush();

			// sequence number of jobs completed by writer always increases.
			highest_write_completed.store(highest_completed, std::memory_order_relaxed);
			highest_write_completed.notify_all();
		}

		std::cout<<"Stopped Writer\n";
	}

	/*void DB::reader() {
		std::vector<char> buffer(100 * 100);
		while (run_readers.load(std::memory_order_relaxed)) {
			auto job = reader_buffer.consume();

			if (!run_readers.load(std::memory_order_relaxed)) break;

			auto current_highest_write = highest_write_completed.load(std::memory_order_relaxed);

			while (current_highest_write < job.highest_write) {

				highest_write_completed.wait(current_highest_write, std::memory_order_relaxed);
				current_highest_write = highest_write_completed.load(std::memory_order_relaxed);

			}

			*job.result_container = search(job.key, buffer); // NRVO should trigger, so don't need to move here.
			job.signal_done->release();

		}

		std::cout<<"Stopped a Reader\n";
	}*/

	void DB::checkAndHandleFlush() {
		{
			//Don't need active lock as only this thread can change the value of active.
			if (memtable[active].getSizeBytes() >= FLUSH_TRIGGER) {
				wal_log[active].flush();

				bool wait = false;
				{
				 	std::unique_lock<std::mutex> lock(writer_flusher);
				 	if (inactive_flushing) {
						std::cout<<"Writer Thread is Paused"<<std::endl;
						writer_waiting = true;
						wait = true;
					}
				}

				if (wait) resume_writes.acquire(); // flusher changes status of writer_waiting.

				{
					std::unique_lock<std::shared_mutex> lock(active_lock);
					active = 1 - active;
					inactive_flushing = true; // no need for lock here as only one memtable is flushed at a time.
				}

				start_flush.release();

				wal_log[active].clear();
				wal_log[active].open(wal_log_counter++);
			}
		}
	}


	void DB::put(const std::string& key, const std::string& val, bool tombstone) {
		writeBufferElem job{ tombstone, key, val, 0 };

		// going to update sequence number and highest write received inside the ring buffer.
		writer_buffer.produce(job, job.sequence_number, highest_write_received); // need to put in lock to maintain the property that writer takes job in increasing order of sequence number.
	}

	void DB::remove(const std::string& key) {
		put(key, "", true);
	}

	std::optional<std::string> DB::get(const std::string& key) {

		/*std::optional<std::string> result;
		std::binary_semaphore signal_done{0};

		readBufferElem job{key, &result, &signal_done, 0};

		{
			std::shared_lock<std::shared_mutex> lock(highest_write_lock);
			job.highest_write = highest_write_received;
		}

		reader_buffer.produce(job);

		signal_done.acquire();

		return result;*/

		uint64_t target = highest_write_received.load(std::memory_order_acquire);
		auto current_highest_write = highest_write_completed.load(std::memory_order_relaxed);

		while (current_highest_write < target) {

			highest_write_completed.wait(current_highest_write, std::memory_order_relaxed);
			current_highest_write = highest_write_completed.load(std::memory_order_relaxed);

		}

		return search(key);
	}

	void DB::insert(std::string& key, std::string& val, bool tombstone) {
		wal_log[active].append(tombstone, key, val);

		{
			std::shared_lock<std::shared_mutex> lock1(active_lock); // Can probably remove this lock.
			std::unique_lock<std::shared_mutex> lock2(active_memtable_lock);
			memtable[active].insert(tombstone, key, val);
		}

		checkAndHandleFlush();
	}

	extern std::pair<uint32_t, uint32_t> getHashes(const std::string& key);

	std::optional<std::string> DB::searchInMemtable(const std::string& key) {
		std::shared_lock<std::shared_mutex> lock1(active_lock);

		{
			std::shared_lock<std::shared_mutex> lock2(active_memtable_lock);
			auto it = memtable[active].search(key);
			if (it != memtable[active].end()) {
				if (it->is_tombstone) {return "";}
				return std::string(it->val);
			}
		}

		{
			std::shared_lock<std::shared_mutex> lock2(inactive_memtable_lock);
			auto it = memtable[1-active].search(key);
			if (it != memtable[1-active].end()) {
				if (it->is_tombstone) {return "";}
				return std::string(it->val);
			}
		}

		return std::nullopt;
	}

	std::optional<std::string> DB::search(const std::string& key) {
		auto memtable_result = searchInMemtable(key);
		if (memtable_result) { return memtable_result; }

		//std::cout<<"Not in memtable"<<std::endl;

		auto [h1, h2] = getHashes(key);

		for (size_t level = 0; level <= MAX_LEVEL; ++level) {

			auto readers = std::atomic_load_explicit(&readers_at_level[level], std::memory_order_acquire);

			for (int i = (int)readers->size() - 1; i>=0; --i) {
				auto res = (*readers)[i]->findKey(key, h1, h2);
				if (res) {return res;}
			}
		}

		return std::nullopt;
	}


	void DB::recover() {
		std::shared_lock<std::shared_mutex> lock1(active_lock);
		std::unique_lock<std::shared_mutex> lock2(active_memtable_lock);
		wal_log[active].recover(memtable[active], "");
	}


}