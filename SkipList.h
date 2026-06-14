#pragma once
#include "helper.h"
#include <random>
#include <string> 
#include <string_view>

namespace lsm {

	class SkipList {
	private:

		std::vector<char> arena;
		std::vector<std::string> val_data;

		static constexpr uint32_t INVALID_INDEX = -1;

		const uint8_t MAX_LVL = 20;
		const double PROBABILITY = 0.5;
		std::mt19937 gen;
		std::geometric_distribution<int> dist;

		size_t byte_counter;

		std::vector<uint32_t> getPredecessors(const std::string& key) const;
		uint8_t calcNodeLevel();

		uint32_t constructNode(uint8_t tombstone, const std::string& key, const std::string& val, uint8_t levels);
		void setNext(const uint32_t node, const uint8_t level, const uint32_t next);
		uint32_t getNext(const uint32_t node, const uint8_t level) const;

		std::string_view getKey(const uint32_t node) const;
		std::optional<std::string_view> getVal(const uint32_t node) const;

		void addVal(const uint32_t node, const std::string& val);
		void updateVal(const uint32_t node, uint8_t tombstone, const std::string& val);


	public:
		SkipList();
		~SkipList();

		struct Node {
			bool is_tombstone;
			std::string_view key;
			std::string_view val;
		};

		struct iterator {
		private:
			uint32_t ptr;
			Node data;
			const SkipList* master;

		public:
			iterator(uint32_t ptr, const SkipList* master);

			std::string_view& operator*();
	        Node* operator->();

			iterator& operator++();
			bool operator==(const iterator& other) const;
			bool operator!=(const iterator& other) const;

		};

		iterator begin() const;
		iterator end() const;

		iterator search(const std::string& key) const;
		void insert(bool insert_tombstone, const std::string& key, const std::string& val);

		size_t getSizeBytes() const;

		void clear();

		Node getData(const uint32_t node) const;
	};

}