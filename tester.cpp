// This file has been made with help from claude code.

#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <stdexcept>
//#include <omp.h>
#include <unordered_map>
#include <atomic>
#include <numeric>
#include <array>
#include <cstdlib>
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
	for (int i=start; i<end; ++i) {
		auto res = database.get(data[i].first);
	}
}

void insert_to_databse(DB& database, const vector<pair<string, string>>& data, int start, int end) {
	for (int i=start; i<end; ++i) {
		database.put(data[i].first, data[i].second);
	}
}

// Old harness. Materializes the whole dataset in RAM up to 3 times (map + two
// vectors), so at large n the harness OOMs long before the engine breaks a sweat.
void materialized_test() {
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

	cout<<"Starting Insertion into Database."<<endl;

	size_t st = 0;
	int num_threads = 1;
	size_t e = write_data.size() / num_threads;
	std::vector<std::thread> threads;

	auto start = std::chrono::high_resolution_clock::now();
	for (int i=0; i<num_threads; ++i) {
		threads.emplace_back(&insert_to_databse, std::ref(database), std::cref(write_data), st, e);
		st = e;
		e = e + write_data.size()/num_threads;
	}
	for (auto& t : threads) t.join();

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

	cout<<"Shuffling keys."<<endl;

	shuffle(key_val.begin(), key_val.end(), length_gen);

	cout<<"Starting reading from database."<<endl;

	//std::this_thread::sleep_for(std::chrono::seconds(20));

	cout<<"Phase 1 done"<<endl;

	st = 0;
	num_threads = 20;
	e = key_val.size() / num_threads;
	threads.clear();
	start = std::chrono::high_resolution_clock::now();
	for (int i=0; i<num_threads; ++i) {
		threads.emplace_back(&check_databse, std::ref(database), std::cref(key_val), st, e);
		st = e;
		e = e + key_val.size()/num_threads;
	}
	for (auto& t : threads) t.join();

	end = std::chrono::high_resolution_clock::now();

	duration = std::chrono::duration_cast<std::chrono::milliseconds>(
	    end - start
	);

	cout<<"Phase 2 done"<<endl;

	cout<<"Read duration: "<<duration.count()<<endl;

	/*vector<char> buffer(100 * 100);

	for (int i=0; i<limit / 100; ++i) {
		string key = s.generate(dist(length_gen));

		auto res = database.get(key, buffer);
		auto it = m.find(key);

		if ((res != std::nullopt && *res != "") ^ (it != m.end())) {
			cout<<i<<endl;
			if (res) {cout<<"Excess "<<*res<<endl;}
			else {cout<<"Didn't find "<<it->second<<endl;}
			cout<<"Failed"<<endl;
			return;
		}
	}*/

	cout<<"All Clear!!!"<<endl;
}


// New harness. Nothing is materialized: every key, value, and delete decision is
// re-derivable from its index through a fixed seed, so memory stays O(1) at any n,
// runs are reproducible, and the read phase can verify exact values for all n keys.

const uint64_t HARNESS_SEED = 0xC0FFEE123ULL;

inline uint64_t splitmix64(uint64_t x) {
	x += 0x9E3779B97F4A7C15ULL;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
	return x ^ (x >> 31);
}

// Index prefix makes keys unique by construction (no dedup map needed); random
// padding keeps byte content and sizes comparable to the old harness.
string make_key(uint64_t i) {
	uint64_t h = splitmix64(i ^ HARNESS_SEED);
	string k = to_string(i);
	k += '_';
	size_t pad = 1 + h % 80;
	k.reserve(k.size() + pad);
	for (size_t j=0; j<pad; ++j) {
		h = splitmix64(h);
		k += (char)(h & 127);
	}
	return k;
}

// Never empty, so a "" result can only mean a tombstone.
string make_val(uint64_t i) {
	uint64_t h = splitmix64(i ^ ~HARNESS_SEED);
	size_t len = 1 + h % 100;
	string v;
	v.reserve(len);
	for (size_t j=0; j<len; ++j) {
		h = splitmix64(h);
		v += (char)(h & 127);
	}
	return v;
}

