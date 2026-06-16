#pragma once
#include "helper.h"
#include "SkipList.h"
#include "WAL.h"
#include "SSTableBuilder.h"
#include "SSTableReader.h"
#include "SSTableMerger.h"
#include "RingBuffer.h"

#include <thread>
#include <atomic>
#include <semaphore>
#include <mutex>
#include <shared_mutex>
#include <future>
#include <array>

namespace lsm {
	//Important: Always lock readers before filters to avoid deadlocks. (Not relevant now)
	//Important: Consider Recover to be non-functional.
	// put and get are written to be capable of called by multiple threads but pipeline for receiving currently does not send the results back to those threads instead puts them in a single buffer to be collected.
	// Number of writers (DB writers) = 1, Number of readers (DB readers) = NUM_READER_THREADS.

	class DB {
	private:
		struct writeBufferElem {
			bool tombstone;
			std::string key;
			std::string val;
			uint64_t sequence_number;

			writeBufferElem() {}
			writeBufferElem(bool tombstone, std::string key, std::string val, uint64_t sequence_number) : tombstone(tombstone), key(std::move(key)), val(std::move(val)), sequence_number(sequence_number) {}
		};

		/*struct readBufferElem {
			std::string key;
			std::optional<std::string>* result_container;
			std::binary_semaphore* signal_done;
			uint64_t highest_write;

			readBufferElem() : result_container(nullptr), signal_done(nullptr), highest_write(0) {}
			readBufferElem(std::string key, std::optional<std::string>* result_container, std::binary_semaphore* signal_done, uint64_t highest_write) : key(std::move(key)), result_container(result_container), signal_done(signal_done), highest_write(highest_write) {}
		};*/

		const std::string wal_dir = "./Logs";
		std::array<WAL, 2> wal_log{wal_dir, wal_dir};
		uint64_t wal_log_counter = 1;

		std::array<SkipList, 2> memtable;
		int active = 0;

		std::shared_mutex active_lock;
		std::shared_mutex active_memtable_lock;
		std::shared_mutex inactive_memtable_lock;

		static constexpr size_t RING_BUFFER_SIZE = 20000;
		static constexpr size_t NUM_READER_THREADS = 10;
		

		std::atomic<uint64_t> highest_write_received{0};

		RingBuffer<writeBufferElem, RING_BUFFER_SIZE> writer_buffer;
		//RingBuffer<readBufferElem, RING_BUFFER_SIZE> reader_buffer; 

		uint64_t sequence_counter = 1;
		std::atomic<uint64_t> highest_write_completed{0};

		// need to tune them manually based on key-val sizes and required FP rate.
		static constexpr size_t FLUSH_TRIGGER = 64 * 1024 * 1024;
		static constexpr size_t bloom_filter_size = FLUSH_TRIGGER / (8);  // Keep this a power of 2, if want to change it then need to change the mod logic in bloom filter.
		const int num_hashes = 8;

		const size_t COMPACTION_TRIGGER = 4; // Merge after reaching this many files on a level. Keep this a power of 2 as well or change bloom filter mod logic.
		const size_t MAX_LEVEL = 7;
		size_t curr_max_level = 0; // to use only by compactor

		std::thread compactor_thread;
		std::atomic<bool> run_compactor{true};
		std::binary_semaphore start_compactor{0};

		std::thread flush_thread;
		std::atomic<bool> run_memtable_flusher{true};
		std::binary_semaphore start_flush{0};

		std::binary_semaphore resume_writes{0};
		bool inactive_flushing{false};
		bool writer_waiting{false};
		std::mutex writer_flusher;

		std::atomic<bool> run_writer{true};
		std::thread writer_thread;

		std::atomic<bool> run_readers{true};
		std::array<std::thread, NUM_READER_THREADS> reader_threads;


		const std::string prefix = "./Tables/sstable";
		std::atomic<int> sstable_counter = 0; // to use only by a single writer thread.

		std::vector<std::vector<std::shared_ptr<SSTableReader>>> readers_at_level;
		std::vector<std::shared_mutex> reader_level_locks;

		void checkAndHandleFlush();
		void memtableFlush();

		void compactor();

		void writer();
		void reader();

		std::optional<std::string> searchInMemtable(const std::string& key);

		void insert(std::string& key, std::string& val, bool tombstone = false);

		std::optional<std::string> search(const std::string& key);


	public:
		DB();
		~DB();

		void put(const std::string& key, const std::string& val, bool tombstone = false);
		void remove(const std::string& key);
		std::optional<std::string> get(const std::string& key);

		void recover();
	};

}