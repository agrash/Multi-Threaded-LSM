#pragma once
#include "helper.h"
#include "SkipList.h"
#include "WAL.h"
#include "SSTableBuilder.h"
#include "SSTableReader.h"
#include "SSTableMerger.h"

#include <thread>
#include <atomic>
#include <semaphore>
#include <mutex>
#include <shared_mutex>

namespace lsm {
	//Important: Always lock readers before filters to avoid deadlocks.

	class DB {
	private:
		WAL wal_log;
		std::array<SkipList, 2> memtable;
		int active = 0;

		std::shared_mutex active_lock;
		std::shared_mutex active_memtable_lock;
		std::shared_mutex inactive_memtable_lock;


		static constexpr size_t FLUSH_TRIGGER = 4 * 1024 * 1024;
		static constexpr size_t bloom_filter_size = FLUSH_TRIGGER / (16);  // Keep this a power of 2, if want to change it then need to change the mod logic in bloom filter.
		const int num_hashes = 10;

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
		std::atomic<bool> inactive_flushing{false};
		std::atomic<bool> writer_waiting{false};

		//std::atomic<bool> inactive_flushing{false};
		//std::atomic<bool> pause_writes{false};

		const std::string prefix = "./Tables/sstable";
		std::atomic<int> sstable_counter = 0; // to use only by a single writer thread.

		std::vector<std::vector<BloomFilter>> filters_at_level;
		std::vector<std::vector<std::unique_ptr<SSTableReader>>> readers_at_level;
		std::vector<std::shared_mutex> reader_level_locks;
		std::vector<std::shared_mutex> filter_level_locks;

		void checkAndHandleFlush();
		void memtableFlush();

		void compactor();

		std::optional<std::string> searchInMemtable(const std::string& key);

	public:
		DB();
		~DB();

		void put(const std::string& key, const std::string& val, bool tombstone = false);
		void remove(const std::string& key);
		std::optional<std::string> get(const std::string& key);

		void recover();
	};

}