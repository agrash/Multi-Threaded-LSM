#include "SkipList.h"

namespace lsm {

	using iterator = SkipList::iterator;

	size_t SkipList::Node::getSizeBytes() const {
		size_t footprint = sizeof(Node) + key.size() + val.size() + (next.size() * sizeof(Node*));
		return footprint;
	};

	bool SkipList::Node::isTombstone() const {
		return is_tombstone;
	}
	//End Node functions.

	std::string& iterator::operator*() {
		return ptr->val;
	}

    SkipList::Node* iterator::operator->() {
        return ptr;
    }

	iterator& iterator::operator++() {
		if (!ptr) {throw std::domain_error("Accessing out of bounds memory.");}
		ptr = ptr->next[0];
		return *this;
	}

	bool iterator::operator==(const iterator& other) const{
		return ptr == other.ptr;
	}

	bool iterator::operator!=(const iterator& other) const {
        return !(*this == other);
    }
    //End iterator functions.

    SkipList::SkipList() : gen(std::random_device{}()), dist(PROBABILITY) {
    	std::string dummy_key = "";
    	std::string dummy_val = "";
		header = new Node(dummy_key, dummy_val, MAX_LVL);

		byte_counter = sizeof(SkipList) + getEntrySize(header);
	}

	SkipList::~SkipList() {
		Node* curr = header;
		while (curr) {
			Node* next = curr->next[0];
			delete curr;
			curr = next;
		}
		byte_counter = 0;
	}


	SkipList::Node* SkipList::getHeader() const {
		return header;
	}
 
	std::vector<SkipList::Node*> SkipList::getPredecessors(const std::string& key) const {
		Node* curr = header;
		int curr_level = MAX_LVL;

		std::vector<Node*> res(MAX_LVL + 1);

		while (curr_level >= 0) {
			if (curr->next[curr_level] && curr->next[curr_level]->key < key) {
				curr = curr->next[curr_level];
			}
			else {
				res[curr_level] = curr;
				--curr_level;
			}
		}

		return res;
	}


	size_t SkipList::calcNodeLevel() {
		int res = dist(gen);
		if (res < MAX_LVL) {return res;}
		return MAX_LVL;
	}


	iterator SkipList::begin() const {
		return iterator(header->next[0]);
	}

	iterator SkipList::end() const {
		return iterator(nullptr);
	}

	iterator SkipList::search(const std::string& key) const {
		Node* req = getPredecessors(key)[0]->next[0];
		if (req && key == req->key) {
			return iterator(req);
		}

		return end();
	}

	void SkipList::insert(bool insert_tombstone, std::string& key, std::string& val) {
		auto predecessors = getPredecessors(key);
		Node* next = predecessors[0]->next[0];

		if (next && key == next->key) {
			byte_counter -= getEntrySize(next);
			insertHelper(insert_tombstone, val, next);
			byte_counter += getEntrySize(next);
		}
		else {
			size_t max_level = calcNodeLevel();
			Node* to_add = new Node(key, val, max_level);

			if (insert_tombstone) to_add->val = "";

			byte_counter += getEntrySize(to_add);

			for (size_t i=0; i<=max_level; ++i) {
				to_add->next[i] = predecessors[i]->next[i];
				predecessors[i]->next[i] = to_add;
			}

		}
	}

	void SkipList::insertHelper(bool insert_tombstone, std::string& val, Node* curr) {
		if (insert_tombstone) {
			curr->val = "";
			curr->is_tombstone = true;
		}
		else {
			curr->val = std::move(val);
			curr->is_tombstone = false;
		}
	}

	size_t SkipList::getEntrySize(const Node* curr) const {
		return curr->getSizeBytes();
	}

	size_t SkipList::getSizeBytes() const {
		return byte_counter;
	}

	void SkipList::clear() {
		Node* next = header->next[0];
		while (next) {
			Node* curr = next;
			next = next->next[0];
			delete curr;
		}

		for (auto& node : header->next) {
			node = nullptr;
		}

		byte_counter = 0;
	}

	//End SkipList functions.

}