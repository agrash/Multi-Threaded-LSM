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

	std::string encode(bool is_tombstone, const std::string_view key, const std::string_view val) {
		std::string buffer;
		uint32_t final_length = 1 + 4 + key.size() + (is_tombstone ? 0 : 4 + val.size());
		buffer.reserve(final_length);

		uint8_t tombstone = is_tombstone ? 1 : 0;
		const char* tombstone_ptr = reinterpret_cast<const char*> (&tombstone);
		buffer.insert(buffer.end(), tombstone_ptr, tombstone_ptr + sizeof(uint8_t));

		auto helper = [&buffer] (const std::string_view s) {
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


	// This function is not going to check whether you are checking out of bounds or not.
	uint64_t decode(char* data, uint64_t offset, bool& is_tombstone, std::string& key, std::string& val) {

		uint64_t bytes_read = 0;

		auto helper = [data](std::string& buffer, uint64_t offset) {
			uint32_t length;
			memcpy(&length, data + offset, sizeof(uint32_t));
			offset += sizeof(uint32_t);

			if (length > buffer.size()) {
				buffer.append(length - buffer.size(), '\0');
			}
			memcpy(buffer.data(), data + offset, length);
			buffer.resize(length);

			return sizeof(uint32_t) + length;
		};

		uint8_t tombstone;
		memcpy(&tombstone, data + offset, sizeof(uint8_t));
		//std::cout<<(int)tombstone<<std::endl;

		bytes_read += 1;

		bytes_read += helper(key, offset + bytes_read);
		if (tombstone == 0) {
			is_tombstone = false;
			//uint32_t val_len;
		    //memcpy(&val_len, data + offset + bytes_read, sizeof(uint32_t));
		    //std::cout << "val_len=" << val_len << std::endl;
			bytes_read += helper(val, offset + bytes_read);

		}
		else {
			is_tombstone = true;
		}

		return bytes_read;
	}

	std::pair<uint32_t, uint32_t> getHashes(const std::string& key) {
		uint32_t h1, h2; 
		MurmurHash3_x86_32(key.data(), key.size(), 0, &h1);
		MurmurHash3_x86_32(key.data(), key.size(), 1, &h2);

		h2 |= 1;

		return {h1, h2};
	}

	std::pair<uint32_t, uint32_t> getHashes(const std::string_view key) {
		uint32_t h1, h2; 
		MurmurHash3_x86_32(key.data(), key.size(), 0, &h1);
		MurmurHash3_x86_32(key.data(), key.size(), 1, &h2);

		h2 |= 1;

		return {h1, h2};
	}

	void BloomFilter::add(const std::string& key) {

		auto [h1, h2] = getHashes(key);

		uint64_t mod_mask = table_size - 1;
		for (uint64_t i=0; i<num_hashes; ++i) {
			uint64_t hash = static_cast<uint64_t>(h1) + i * static_cast<uint64_t>(h2);

			hash = hash & mod_mask;
			set(static_cast<uint32_t>(hash));
		}
	}

	void BloomFilter::add(const std::string_view& key) {

		auto [h1, h2] = getHashes(key);

		uint64_t mod_mask = table_size - 1;
		for (uint64_t i=0; i<num_hashes; ++i) {
			uint64_t hash = static_cast<uint64_t>(h1) + i * static_cast<uint64_t>(h2);

			hash = hash & mod_mask;
			set(static_cast<uint32_t>(hash));
		}
	}

	bool BloomFilter::contains(uint32_t h1, uint32_t h2) const {

		uint64_t mod_mask = table_size - 1;
		for (uint64_t i=0; i<num_hashes; ++i) {
			uint64_t hash = static_cast<uint64_t>(h1) + i * static_cast<uint64_t>(h2);

			hash = hash & mod_mask;
			if (!get(static_cast<uint32_t>(hash))) {return false;}
		}
		return true;
	}

	void BloomFilter::set(uint64_t i) {
		uint64_t mask = static_cast<uint64_t>(1) << (i % word_size);
		hash_table[i / word_size] |= mask;
	}

	bool BloomFilter::get(uint64_t i) const {
		uint64_t mask = static_cast<uint64_t>(1) << (i % word_size);
		return hash_table[i / word_size] & mask;
	}

	std::vector<uint64_t>& BloomFilter::getTable() {
		return hash_table;
	}

	uint64_t BloomFilter::getElems() const {
		return table_size;
	}

}