#include "SSTableBuilder.h"

namespace lsm {


	SSTableBuilder::SSTableBuilder(const std::string& filepath, BloomFilter& filter) : added(0), current_offset(0), filter(filter) {
		file.open(filepath, std::ios::out | std::ios::trunc | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("Unable to open " + filepath);
		}
	}

	SSTableBuilder::~SSTableBuilder() {
		if (file.is_open()) {
			file.close();
		}
	}

	void SSTableBuilder::flush(const SkipList& memtable) {

		for (auto it = memtable.begin(); it != memtable.end(); ++it) {
			writeEntry(it->is_tombstone, it->key, it->val);
		}

		writeIndex();

	}

	extern std::string encode(bool is_tombstone, const std::string& key, const std::string& val);
	extern std::string encode(bool is_tombstone, const std::string_view key, const std::string_view val);

	void SSTableBuilder::writeEntry(bool is_tombstone, const std::string& key, const std::string& val) {
		if (added == 0) {
			index.emplace_back(key, current_offset);
		}

		std::string buffer = encode(is_tombstone, key, val);
		file.write(buffer.data(), buffer.size());

		filter.add(key);

		current_offset += buffer.size();
		++added;
		if (added == INDEX_ENTRY_SIZE) {added = 0;}
	}

	void SSTableBuilder::writeEntry(bool is_tombstone, const std::string_view key, const std::string_view val) {
		if (added == 0) {
			index.emplace_back(std::string(key), current_offset);
		}

		std::string buffer = encode(is_tombstone, key, val);
		file.write(buffer.data(), buffer.size());

		filter.add(key);

		current_offset += buffer.size();
		++added;
		if (added == INDEX_ENTRY_SIZE) {added = 0;}
	}

	void SSTableBuilder::writeIndex() {
		uint64_t index_offset = current_offset;

		for (auto& [key, offset] : index) {

			uint32_t length = key.size();
			file.write(reinterpret_cast<const char*> (&length), sizeof(uint32_t));

			file.write(key.data(), key.size());

			file.write(reinterpret_cast<const char*> (&offset), sizeof(uint64_t));
		}

		file.write(reinterpret_cast<const char*> (&index_offset), sizeof(uint64_t));

		file.flush();
		file.close();
	}

}