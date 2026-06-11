#pragma once
#include "helper.h"
#include <random>

namespace lsm {

	class SkipList {
	private:
		struct Node
		{
			bool is_tombstone;
			std::string key;
			std::string val;

			std::vector<Node*> next;

			// have to change these references to value, and the caller would decide whether to pass them as r-value if they no longer want to keep the data associated with it.
			Node(std::string& key, std::string& val, size_t levels) : is_tombstone(false), key(std::move(key)), val(std::move(val)), next(levels + 1, nullptr) {}
			size_t getSizeBytes() const;
			bool isTombstone() const;
		};

		const size_t MAX_LVL = 20;
		const double PROBABILITY = 0.5;
		std::mt19937 gen;
		std::geometric_distribution<int> dist;

		size_t byte_counter;

		Node* header;

		Node* getHeader() const;
		std::vector<Node*> getPredecessors(const std::string& key) const;
		size_t calcNodeLevel();

		size_t getEntrySize(const Node* curr) const;

		void insertHelper(bool insert_tombstone, std::string& val, Node* curr);

	public:
		SkipList();
		~SkipList();

		class iterator {
		public:
			Node* ptr;

			iterator(Node* ptr) : ptr(ptr) {}

			std::string& operator*();
	        Node* operator->();

			iterator& operator++();
			bool operator==(const iterator& other) const;
			bool operator!=(const iterator& other) const;

		};

		iterator begin() const;
		iterator end() const;

		iterator search(const std::string& key) const;
		// same change needed as Node constructor.
		void insert(bool insert_tombstone, std::string& key, std::string& val);

		size_t getSizeBytes() const;

		void clear();
	};

}