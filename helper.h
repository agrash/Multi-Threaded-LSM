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

		Bookmark(const std::string_view key, uint64_t offset) : key(std::string(key)), offset(offset) {}
		Bookmark(std::string key, uint64_t offset) : key(std::move(key)), offset(offset) {}

		bool operator<(const Bookmark& other) const;
		bool operator<(const std::string& other) const;
		bool operator==(const std::string& other) const;
	};

	class BloomFilter {
	private:
		const uint64_t table_size;
		std::vector<uint64_t> hash_table;
		int num_hashes;

		static constexpr uint64_t word_size = sizeof(uint64_t) * 8;
	public:
		BloomFilter(uint64_t size, int num_hashes) : table_size(size), hash_table(size / word_size), num_hashes(num_hashes) {}

		void add(const std::string& key);
		void add(const std::string_view& key);
		bool contains(uint32_t h1, uint32_t h2) const;

		void set(uint64_t i);
		bool get(uint64_t i) const;

		std::vector<uint64_t>& getTable();
	};

	struct dataContainer {
		bool is_tombstone;
		std::string_view key;
		std::string_view val;
	};

}