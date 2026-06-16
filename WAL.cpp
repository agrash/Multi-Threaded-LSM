#include "WAL.h"

namespace lsm {

	WAL::WAL(const std::string& dir) : dir(dir) {
		file = NULL;
	}

	WAL::~WAL() {
		flush();
		fclose(file);
	}

	void WAL::open(const uint64_t log_number) {
		filepath = dir + "/wal_" + std::to_string(log_number) + ".log";
		file = fopen(filepath.c_str(), "wb");

		if (file == NULL) throw std::runtime_error("Unable to open file " + filepath);
	}

	void WAL::writeKeyOrVal(const std::string_view s) {
		uint32_t length = s.size();
		fwrite(reinterpret_cast<const char*>(&length), 1, sizeof(uint32_t), file);

		fwrite(s.data(), 1, length, file);
	}

	void WAL::append(bool is_tombstone, const std::string& key, const std::string& val) {
		uint8_t tombstone = is_tombstone;
		fwrite(reinterpret_cast<const char*>(&tombstone), 1, sizeof(uint8_t), file);

		writeKeyOrVal(key);

		if (!is_tombstone) writeKeyOrVal(val);
	}

	void WAL::flush() {
		if (file != NULL) {
			fcntl(fileno(file), F_FULLFSYNC);
			fflush(file);
			fsync(fileno(file));
		}
	}

	extern bool decode(std::ifstream& infile, bool& is_tombstone, std::string& key, std::string& val);

	void WAL::recover(SkipList& memtable, const std::string& filepath) {
		std::ifstream infile(filepath, std::ios::in | std::ios::binary);
		if (!infile.is_open()) {return;}


		bool is_tombstone;
		std::string key, val;

		std::string dummy = "";

		while (decode(infile, is_tombstone, key, val)) {
			if (is_tombstone) {memtable.insert(true, key, dummy);}
			else {memtable.insert(false, key, val);}
		}

		infile.close();
	}

	void WAL::clear() {
		if (file != NULL) {
			fclose(file);
			std::filesystem::remove(filepath);
		}
	}

}