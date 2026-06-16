#pragma once
#include <semaphore>
#include <vector>
#include <mutex>
#include <atomic>

template<typename T, int N>
class RingBuffer {
private:
	std::vector<T> buffer;
	int in;
	int out;

	std::atomic<bool> stop_buffer{false};
	std::atomic<uint64_t> sequence_number{1};
	const uint64_t max_elems = 1000;

	std::mutex producer_mutex;
	//std::mutex consumer_mutex;

	std::counting_semaphore<N> empty_slots{N};
	std::counting_semaphore<N> full_slots{0};

public:
	RingBuffer() : in(0), out(0), buffer(N) {}

	void produce(T& item, uint64_t& sequence_num, std::atomic<uint64_t>& highest_write_received) {
		empty_slots.acquire();

		std::lock_guard<std::mutex> lock(producer_mutex);
		sequence_num = sequence_number.fetch_add(1, std::memory_order_relaxed);
		highest_write_received.store(sequence_num, std::memory_order_release);

		buffer[in] = std::move(item);
		in = in+1 < N ? in+1 : 0;

		full_slots.release();
	}

	void consume(std::vector<T>& container, uint64_t highest_write_completed) {
		full_slots.acquire();

		if (stop_buffer.load(std::memory_order_acquire)) return;

		uint64_t taken = 0;
		do {
			container.push_back(std::move(buffer[out]));
			out = out+1 < N ? out+1 : 0;
			++taken;
		} while(taken < max_elems && full_slots.try_acquire());

		empty_slots.release(taken);
	}

	void stop(int n) {
		stop_buffer.store(true, std::memory_order_release);

		for (int i=0; i<n; ++i)
		full_slots.release();
	}

};
