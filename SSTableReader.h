#pragma once
#include <iostream>
#include <filesystem>
#include "helper.h"

namespace lsm {

	class SSTableReader {
	private:
		std::ifstream file;
		std::string filepath;

		uint64_t index_offset;
		std::streampos index_start;

		std::vector<Bookmark> index;

	public:
		SSTableReader(const std::string& filepath);

		~SSTableReader();
		std::optional<std::string> findKey(const std::string& key);

		std::string getFilePath();
	};

}