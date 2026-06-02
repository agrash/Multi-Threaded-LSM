#pragma once
#include "SkipList.h"
#include "helper.h"

namespace lsm {

	class WAL {
	private:
		std::ofstream file;
		const std::string filepath;

	public:
		WAL(const std::string& path);
		~WAL();

		void append(bool is_tombstone, const std::string& key, const std::string& val);
		void flush();

		void recover(SkipList& memtable);
		void clear();
	};

}