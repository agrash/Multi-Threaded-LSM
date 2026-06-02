#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <ctime>
#include <algorithm>
#include<stdexcept>
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

int main() {
	DB database;

	const int limit = 1000000;

	mt19937 length_gen(random_device{}());
	uniform_int_distribution<int> dist(1, 100);

	unordered_map<string, string> m;

	stringGenerator s;


	auto start = std::chrono::high_resolution_clock::now();
	for (int i=0; i<limit; ++i) {

		if (i % 10000 == 0) {cout<<i<<endl;}

		string key = s.generate(dist(length_gen));
		string val = s.generate(dist(length_gen));

		database.put(key, val);

		m[key] = val;
	}
	auto end = std::chrono::high_resolution_clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
	    end - start
	);
	cout<<"Writing duration: "<<duration.count()<<endl;

	for (auto it = m.begin(); it != m.end(); ) {
		bool d = dist(length_gen) <= 5;

		if (d) {
			database.remove(it->first);
			it = m.erase(it);
		}
		else {
			++it;
		}
	}

	vector<pair<string, string>> key_val;
	for (auto& [key, val] : m) {
		key_val.emplace_back(key, val);
	}

	shuffle(key_val.begin(), key_val.end(), length_gen);

	cout<<"Phase 1 done"<<endl;
	int counter = 0;

	start = std::chrono::high_resolution_clock::now();
	for (auto& [key, val] : key_val) {
		auto res = database.get(key);
	}

	end = std::chrono::high_resolution_clock::now();

	duration = std::chrono::duration_cast<std::chrono::milliseconds>(
	    end - start
	);

	cout<<"Phase 2 done"<<endl;

	cout<<"Read duration: "<<duration.count()<<endl;

	for (int i=0; i<limit / 100; ++i) {
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
	}

	cout<<"All Clear!!!"<<endl;

	return 0;
}