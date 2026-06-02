#include "helper.h"

namespace lsm {

	bool Bookmark::operator<(const Bookmark& other) const {
		return key < other.key;
	}

	bool Bookmark::operator<(const std::string& other) const {
		return key < other;
	}

	bool Bookmark::operator==(const std::string& other) const {
		return key == other;
	}

	std::string encode(bool is_tombstone, const std::string& key, const std::string& val) {
		std::string buffer;
		uint32_t final_length = 1 + 4 + key.size() + (is_tombstone ? 0 : 4 + val.size());
		buffer.reserve(final_length);

		uint8_t tombstone = is_tombstone ? 1 : 0;
		const char* tombstone_ptr = reinterpret_cast<const char*> (&tombstone);
		buffer.insert(buffer.end(), tombstone_ptr, tombstone_ptr + sizeof(uint8_t));

		auto helper = [&buffer] (const std::string& s) {
			uint32_t length = s.size();
			const char* ptr = reinterpret_cast<const char*> (&length);
			buffer.insert(buffer.end(), ptr, ptr + sizeof(uint32_t));

			buffer.insert(buffer.end(), s.begin(), s.end());
		};

		helper(key);
		if (is_tombstone) {return buffer;}

		helper(val);
		return buffer;
	}

	bool decode(std::ifstream& infile, bool& is_tombstone, std::string& key, std::string& val) {

		auto helper = [&infile](std::string& buffer) {
			uint32_t length;
			infile.read(reinterpret_cast<char*> (&length), sizeof(uint32_t));

			if (length > buffer.size()) {
				buffer.append(length - buffer.size(), '\0');
			}
			infile.read(buffer.data(), length);
			buffer.resize(length);
		};

		uint8_t tombstone;
		infile.read(reinterpret_cast<char*> (&tombstone), sizeof(uint8_t));
		if (infile.eof()) {return false;}

		helper(key);
		if (tombstone == 0) {
			is_tombstone = false;
			helper(val);
		}
		else {
			is_tombstone = true;
		}

		return true;
	}

	std::pair<uint32_t, uint32_t> getHashes(const std::string& key) {
		uint32_t h1, h2; 
		MurmurHash3_x86_32(key.data(), key.size(), 0, &h1);
		MurmurHash3_x86_32(key.data(), key.size(), 1, &h2);

		h2 |= 1;

		return {h1, h2};
	}

	void BloomFilter::add(const std::string& key) {

		auto [h1, h2] = getHashes(key);

		uint64_t mod_mask = hash_table.size() - 1;
		for (uint64_t i=0; i<num_hashes; ++i) {
			uint64_t hash = static_cast<uint64_t>(h1) + i * static_cast<uint64_t>(h2);

			hash = hash & mod_mask;
			hash_table[static_cast<uint32_t>(hash)] = true;
		}
	}

	bool BloomFilter::contains(uint32_t h1, uint32_t h2) {

		uint64_t mod_mask = hash_table.size() - 1;
		for (uint64_t i=0; i<num_hashes; ++i) {
			uint64_t hash = static_cast<uint64_t>(h1) + i * static_cast<uint64_t>(h2);

			hash = hash & mod_mask;
			if (!hash_table[static_cast<uint32_t>(hash)]) {return false;}
		}
		return true;
	}

}