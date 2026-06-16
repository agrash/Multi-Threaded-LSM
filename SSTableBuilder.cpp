#include "SSTableBuilder.h"

namespace lsm {


	SSTableBuilder::SSTableBuilder(const std::string& filepath, BloomFilter& filter) : added(0), current_offset(0), filter(filter) {
		file = fopen(filepath.c_str(), "wb");

		if (file == NULL) throw std::runtime_error("Unable to open " + filepath);
	}

	void SSTableBuilder::flush(const SkipList& memtable) {

		for (auto it = memtable.begin(); it != memtable.end(); ++it) {
			writeEntry(it->is_tombstone, it->key, it->val);
		}

		writeIndex();

	}

	void SSTableBuilder::writeKeyOrVal(const std::string_view s) {
		uint32_t length = s.size();
		fwrite(reinterpret_cast<const char*>(&length), 1, sizeof(uint32_t), file);

		fwrite(s.data(), 1, length, file);
	}

	void SSTableBuilder::writeEntry(bool is_tombstone, const std::string_view key, const std::string_view val) {
		if (added == 0) {
			index.emplace_back(std::string(key), current_offset);
		}

		uint8_t tombstone = is_tombstone;
		fwrite(reinterpret_cast<const char*>(&tombstone), 1, sizeof(uint8_t), file);

		writeKeyOrVal(key);
		current_offset += sizeof(uint8_t) + sizeof(uint32_t) + key.size();

		if (!is_tombstone){
			writeKeyOrVal(val);
			current_offset += sizeof(uint32_t) + val.size();	
		} 

		filter.add(key);

		++added;
		if (added == INDEX_ENTRY_SIZE) {added = 0;}
	}

	void SSTableBuilder::writeIndex() {
		uint64_t index_offset = current_offset;

		for (auto& [key, offset] : index) {

			writeKeyOrVal(key);

			fwrite(reinterpret_cast<const char*> (&offset), 1, sizeof(uint64_t), file);

			current_offset += sizeof(uint32_t) + key.size() + sizeof(uint64_t);
		}

		const uint64_t* data = filter.getData();
		const uint64_t num_elems = filter.getElems();
		fwrite(data, sizeof(uint64_t), num_elems, file);

		uint64_t filter_offset = current_offset;

		fwrite(reinterpret_cast<const char*> (&filter_offset), 1, sizeof(uint64_t), file);
		fwrite(reinterpret_cast<const char*> (&index_offset), 1, sizeof(uint64_t), file);


		fflush(file);

		fcntl(fileno(file), F_FULLFSYNC);

		fsync(fileno(file));

		fclose(file);
	}

}