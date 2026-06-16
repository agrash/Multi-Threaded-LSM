#pragma once
#include "SkipList.h"
#include "helper.h"

namespace lsm {

	class WAL {
	private:
		FILE* file;
		std::string filepath;
		const std::string dir;

	public:
		WAL(const std::string& dir);
		~WAL();

		void open(const uint64_t log_number);
		void writeKeyOrVal(const std::string_view s);

		void append(bool is_tombstone, const std::string& key, const std::string& val);
		void flush();

		void recover(SkipList& memtable, const std::string& filepath);
		void clear();
	};

}