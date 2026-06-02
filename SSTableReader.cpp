#include "SSTableReader.h"

namespace lsm {


	SSTableReader::~SSTableReader() {
		file.close();

		std::filesystem::remove(filepath);
	}

	SSTableReader::SSTableReader(const std::string& filepath) : filepath(filepath) {
		file.open(filepath.c_str(), std::ios::in | std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Unable to open file " + filepath);
		}

		file.seekg(-(int)sizeof(uint64_t), std::ios::end);

		std::streampos offset_start = file.tellg();

		file.read(reinterpret_cast<char*> (&index_offset), sizeof(uint64_t));

		file.seekg(index_offset);
		index_start = file.tellg();

		std::streampos curr = index_start;

		while (curr < offset_start) {
			uint32_t key_length;
			file.read(reinterpret_cast<char*> (&key_length), sizeof(uint32_t));

			std::string key(key_length, '\0');
			file.read(key.data(), key_length);

			uint64_t offset;
			file.read(reinterpret_cast<char*> (&offset), sizeof(uint64_t));

			curr += sizeof(uint32_t) + key_length + sizeof(uint64_t);
			index.emplace_back(std::move(key), offset);
		}
	}

	extern bool decode(std::ifstream& infile, bool& is_tombstone, std::string& key, std::string& val);

	std::optional<std::string> SSTableReader::findKey(const std::string& key) {

		Bookmark dummy(key, 0);
		int idx = upper_bound(index.begin(), index.end(), dummy) - index.begin() - 1;
		if (idx < 0) {
			return "";
		}

		file.seekg(index[idx].offset);

		std::streampos curr = file.tellg();
		std::streampos end  = (idx != index.size() - 1) ? static_cast<std::streampos>(index[idx + 1].offset) : index_start;

		bool is_tombstone;
		std::string saved_key, saved_val;
		while (curr < end) {	

			decode(file, is_tombstone, saved_key, saved_val);

			auto cmp_result = saved_key <=> key;
			if (cmp_result == 0) {
				if (is_tombstone) {return "";}
				else {return saved_val;}
			}
			else if (cmp_result > 0) {
				return std::nullopt;
			}

			curr += sizeof(uint8_t) + sizeof(uint32_t) + saved_key.size();
			if (!is_tombstone) {
				curr += sizeof(uint32_t) + saved_val.size();
			}
		}

		return std::nullopt;
	}

	std::string SSTableReader::getFilePath() {
		return filepath;
	}

}