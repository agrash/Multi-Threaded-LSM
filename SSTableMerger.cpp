#include "SSTableMerger.h"

namespace lsm {


	extern bool decode(std::ifstream& infile, bool& is_tombstone, std::string& key, std::string& val);

	SSTableIterator::SSTableIterator(const std::string& filepath) : pos(0) {
		file.open(filepath, std::ios::in | std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Unable to open file " + filepath + " in SSTableIterator");
		}

		file.seekg(-(int)sizeof(uint64_t), std::ios::end);

		file.read(reinterpret_cast<char*> (&index_start), sizeof(uint64_t));

		file.seekg(0);
	}

	SSTableIterator::~SSTableIterator() {
		file.close();
	}

	std::optional<dataContainer> SSTableIterator::getNext() {
		if (pos == index_start) {return std::nullopt;}

		dataContainer container;

		decode(file, container.is_tombstone, container.key, container.val);

		pos += sizeof(uint8_t) + sizeof(uint32_t) + container.key.size();
		if (!container.is_tombstone) {
			pos += sizeof(uint32_t) + container.val.size();
		}

		return container;
	}

	// Assumes that file are passed in chronological order (oldest to youngest).
	SSTableMerger::SSTableMerger(bool remove_tombstones, const std::vector<std::string>& filepaths, const std::string& merged_file_path, BloomFilter& filter) : remove_tombstones(remove_tombstones), merged_file(merged_file_path, filter) {

		for (size_t i=0; i<filepaths.size(); ++i) {
			iterators.emplace_back(std::make_unique<SSTableIterator>(filepaths[i]));
		}

		merge();
	}

	void SSTableMerger::merge() {

		std::vector<std::optional<dataContainer>> file_data(iterators.size(), std::nullopt);

		bool all_null;
		do {
			all_null = true;
			int min_index = -1;

			for (size_t i=0; i<iterators.size(); ++i) {
				if (!file_data[i]) {
					file_data[i] = iterators[i]->getNext();
				}

				if (file_data[i]) {
					if (all_null) {
						all_null = false;
						min_index = i;
					}
					else {
						auto cmp = (*file_data[min_index]).key <=> (*file_data[i]).key;

						if (cmp == 0) {
							file_data[min_index] = std::nullopt;
						}

						if (cmp >= 0) {
							min_index = i;
						}
					}
				}
			}

			if (!all_null) {
				writeEntry(*file_data[min_index]);
				file_data[min_index] = std::nullopt;
			}

		} while (!all_null);


		writeIndex();
	}

	void SSTableMerger::writeEntry(dataContainer& data) {
		if (remove_tombstones && data.is_tombstone) {return;}
		merged_file.writeEntry(data.is_tombstone, data.key, data.val);
	}

	void SSTableMerger::writeIndex() {
		merged_file.writeIndex();
	}

}