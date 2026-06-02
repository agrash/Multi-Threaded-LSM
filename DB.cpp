#include "DB.h"

namespace lsm {

	DB::DB() : wal_log("wal.log"), filters_at_level(MAX_LEVEL + 1), readers_at_level(MAX_LEVEL + 1), reader_level_locks(MAX_LEVEL + 1), filter_level_locks(MAX_LEVEL + 1) {
		compactor_thread = std::thread(&DB::compactor, this);
		flush_thread = std::thread(&DB::memtableFlush, this);
	}

	DB::~DB() {
		run_memtable_flusher.store(false, std::memory_order_relaxed);
		start_flush.release();

		if (flush_thread.joinable()) {
			flush_thread.join();
		}

		std::cout<<"Stopped Flusher"<<std::endl;
		
		run_compactor.store(false, std::memory_order_relaxed);
		start_compactor.release();

		if (compactor_thread.joinable()) {
			compactor_thread.join();
		}

		std::cout<<"Stopped Compactor"<<std::endl;
	}

	void DB::checkAndHandleFlush() {
		{
			//Don't need active lock as only this thread can change the value of active.
			if (memtable[active].getSizeBytes() >= FLUSH_TRIGGER) {
				if (inactive_flushing.load(std::memory_order_acquire)) {
					std::cout<<"Writer Thread is Paused"<<std::endl;
					writer_waiting.store(true, std::memory_order_release);
					resume_writes.acquire();
				}

				{
					std::unique_lock<std::shared_mutex> lock(active_lock);
					active = 1 - active;
					inactive_flushing.store(true, std::memory_order_release);
				}

				start_flush.release();
			}
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
				std::unique_lock<std::shared_mutex> lock1(reader_level_locks[0]);
				std::unique_lock<std::shared_mutex> lock2(filter_level_locks[0]);
				readers_at_level[0].emplace_back(make_unique<SSTableReader>(flush_name));
				filters_at_level[0].emplace_back(std::move(new_filter));
			}

			wal_log.clear();

			{
				std::unique_lock<std::shared_mutex> lock(inactive_memtable_lock);
				memtable[1 - active].clear(); 
			}

			//checked here that order is not important there won't be a deadlock.
			inactive_flushing.store(false, std::memory_order_release);
			if (writer_waiting.exchange(false, std::memory_order_acquire)) {
				resume_writes.release();
			}

			std::cout<<"Finished Flush!"<<std::endl;

			{
				std::shared_lock<std::shared_mutex> lock(reader_level_locks[0]);
				if (readers_at_level[0].size() >= COMPACTION_TRIGGER) { start_compactor.release(); }
			}
		}

	}

	void DB::compactor() {
		while (run_compactor.load(std::memory_order_relaxed)) {
			start_compactor.acquire();

			size_t curr_level = 0;
			size_t filter_size = COMPACTION_TRIGGER	* bloom_filter_size;

			bool continue_compaction = false;
			{
				std::shared_lock<std::shared_mutex> lock(reader_level_locks[0]);
				continue_compaction = readers_at_level[0].size() >= COMPACTION_TRIGGER;

				if (readers_at_level[0].size() >= 2 * COMPACTION_TRIGGER) { start_compactor.release(); }
			}

			while (curr_level < MAX_LEVEL && continue_compaction) {

				std::cout<<"Trigerring Compaction"<<std::endl;

				size_t num_files = COMPACTION_TRIGGER;

				std::vector<std::string> file_paths;
				{
					std::shared_lock<std::shared_mutex> lock(reader_level_locks[curr_level]);
					for (size_t i=0; i<num_files; ++i) {
						file_paths.push_back(readers_at_level[curr_level][i]->getFilePath());
					}
				}

				bool remove_tombstones = (curr_level == curr_max_level);

				BloomFilter new_filter(filter_size, num_hashes);

				const std::string merged_file_path = prefix + std::to_string(curr_level + 1) + "_" + std::to_string(sstable_counter.fetch_add(1, std::memory_order_relaxed)) + ".db"; // This line is safe to run without a lock.

				SSTableMerger merge(remove_tombstones, file_paths, merged_file_path, new_filter); //Don't need locks for this. (Only compactor adds or remove bloom filters and the files are only made irrelevant by compactor.)	

				{
					std::unique_lock<std::shared_mutex> lock1(reader_level_locks[curr_level + 1]);
					std::unique_lock<std::shared_mutex> lock2(filter_level_locks[curr_level + 1]);
					readers_at_level[curr_level + 1].emplace_back(make_unique<SSTableReader>(merged_file_path));
					filters_at_level[curr_level + 1].emplace_back(std::move(new_filter));
				}

				{
					std::unique_lock<std::shared_mutex> lock1(reader_level_locks[curr_level]);
					std::unique_lock<std::shared_mutex> lock2(filter_level_locks[curr_level]);

					readers_at_level[curr_level].erase(readers_at_level[curr_level].begin(), readers_at_level[curr_level].begin() + num_files);
					filters_at_level[curr_level].erase(filters_at_level[curr_level].begin(), filters_at_level[curr_level].begin() + num_files);
				}

				filter_size *= COMPACTION_TRIGGER;
				++curr_level;
				if (curr_level > curr_max_level) { curr_max_level = curr_level; }

				{
					std::shared_lock<std::shared_mutex> lock(reader_level_locks[curr_level]);
					continue_compaction = readers_at_level[curr_level].size() >= COMPACTION_TRIGGER;
				}

				std::cout<<"Finished Compaction"<<std::endl;
			}
		}
	}

	void DB::put(const std::string& key, const std::string& val, bool tombstone) {
		wal_log.append(tombstone, key, val);

		{
			std::shared_lock<std::shared_mutex> lock1(active_lock);
			std::unique_lock<std::shared_mutex> lock2(active_memtable_lock);
			memtable[active].insert(tombstone, key, val);
		}

		checkAndHandleFlush();
	}

	void DB::remove(const std::string& key) {
		put(key, "", true);
	}

	extern std::pair<uint32_t, uint32_t> getHashes(const std::string& key);

	std::optional<std::string> DB::searchInMemtable(const std::string& key) {
		std::shared_lock<std::shared_mutex> lock1(active_lock);

		{
			std::shared_lock<std::shared_mutex> lock2(active_memtable_lock);
			auto it = memtable[active].search(key);
			if (it != memtable[active].end()) {
				if (it->is_tombstone) {return "";}
				return it->val;
			}
		}

		{
			std::shared_lock<std::shared_mutex> lock2(inactive_memtable_lock);
			auto it = memtable[1-active].search(key);
			if (it != memtable[1-active].end()) {
				if (it->is_tombstone) {return "";}
				return it->val;
			}
		}

		return std::nullopt;
	}

	std::optional<std::string> DB::get(const std::string& key) {
		auto memtable_result = searchInMemtable(key);
		if (memtable_result) { return memtable_result; }

		auto [h1, h2] = getHashes(key);

		for (size_t level = 0; level <= MAX_LEVEL; ++level) {

			std::shared_lock<std::shared_mutex> lock1(reader_level_locks[level]);
			std::shared_lock<std::shared_mutex> lock2(filter_level_locks[level]);

			for (int i = (int)readers_at_level[level].size() - 1; i>=0; --i) {
				if (!filters_at_level[level][i].contains(h1, h2)) {continue;}

				auto res = readers_at_level[level][i]->findKey(key);
				if (res) {return res;}
			}
		}

		return std::nullopt;
	}

	void DB::recover() {
		std::shared_lock<std::shared_mutex> lock1(active_lock);
		std::unique_lock<std::shared_mutex> lock2(active_memtable_lock);
		wal_log.recover(memtable[active]);
	}


}