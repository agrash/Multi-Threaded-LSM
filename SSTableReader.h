#pragma once
#include <iostream>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <string_view>
#include <sys/mman.h>
#include "helper.h"

namespace lsm {

	class SSTableReader {
	private:
		int fd;
		std::string filepath;
		const char* file_data;
		uint64_t file_size;

		BloomFilter filter;

		uint64_t index_offset;

		std::vector<Bookmark> index;

		bool delete_file_at_destruction = true;

	public:
		SSTableReader(const std::string& filepath, BloomFilter& filter);

		~SSTableReader();
		std::optional<std::string> findKey(const std::string& key, uint32_t h1, uint32_t h2);

		void preserveTable();

		std::string getFilePath();
	};

}