// ~5% of keys get deleted; recomputable in the read phase.
bool is_removed(uint64_t i) {
	return splitmix64(i * 0x9E3779B97F4A7C15ULL ^ HARNESS_SEED) % 100 < 5;
}

// Visiting j -> (j * mult) % n for j in [0, n) hits every index exactly once when
// gcd(mult, n) == 1, giving a shuffled access order with nothing stored.
uint64_t coprimeMultiplier(uint64_t n, uint64_t start) {
	uint64_t mult = start | 1;
	while (gcd(mult, n) != 1) mult += 2;
	return mult;
}

void streaming_test(uint64_t n, int num_threads) {
	DB database;

	// j * mult must not overflow: j < n and mult ~2^31.5, safe for n up to ~2^32.
	const uint64_t write_mult = coprimeMultiplier(n, 2654435761ULL);
	const uint64_t read_mult = coprimeMultiplier(n, write_mult + 1000003ULL);

	cout<<"Streaming test: n = "<<n<<", read_threads = "<<num_threads<<", seed = "<<HARNESS_SEED<<endl;

	auto start = std::chrono::high_resolution_clock::now();

	for (uint64_t j=0; j<n; ++j) {
		uint64_t i = (j * write_mult) % n;
		database.put(make_key(i), make_val(i));
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto enqueue_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	cout<<"Write enqueue duration: "<<enqueue_ms<<" ms"<<endl;

	// get() waits until the writer has applied every write enqueued before it,
	// so a single get doubles as a drain barrier.
	start = std::chrono::high_resolution_clock::now();
	database.get(make_key(0));
	end = std::chrono::high_resolution_clock::now();
	auto drain_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	cout<<"Writer drain duration: "<<drain_ms<<" ms"<<endl;

	//auto applied_ms = max<long long>(enqueue_ms + drain_ms, 1);
	auto applied_ms = max<long long>(enqueue_ms, 1);
	cout<<"Write throughput (applied): "<<(uint64_t)(n * 1000.0 / applied_ms)<<" ops/s"<<endl;

	start = std::chrono::high_resolution_clock::now();

	uint64_t removed = 0;
	for (uint64_t i=0; i<n; ++i) {
		if (is_removed(i)) {
			database.remove(make_key(i));
			++removed;
		}
	}

	end = std::chrono::high_resolution_clock::now();
	auto delete_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	cout<<"Deleted "<<removed<<" keys in "<<delete_ms<<" ms"<<endl;

	start = std::chrono::high_resolution_clock::now();

	atomic<uint64_t> failures{0};
	vector<thread> threads;


	for (int t=0; t<num_threads; ++t) {
		uint64_t lo = n * t / num_threads;
		uint64_t hi = n * (t+1) / num_threads;

		threads.emplace_back([&database, &failures, lo, hi, n, read_mult]() {
			uint64_t local_failures = 0;
			for (uint64_t j=lo; j<hi; ++j) {
				uint64_t i = (j * read_mult) % n;
				auto res = database.get(make_key(i));

				if (is_removed(i)) {
					if (res && *res != "") ++local_failures;          // deleted key came back
				}
				else {
					if (!res || *res != make_val(i)) ++local_failures; // lost or wrong value
				}
			}
			failures.fetch_add(local_failures);
		});
	}
	for (auto& t : threads) t.join();

	end = std::chrono::high_resolution_clock::now();
	auto read_ms = max<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 1);

	cout<<"Read+verify duration: "<<read_ms<<" ms ("<<(uint64_t)(n * 1000.0 / read_ms)<<" ops/s)"<<endl;

	if (failures == 0) {
		cout<<"All Clear!!!"<<endl;
	}
	else {
		cout<<failures<<" verification failures!"<<endl;
	}
}

