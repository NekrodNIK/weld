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
	for (const auto& [key, value] : map) {
		ASSERT_TRUE(map.at(key) == cmp_map.at(key) && cmp_map.at(key) == value);
	}
}


int main() {
	testing::InitGoogleTest();
	auto s = RUN_ALL_TESTS();
}