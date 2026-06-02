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

			Node(const std::string& key, const std::string& val, size_t levels) : is_tombstone(false), key(key), val(val), next(levels + 1, nullptr) {}
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

		void insertHelper(bool insert_tombstone, const std::string& val, Node* curr);

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
		void insert(bool insert_tombstone, const std::string& key, const std::string& val);

		size_t getSizeBytes() const;

		void clear();
	};

}