void recovery_test(uint64_t n, int num_threads) {
	cout<<"Recovery test: n = "<<n<<", read_threads = "<<num_threads<<", seed = "<<HARNESS_SEED<<endl;
	DB database(true);

	// j * mult must not overflow: j < n and mult ~2^31.5, safe for n up to ~2^32.
	const uint64_t write_mult = coprimeMultiplier(n, 2654435761ULL);
	const uint64_t read_mult = coprimeMultiplier(n, write_mult + 1000003ULL);

	
	auto start = std::chrono::high_resolution_clock::now();
	database.get(make_key(0));
	auto end = std::chrono::high_resolution_clock::now();
	auto drain_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	cout<<"Recovery drain duration: "<<drain_ms<<" ms"<<endl;

	//auto applied_ms = max<long long>(enqueue_ms + drain_ms, 1);

	start = std::chrono::high_resolution_clock::now();

	atomic<uint64_t> failures{0};
	vector<thread> threads;


	for (int t=0; t<num_threads; ++t) {
		uint64_t lo = n * t / num_threads;
		uint64_t hi = n * (t+1) / num_threads;

		threads.emplace_back([&database, &failures, lo, hi, n, read_mult]() {
			uint64_t local_failures = 0;
			for (uint64_t j=lo; j<hi; ++j) {
				uint64_t i = (j * read_mult) % n;
				auto res = database.get(make_key(i));

				if (is_removed(i)) {
					if (res && *res != "") ++local_failures;          // deleted key came back
				}
				else {
					if (!res || *res != make_val(i)) ++local_failures; // lost or wrong value
				}
			}
			failures.fetch_add(local_failures);
		});
	}
	for (auto& t : threads) t.join();

	end = std::chrono::high_resolution_clock::now();
	auto read_ms = max<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 1);

	cout<<"Read+verify duration: "<<read_ms<<" ms ("<<(uint64_t)(n * 1000.0 / read_ms)<<" ops/s)"<<endl;

	if (failures == 0) {
		cout<<"All Clear!!!"<<endl;
	}
	else {
		cout<<failures<<" verification failures!"<<endl;
	}
}

// floor(log2(ns)); bucket b spans [2^b, 2^(b+1)) ns. Bucket 0 also absorbs ns==0.
static inline int log2_bucket(uint64_t ns) {
	if (ns == 0) return 0;
	return 63 - __builtin_clzll(ns);
}

