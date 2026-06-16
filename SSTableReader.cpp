#include "SSTableReader.h"

namespace lsm {


	void SSTableReader::preserveTable() {
		delete_file_at_destruction = false;
	}

	SSTableReader::~SSTableReader() {
		munmap(const_cast<char*>(file_data), file_size);

		close(fd);

		if (delete_file_at_destruction) std::filesystem::remove(filepath);
	}

	SSTableReader::SSTableReader(const std::string& filepath, BloomFilter& filter) : filepath(filepath), filter(std::move(filter)) {
		fd = open(filepath.c_str(), O_RDONLY);
		if (fd == -1) throw std::runtime_error("Unable to open file " + filepath);

		struct stat file_stat;
		if (fstat(fd, &file_stat) == -1) throw std::runtime_error("Unable to get file stats" + filepath);

		file_size = file_stat.st_size;

		void* base = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (base == MAP_FAILED) throw std::runtime_error("Unable to map file " + filepath);

		file_data = static_cast<const char*>(base);
		madvise(base, file_size, MADV_RANDOM);

		uint64_t index_offset_start = file_size - sizeof(uint64_t);
		memcpy(&index_offset, file_data + index_offset_start, sizeof(uint64_t));

		uint64_t filter_offset_start = index_offset_start - sizeof(uint64_t);
		uint64_t filter_offset;
		memcpy(&filter_offset, file_data + filter_offset_start, sizeof(uint64_t));

		uint64_t index_size = filter_offset - index_offset;

		std::string_view buffer(file_data + index_offset, index_size);

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
	}

	std::optional<std::string> SSTableReader::findKey(const std::string& key, uint32_t h1, uint32_t h2) {

		if (!filter.contains(h1, h2)) return std::nullopt;

		Bookmark dummy(key, 0);
		int idx = upper_bound(index.begin(), index.end(), dummy) - index.begin() - 1;
		if (idx < 0) {
			return std::nullopt;
		}


		uint64_t start = index[idx].offset;
		uint64_t end  = (idx != index.size() - 1) ? index[idx + 1].offset : index_offset;

		uint64_t block_size = end - start;

		std::string_view buffer(file_data + start, block_size);

		uint64_t curr = 0;

		uint8_t tombstone;
		uint32_t key_length, val_length;

		while (curr < block_size) {	

			memcpy(&tombstone, buffer.data() + curr, sizeof(uint8_t));
			curr += sizeof(uint8_t);

			memcpy(&key_length, buffer.data() + curr, sizeof(uint32_t));
			curr += sizeof(uint32_t);

			auto cmp_result = buffer.substr(curr, key_length) <=> key;
			curr += key_length;

			if (tombstone == 0) {
				memcpy(&val_length, buffer.data() + curr, sizeof(uint32_t));
				curr += sizeof(uint32_t);
			}

			if (cmp_result == 0) {
				if (tombstone == 1) return "";
				else return std::string{buffer.substr(curr, val_length)};
			}
			else if (cmp_result > 0) return std::nullopt;

			if (tombstone == 0) curr += val_length;
		}

		return std::nullopt;
	}

	std::string SSTableReader::getFilePath() {
		return filepath;
	}

}