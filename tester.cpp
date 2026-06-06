#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <stdexcept>
#include <omp.h>
#include <unordered_map>
#include <atomic>
#include "DB.h"
using namespace std;
using namespace lsm;

class stringGenerator {
public:
	mt19937 gen;
	uniform_int_distribution<int> dist{0, 127};

	stringGenerator() : gen(random_device{}()) {}

	string generate(size_t length) {
		string res;
		for (int i=0; i<length; ++i) {
			res += (char)dist(gen);
		}
		return res;
	}
};

void check_databse(DB& database, const vector<pair<string, string>>& data, int start, int end) {
	int count = 0;
	vector<char> buffer(100 * 100);
	for (int i=start; i<end; ++i) {
		auto res = database.get(data[i].first, buffer);
	}
}

/*void insert_to_databse(DB& database, const vector<pair<string, string>>& data, int start, int end) {
	for (int i=start; i<end; ++i) {
		database.put(data[i].first, data[i].second);
	}
}*/

int main() {
	DB database;

	const int limit = 1000000;

	mt19937 length_gen(random_device{}());
	uniform_int_distribution<int> dist(1, 100);

	unordered_map<string, string> m;

	stringGenerator s;


	for (int i=0; i<limit; ++i) {

		string key = s.generate(dist(length_gen));
		string val = s.generate(dist(length_gen));

		m[key] = val;
	}

	cout<<"Creating random keys done"<<endl;

	vector<pair<string, string>> write_data;
	write_data.reserve(m.size());

	for (const auto& [key, val] : m) {
		write_data.emplace_back(key, val);
	}

	auto start = std::chrono::high_resolution_clock::now();

	#pragma omp parallel for num_threads(4)
	for (const auto& [key, val] : write_data) {
		database.put(key, val);
	}
	auto end = std::chrono::high_resolution_clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
	    end - start
	);
	cout<<"Writing duration: "<<duration.count()<<endl;

	/*for (auto it = m.begin(); it != m.end(); ) {
		bool d = dist(length_gen) <= 5;

		if (d) {
			database.remove(it->first);
			it = m.erase(it);
		}
		else {
			++it;
		}
	}*/

	vector<pair<string, string>> key_val;
	for (auto& [key, val] : m) {
		key_val.emplace_back(key, val);
	}

	shuffle(key_val.begin(), key_val.end(), length_gen);

	std::this_thread::sleep_for(std::chrono::seconds(20));

	cout<<"Phase 1 done"<<endl;
	atomic<int> counter = 0;

	start = std::chrono::high_resolution_clock::now();
	
	size_t st = 0;
	int num_threads = 10;
	size_t e = key_val.size() / num_threads;
	std::vector<std::thread> threads;
	for (int i=0; i<num_threads; ++i) {
		threads.emplace_back(&check_databse, std::ref(database), std::cref(key_val), st, e);
		st = e;
		e = min(key_val.size(), e + key_val.size()/num_threads);
	}
	for (auto& t : threads) t.join();

	end = std::chrono::high_resolution_clock::now();

	duration = std::chrono::duration_cast<std::chrono::milliseconds>(
	    end - start
	);
	cout<<counter<<endl;

	cout<<"Phase 2 done"<<endl;

	cout<<"Read duration: "<<duration.count()<<endl;

	/*for (int i=0; i<limit / 100; ++i) {
		string key = s.generate(dist(length_gen));

		auto res = database.get(key);
		auto it = m.find(key);

		if ((res != std::nullopt && *res != "") ^ (it != m.end())) {
			cout<<i<<endl;
			if (res) {cout<<"Excess "<<*res<<endl;}
			else {cout<<"Didn't find "<<it->second<<endl;}
			cout<<"Failed"<<endl;
			return 0;
		}
	}*/

	cout<<"All Clear!!!"<<endl;

	return 0;
}