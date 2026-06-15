#pragma once
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <optional>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include "MurmurHash3.h"

namespace lsm {

	std::string encode(bool is_tombstone, const std::string& key, const std::string& val);
	std::string encode(bool is_tombstone, const std::string_view key, const std::string_view val);
	bool decode(std::ifstream& file, bool& is_tombstone, std::string& key, std::string& val);
	uint64_t decode(char* data, uint64_t offset, bool& is_tombstone, std::string& key, std::string& val);

	std::pair<uint32_t, uint32_t> getHashes(const std::string& key);
	std::pair<uint32_t, uint32_t> getHashes(const std::string_view key);

	struct Bookmark {
		std::string key;
		uint64_t offset;

		Bookmark(const std::string& key, uint64_t offset) : key(key), offset(offset) {}

		bool operator<(const Bookmark& other) const;
		bool operator<(const std::string& other) const;
		bool operator==(const std::string& other) const;
	};

	class BloomFilter {
	private:
		std::vector<bool> hash_table;
		int num_hashes;
	public:
		BloomFilter(size_t size, int num_hashes) : hash_table(size), num_hashes(num_hashes) {}

		void add(const std::string& key);
		void add(const std::string_view key);
		bool contains(uint32_t h1, uint32_t h2) const;
	};

	struct dataContainer {
		bool is_tombstone;
		std::string key;
		std::string val;

		dataContainer() {}
		dataContainer(std::string key, std::string val, bool is_tombstone) : key(std::move(key)), val(std::move(val)), is_tombstone(is_tombstone) {}
	};

}