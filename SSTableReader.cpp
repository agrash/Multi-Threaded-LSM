#include "SSTableReader.h"

namespace lsm {


	SSTableReader::~SSTableReader() {
		close(fd);

		std::filesystem::remove(filepath);
	}

	SSTableReader::SSTableReader(const std::string& filepath) : filepath(filepath) {
		fd = open(filepath.c_str(), O_RDONLY);
		if (fd == -1) {
			throw std::runtime_error("Unable to open file " + filepath);
		}

		struct stat file_stat;
		if (fstat(fd, &file_stat) == -1) {
			throw std::runtime_error("Unable to get file stats" + filepath);
		}

		uint64_t offset_start = file_stat.st_size - sizeof(uint64_t);
		pread(fd, reinterpret_cast<char*> (&index_offset), sizeof(uint64_t), offset_start);

		uint64_t index_size = offset_start - index_offset;

		std::vector<char> buffer(index_size);
		pread(fd, buffer.data(), index_size, index_offset);

		uint64_t curr = 0;

		while (curr < index_size) {
			uint32_t key_length;
			memcpy(&key_length, buffer.data() + curr, sizeof(uint32_t));

			curr += sizeof(uint32_t);

			std::string key(key_length, '\0');
			memcpy(key.data(), buffer.data() + curr, key_length);
			curr += key_length;

			uint64_t offset;
			memcpy(&offset, buffer.data() + curr, sizeof(uint64_t));
			curr += sizeof(uint64_t);

			index.emplace_back(std::move(key), offset);
		}

		fsync(fd);
	}

	extern uint64_t decode(char* data, uint64_t offset, bool& is_tombstone, std::string& key, std::string& val);

	std::optional<std::string> SSTableReader::findKey(const std::string& key, std::vector<char>& buffer) {

		Bookmark dummy(key, 0);
		int idx = upper_bound(index.begin(), index.end(), dummy) - index.begin() - 1;
		if (idx < 0) {
			return std::nullopt;
		}


		uint64_t start = index[idx].offset;
		uint64_t end  = (idx != index.size() - 1) ? index[idx + 1].offset : index_offset;

		uint64_t block_size = end - start;

		if (buffer.size() < block_size) buffer.resize(block_size);

		pread(fd, buffer.data(), block_size, start);

		uint64_t curr = 0;

		bool is_tombstone;
		std::string saved_key, saved_val;
		while (curr < block_size) {	

			curr += decode(buffer.data(), curr, is_tombstone, saved_key, saved_val);

			auto cmp_result = saved_key <=> key;
			if (cmp_result == 0) {
				//std::cout<<"Key Found"<<std::endl;
				if (is_tombstone) {return "";}
				else {return saved_val;}
			}
			else if (cmp_result > 0) {
				return std::nullopt;
			}
		}

		return std::nullopt;
	}

	std::string SSTableReader::getFilePath() {
		return filepath;
	}

}