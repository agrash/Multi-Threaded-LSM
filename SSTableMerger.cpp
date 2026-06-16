#include "SSTableMerger.h"

namespace lsm {

	// Assumes that file are passed in chronological order (oldest to youngest).
	SSTableMerger::SSTableMerger(bool remove_tombstones, std::vector<std::shared_ptr<SSTableReader>>& readers, const std::string& merged_file_path, BloomFilter& filter) : remove_tombstones(remove_tombstones), merged_file(merged_file_path, filter) {
		std::vector<std::optional<dataContainer>> file_data(readers.size(), std::nullopt);

		bool all_null;
		do {
			all_null = true;
			int min_index = -1;

			for (size_t i=0; i<readers.size(); ++i) {
				if (!file_data[i]) {
					file_data[i] = readers[i]->getNext();
				}

				if (file_data[i]) {
					if (all_null) {
						all_null = false;
						min_index = i;
					}
					else {
						auto cmp = file_data[min_index]->key <=> file_data[i]->key;

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