#include <cassert>
#include <gtest/gtest.h>
#include "../src/hashmap.h"
#include "../src/thread-pool.h"
#include <iostream>
#include <unordered_map>

static int fib(int x) {
	if (x <= 1) {
		return x;
	}
	return fib(x - 1) + fib(x - 2);
}

TEST(HashMap, Base) {
	ThreadPool pool(16);
	LockFreeHashMap<int, int> map;
	for (int i = 0; i < 16; i++) {
		pool.submit([i, &map]() -> void {
			map.insert(i, fib(i + 30));
		});
	}
	pool.await_termination();
	std::cout << "inserted" << std::endl;
	for (int i = 0; i < 16; i++) {
		std::cout << map.at(i) << std::endl;
	}
	std::cout << "getted" << std::endl;
}


TEST(HashMap, Iterator) {
	LockFreeHashMap<int, int> map;
	std::unordered_map<int, int> cmp_map;
	for (int i = 0; i < 16; i++) {
		int value = fib(i + 30);
		map.insert(i, value);
		cmp_map.emplace(i, value);
	}
	for (const auto&& [key, value] : map) {
		ASSERT_TRUE(map.at(key) == cmp_map.at(key) && cmp_map.at(key) == value);
	}
}

TEST(HashMap, InsertRemove) {
    LockFreeHashMap<int, int> map(10);
    const int num_threads = 8;
    const int ops_per_thread = 1000;
    ThreadPool pool(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        pool.submit([&map, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                map.insert(t * ops_per_thread + i, i);
            }
        });
    }
    pool.await_termination();
    ThreadPool pool2(num_threads);
    for (int t = 0; t < num_threads; t++) {
        pool2.submit([&map, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                if (i % 2 == 0) {
                    map.remove(t * ops_per_thread + i);
                } else {
                    map.insert(1000000 + t * ops_per_thread + i, i);
                }
            }
        });
    }
    pool2.await_termination();
}

TEST(HashMap, ConcurrentIteration) {
    LockFreeHashMap<int, int> map(100);
    const int num_threads = 4;
    std::atomic<bool> stop{false};
    std::thread writer([&map, &stop]() {
        int i = 0;
        while (!stop) {
            map.insert(i % 1000, i);
            if (i % 2 == 0) map.remove((i - 10) % 1000);
            i++;
        }
    });
    std::vector<std::thread> readers;
    for (int i = 0; i < num_threads; i++) {
        readers.emplace_back([&map, &stop]() {
            while (!stop) {
                int count = 0;
                for (auto it = map.begin(); it != map.end(); it++) {
                    count++;
                    (void)it->key;
                    (void)it->value;
                }
            }
        });
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    stop = true;
    writer.join();
    for (auto& t : readers) t.join();
}


TEST(HashMap, InPlaceUpdate) {
    LockFreeHashMap<int, int> map(10);
    map.insert(1, 10);
    const int num_threads = 4;
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&map]() {
            for (int j = 0; j < 1000; j++) {
                map.at(1)++;
            }
        });
    }
    for (auto& t : threads) {
    	t.join();
    }
}
