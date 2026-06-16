#pragma once
#include "helper.h"
#include "SSTableBuilder.h"
#include "SSTableReader.h"

#include <memory>

namespace lsm {

	class SSTableMerger {
	private:
		bool remove_tombstones;

		SSTableBuilder merged_file;

		void writeEntry(dataContainer& data);
		void writeIndex();

	public:
		SSTableMerger(bool remove_tombstones, std::vector<std::shared_ptr<SSTableReader>>& readers, const std::string& merged_file_path, BloomFilter& filter);
	};

}