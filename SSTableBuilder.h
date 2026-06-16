#pragma once
#include "helper.h"
#include "SkipList.h"
#include <stdio.h>

namespace lsm {

	class SSTableBuilder {
	private:
		std::vector<Bookmark> index;
		size_t added;
		uint64_t current_offset;

		FILE* file;

		const size_t INDEX_ENTRY_SIZE = 40;

		BloomFilter& filter;

	public:
		SSTableBuilder(const std::string& filepath, BloomFilter& filter);

		void writeKeyOrVal(const std::string_view s);
		void writeEntry(bool is_tombstone, const std::string_view key, const std::string_view val);
		void writeIndex();

		void flush(const SkipList& memtable);
	};

}