#pragma once
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <optional>
#include "MurmurHash3.h"

namespace lsm {

	std::string encode(bool is_tombstone, const std::string& key, const std::string& val);
	bool decode(std::ifstream& file, bool& is_tombstone, std::string& key, std::string& val);

	std::pair<uint32_t, uint32_t> getHashes(const std::string& key);

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
		BloomFilter(int size, int num_hashes) : hash_table(size), num_hashes(num_hashes) {}

		void add(const std::string& key);
		bool contains(uint32_t h1, uint32_t h2);
	};

}