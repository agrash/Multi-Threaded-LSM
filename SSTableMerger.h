#pragma once
#include "helper.h"
#include "SSTableBuilder.h"

namespace lsm {

	struct dataContainer {
		bool is_tombstone;
		std::string key;
		std::string val;

		dataContainer() {}
	};

	class SSTableIterator {
	private:
		std::ifstream file;
		uint64_t index_start;
		std::streampos pos;

	public:
		SSTableIterator(const std::string& filepath);
		~SSTableIterator();

		std::optional<dataContainer> getNext();
	};

	class SSTableMerger {
	private:
		bool remove_tombstones;

		SSTableBuilder merged_file;

		std::vector<std::unique_ptr<SSTableIterator>> iterators;

		void merge();
		void writeEntry(dataContainer& data);
		void writeIndex();

	public:
		SSTableMerger(bool remove_tombstones, const std::vector<std::string>& filepaths, const std::string& merged_file_path, BloomFilter& filter);
	};

}