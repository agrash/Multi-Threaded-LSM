#pragma once
#include <counting_semaphore>
#include <semaphore>
#include <vector>
#include <mutex>

template<typename T, int N>
class RingBuffer {
private:
	std::vector<T> buffer;
	int in;
	int out;

	std::mutex producer_mutex;
	std::mutex consumer_mutex;

	std::counting_semaphore<N> empty_slots{N};
	std::counting_semaphore<N> full_slots{0};
public:
	RingBuffer() : in(0), out(0), buffer(N) {}

	void produce(T& item) {
		empty_slots.acquire();

		std::lock_guard<std::mutex> lock(producer_mutex);

		buffer[in] = std::move(item);
		in = in+1 < N ? in+1 : 0;

		full_slots.release();
	}

	T consume() {
		full_slots.acquire();

		std::lock_guard<std::mutex> lock(consumer_mutex);

		T return_item = std::move(buffer[out]);
		out = out+1 < N ? out+1 : 0;

		empty_slots.release();

		return return_item;
	}

};
