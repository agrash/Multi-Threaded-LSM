#pragma once
#include <iostream>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <string_view>
#include "helper.h"

namespace lsm {

	class SSTableReader {
	private:
		int fd;
		std::string filepath;

		uint64_t index_offset;

		std::vector<Bookmark> index;

	public:
		SSTableReader(const std::string& filepath);

		~SSTableReader();
		std::optional<std::string> findKey(const std::string& key, std::vector<char>& buffer);

		std::string getFilePath();
	};

}