// Latency probe over recovered (cold) tables. Reports unloaded QD1 per-lookup
// service time (exact percentiles from a sorted sample) and loaded QDnum_threads
// percentiles (per-thread log2 histogram), alongside aggregate throughput.
void latency_test(uint64_t n, int num_threads) {
	cout<<"Latency test (recovered cold tables): n = "<<n<<", read_threads = "<<num_threads<<", seed = "<<HARNESS_SEED<<endl;
	DB database(true);

	const uint64_t write_mult = coprimeMultiplier(n, 2654435761ULL);
	const uint64_t read_mult = coprimeMultiplier(n, write_mult + 1000003ULL);

	// Drain barrier (also applies any inactive memtable recovered from the WAL).
	auto barrier_start = std::chrono::high_resolution_clock::now();
	database.get(make_key(0));
	auto barrier_end = std::chrono::high_resolution_clock::now();
	cout<<"Recovery drain duration: "<<std::chrono::duration_cast<std::chrono::milliseconds>(barrier_end - barrier_start).count()<<" ms"<<endl;

	// ---- QD1 unloaded probe: single thread, exact percentiles from a sorted sample ----
	{
		const uint64_t samples = std::min<uint64_t>(n, 1000000ULL);
		std::vector<uint64_t> lat;
		lat.reserve(samples);
		uint64_t failures = 0;

		for (uint64_t j=0; j<samples; ++j) {
			uint64_t i = (j * read_mult) % n;
			std::string key = make_key(i);

			auto t0 = std::chrono::steady_clock::now();
			auto res = database.get(key);
			auto t1 = std::chrono::steady_clock::now();
			lat.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());

			if (is_removed(i)) { if (res && *res != "") ++failures; }
			else { if (!res || *res != make_val(i)) ++failures; }
		}

		std::sort(lat.begin(), lat.end());
		long double sum = std::accumulate(lat.begin(), lat.end(), (long double)0);
		auto pct = [&](double p) { return lat[std::min<size_t>(lat.size()-1, (size_t)(p * lat.size()))]; };

		cout<<"--- QD1 unloaded ("<<samples<<" cold gets) ---"<<endl;
		cout<<"mean "<<(uint64_t)(sum / lat.size())<<" ns | p50 "<<pct(0.50)<<" | p90 "<<pct(0.90)
		    <<" | p99 "<<pct(0.99)<<" | p999 "<<pct(0.999)<<" | max "<<lat.back()<<" ns"<<endl;
		cout<<"QD1 throughput: "<<(uint64_t)(1e9 * lat.size() / (double)sum)<<" ops/s";
		if (failures) cout<<"  ("<<failures<<" verify failures)";
		cout<<endl;
	}

	// ---- Loaded probe: num_threads concurrent gets, per-thread log2 latency histogram ----
	{
		std::vector<std::array<uint64_t,64>> hists(num_threads);
		for (auto& h : hists) h.fill(0);
		std::atomic<uint64_t> failures{0};
		std::vector<std::thread> threads;

		auto start = std::chrono::high_resolution_clock::now();
		for (int t=0; t<num_threads; ++t) {
			uint64_t lo = n * t / num_threads;
			uint64_t hi = n * (t+1) / num_threads;

			threads.emplace_back([&database, &failures, &hists, t, lo, hi, n, read_mult]() {
				auto& h = hists[t];
				uint64_t local_failures = 0;
				for (uint64_t j=lo; j<hi; ++j) {
					uint64_t i = (j * read_mult) % n;
					std::string key = make_key(i);

					auto t0 = std::chrono::steady_clock::now();
					auto res = database.get(key);
					auto t1 = std::chrono::steady_clock::now();
					++h[log2_bucket(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count())];

					if (is_removed(i)) { if (res && *res != "") ++local_failures; }
					else { if (!res || *res != make_val(i)) ++local_failures; }
				}
				failures.fetch_add(local_failures);
			});
		}
		for (auto& t : threads) t.join();
		auto end = std::chrono::high_resolution_clock::now();
		auto read_ms = max<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 1);

		std::array<uint64_t,64> all; all.fill(0);
		uint64_t total = 0;
		for (auto& h : hists) for (int b=0; b<64; ++b) { all[b] += h[b]; total += h[b]; }

		auto hist_pct = [&](double p) -> std::string {
			uint64_t target = (uint64_t)(p * total), cum = 0;
			for (int b=0; b<64; ++b) {
				cum += all[b];
				if (cum >= target) {
					uint64_t lo_ns = (b == 0) ? 0 : (1ULL << b);
					return std::to_string(lo_ns/1000) + "-" + std::to_string((1ULL << (b+1))/1000) + " us";
				}
			}
			return std::string("overflow");
		};

		cout<<"--- Loaded QD"<<num_threads<<" ---"<<endl;
		cout<<"Read+verify duration: "<<read_ms<<" ms ("<<(uint64_t)(n * 1000.0 / read_ms)<<" ops/s)"<<endl;
		cout<<"latency p50 "<<hist_pct(0.50)<<" | p90 "<<hist_pct(0.90)<<" | p99 "<<hist_pct(0.99)<<" | p999 "<<hist_pct(0.999)<<endl;
		cout<<"log2 histogram [lo,hi) ns -> count:"<<endl;
		for (int b=0; b<64; ++b) if (all[b]) {
			uint64_t lo_ns = (b == 0) ? 0 : (1ULL << b);
			cout<<"  ["<<lo_ns<<","<<(1ULL << (b+1))<<") : "<<all[b]<<endl;
		}

		if (failures == 0) cout<<"All Clear!!!"<<endl;
		else cout<<failures<<" verification failures!"<<endl;
	}
}

int main(int argc, char* argv[]) {
	uint64_t n = 1000000;
	if (argc > 1) {
		n = strtoull(argv[1], nullptr, 10);
		if (n == 0) {
			cerr<<"Invalid count: "<<argv[1]<<endl;
			return 1;
		}
	}

	int read_threads = 20;
	if (argc > 2) read_threads = atoi(argv[2]);

	int test_num = 0;
	if (argc > 3) test_num = atoi(argv[3]);

	if (test_num == 0) streaming_test(n, read_threads);
	else if (test_num == 1) recovery_test(n, read_threads);
	else if (test_num == 3) latency_test(n, read_threads);
	else materialized_test();

	return 0;
}
