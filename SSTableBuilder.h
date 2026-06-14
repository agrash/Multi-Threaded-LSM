#pragma once
#include "helper.h"
#include "SkipList.h"

namespace lsm {

	class SSTableBuilder {
	private:
		std::ofstream file;
		std::vector<Bookmark> index;
		size_t added;
		uint64_t current_offset;

		const size_t INDEX_ENTRY_SIZE = 40;

		BloomFilter& filter;

	public:
		SSTableBuilder(const std::string& filepath, BloomFilter& filter);
		~SSTableBuilder();

		void writeEntry(bool is_tombstone, const std::string& key, const std::string& val);
		void writeEntry(bool is_tombstone, const std::string_view key, const std::string_view val);
		void writeIndex();

		void flush(const SkipList& memtable);
	};

}