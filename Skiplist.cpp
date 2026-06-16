#include "SkipList.h"

namespace lsm {

	using iterator = SkipList::iterator;

	uint32_t SkipList::constructNode(uint8_t tombstone, const std::string& key, const std::string& val, uint8_t levels) {
		uint32_t node_size = sizeof(uint8_t) + (levels + 1) * sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + key.size() + sizeof(uint32_t); // num_levels + next_node data + tombstone + key_length + key_size + val index.

		uint32_t starting = arena.size();
		arena.resize(starting + node_size);

		uint32_t offset = starting;
		memcpy(arena.data() + offset, reinterpret_cast<const char*> (&levels), sizeof(uint8_t)); // num_levels inserted.
		offset += sizeof(uint8_t);
		memset(arena.data() + offset, 0, (levels + 1) * sizeof(uint32_t)); //next data of node.
		offset += (levels + 1) * sizeof(uint32_t);
		memcpy(arena.data() + offset, reinterpret_cast<const char*> (&tombstone), sizeof(uint8_t)); //tombstone
		offset += sizeof(uint8_t);
		uint32_t key_length = key.size();
		memcpy(arena.data() + offset, reinterpret_cast<const char*> (&key_length), sizeof(uint32_t)); //key_length.
		offset += sizeof(uint32_t);
		memcpy(arena.data() + offset, key.data(), key.size()); // key_data.
		offset += key.size();

		if (tombstone == 0) addVal(starting, val);
		else memcpy(arena.data() + offset, reinterpret_cast<const char*>(&INVALID_INDEX), sizeof(uint32_t));

		byte_counter += key.size() + val.size();

		return starting;

	}

	void SkipList::addVal(const uint32_t node, const std::string& val) {
		uint8_t levels;
		memcpy(&levels, arena.data() + node, sizeof(uint8_t));

		uint32_t offset = node + sizeof(uint8_t) + (levels + 1) * sizeof(uint32_t) + sizeof(uint8_t);

		uint32_t key_length;
		memcpy(&key_length, arena.data() + offset, sizeof(uint32_t));
		offset += sizeof(uint32_t) + key_length;

		uint32_t val_data_size = val_data.size();
		memcpy(arena.data() + offset, reinterpret_cast<const char*> (&val_data_size), sizeof(uint32_t));
		val_data.push_back(std::move(val));
	}

	void SkipList::updateVal(const uint32_t node, uint8_t tombstone, const std::string& val) {
		uint32_t offset = node;

		uint8_t levels;
		memcpy(&levels, arena.data() + offset, sizeof(uint8_t));
		offset += sizeof(uint8_t) + (levels + 1) * sizeof(uint32_t);

		memcpy(arena.data() + offset, reinterpret_cast<const char*>(&tombstone), sizeof(uint8_t));
		offset += sizeof(uint8_t);

		uint32_t key_length;
		memcpy(&key_length, arena.data() + offset, sizeof(uint32_t));
		offset += sizeof(uint32_t) + key_length;

		uint32_t val_index;
		memcpy(&val_index, arena.data() + offset, sizeof(uint32_t));

		if (val_index != INVALID_INDEX) byte_counter -= val_data[val_index].size();
		if (tombstone == 1) return;

		if (val_index == INVALID_INDEX) addVal(node, val);
		else val_data[val_index] = std::move(val);

		byte_counter += val.size();
	}

	void SkipList::setNext(const uint32_t node, const uint8_t level, const uint32_t next) {
		uint32_t offset = node + sizeof(uint8_t) + sizeof(uint32_t) * level;
		memcpy(arena.data() + offset, reinterpret_cast<const char*> (&next), sizeof(uint32_t));
	}

	uint32_t SkipList::getNext(const uint32_t node, const uint8_t level) const {
		uint32_t offset = node + sizeof(uint8_t) + sizeof(uint32_t) * level;

		uint32_t res;
		memcpy(&res, arena.data() + offset, sizeof(uint32_t));

		return res;
	}

	std::string_view SkipList::getKey(const uint32_t node) const {
		uint8_t levels;
		memcpy(&levels, arena.data() + node, sizeof(uint8_t));
		uint32_t offset = node + sizeof(uint8_t) + (levels + 1) * sizeof(uint32_t) + sizeof(uint8_t);

		uint32_t key_length;
		memcpy(&key_length, arena.data() + offset, sizeof(uint32_t));
		offset += sizeof(uint32_t);

		return std::string_view(arena.data() + offset, key_length);
	}

