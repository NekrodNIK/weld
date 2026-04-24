#include <gtest/gtest.h>
#include "hashmap.h"
#include "thread-pool.h"
#include <iostream>


int fib(int x) {
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

int main() {
	testing::InitGoogleTest();
	auto s = RUN_ALL_TESTS();
}