	std::optional<std::string_view> SkipList::getVal(const uint32_t node) const {
		uint8_t levels;
		memcpy(&levels, arena.data() + node, sizeof(uint8_t));
		uint32_t offset = node + sizeof(uint8_t) + (levels + 1) * sizeof(uint32_t);

		uint8_t tombstone;
		memcpy(&tombstone, arena.data() + offset, sizeof(uint8_t));
		if (tombstone == 1) return std::nullopt;

		offset += sizeof(uint8_t);

		uint32_t key_length;
		memcpy(&key_length, arena.data() + offset, sizeof(uint32_t));
		offset += sizeof(uint32_t) + key_length;

		uint32_t val_index;
		memcpy(&val_index, arena.data() + offset, sizeof(uint32_t));

		if (val_index == INVALID_INDEX) return std::nullopt;
		return std::string_view(val_data[val_index]);
	}


	SkipList::Node SkipList::getData(const uint32_t node) const {
		uint8_t levels;
		memcpy(&levels, arena.data() + node, sizeof(uint8_t));
		uint32_t offset = node + sizeof(uint8_t) + (levels + 1) * sizeof(uint32_t);

		uint8_t tombstone;
		memcpy(&tombstone, arena.data() + offset, sizeof(uint8_t));
		offset += sizeof(uint8_t);

		Node res;
		res.is_tombstone = (tombstone == 1);

		uint32_t key_length;
		memcpy(&key_length, arena.data() + offset, sizeof(uint32_t));
		offset += sizeof(uint32_t);

		res.key = std::string_view(arena.data() + offset, key_length);
		offset += key_length;

		res.val = "";

		if (tombstone == 0) {
			uint32_t val_index;
			memcpy(&val_index, arena.data() + offset, sizeof(uint32_t));
			offset += sizeof(uint32_t);

			res.val = std::string_view(val_data[val_index]);
		}

		return res;
	}

	iterator::iterator(uint32_t ptr, const SkipList* master) : ptr(ptr), master(master) {
		data = master->getData(ptr);
	}

	std::string_view& iterator::operator*() {
		return data.val;
	}

    SkipList::Node* iterator::operator->() {
        return &data;
    }

	iterator& iterator::operator++() {
		if (ptr == 0) {throw std::domain_error("Accessing out of bounds memory.");}
		ptr = master->getNext(ptr, 0);
		data = master->getData(ptr);
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
    	arena.reserve(50 * 20000 * 16);
    	val_data.reserve(20000 * 16);
		constructNode(true, "", "", MAX_LVL);
		byte_counter = 0;
	}

	SkipList::~SkipList() {
		byte_counter = 0;
	}

 
	std::vector<uint32_t> SkipList::getPredecessors(const std::string& key) const {
		uint32_t curr = 0;
		int curr_level = MAX_LVL;

		std::vector<uint32_t> res(MAX_LVL + 1);

		while (curr_level >= 0) {
			uint32_t next = getNext(curr, curr_level);
			if (next != 0 && getKey(next) < key) {
				curr = next;
			}
			else {
				res[curr_level] = curr;
				--curr_level;
			}
		}

		return res;
	}


	uint8_t SkipList::calcNodeLevel() {
		int res = dist(gen);
		if (res < MAX_LVL) {return res;}
		return MAX_LVL;
	}


	iterator SkipList::begin() const {
		return iterator(getNext(0, 0), this);
	}

	iterator SkipList::end() const {
		return iterator(0, this);
	}

	iterator SkipList::search(const std::string& key) const {
		auto predecessors = getPredecessors(key);
		uint32_t req = getNext(predecessors[0], 0);
		if (req != 0 && key == getKey(req)) {
			return iterator(req, this);
		}

		return end();
	}

	void SkipList::insert(bool insert_tombstone, const std::string& key, const std::string& val) {
		auto predecessors = getPredecessors(key);
		uint32_t next = getNext(predecessors[0], 0);

		if (key == getKey(next)) {
			updateVal(next, (insert_tombstone ? 1 : 0), val);
			return;
		}

		uint32_t max_level = calcNodeLevel();
		uint32_t to_add = constructNode(insert_tombstone ? 1 : 0, key, val, max_level);

		for (size_t i=0; i<=max_level; ++i) {
			setNext(to_add, i, getNext(predecessors[i], i));
			setNext(predecessors[i], i, to_add);
		}

	}


	size_t SkipList::getSizeBytes() const {
		return byte_counter;
	}

	void SkipList::clear() {
		arena.clear();
		constructNode(true, "", "", MAX_LVL);
		val_data.clear();

		byte_counter = 0;
	}


}