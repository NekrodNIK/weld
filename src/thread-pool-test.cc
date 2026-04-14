#include <future>
#include <gtest/gtest.h>
#include <vector>
#include "thread-pool.h"


int fib(int x) {
	if (x <= 1) {
		return x;
	}
	return fib(x - 1) + fib(x - 2);
}

std::vector<int> fib_seq(int start, int len) {
	std::vector<int> result;
	for (int i = start; i < start + len; i++) {
		result.push_back(fib(i));
	}
	return result;
}


std::vector<int> fib_parallel(int start, int len) {
	std::vector<int> result;
	std::vector<std::future<int>> futures;
	ThreadPool pool(16);
	for (int i = start; i < start + len; i++) {
		futures.push_back(pool.submit(fib, i));
	}
	pool.await_termination();
	for (int j = 0; j < len; j++) {
		result.push_back(futures[j].get());
	}
	return result;
}

TEST(ThreadPool, Submit) {
	ThreadPool pool(16);
	int l = 10;
	int offset = 15;
	std::vector<std::future<int>> res;
	for (int i = 0; i < l; i++) {
		res.push_back(pool.submit(fib, i + offset));
	}
	pool.await_termination();
	int fut_res;
	for (int j = 0; j < l; j++) {
		fut_res = res[j].get();
		ASSERT_EQ(fut_res, fib(j + offset));
	}
}


TEST(ThreadPool, CmpTime) {
	std::vector<int> res1;
	std::vector<int> res2;
	int n = 1;
	int m = 50;
	std::chrono::steady_clock::time_point begin1 = std::chrono::steady_clock::now();
	res1 = fib_seq(n, m);
	std::chrono::steady_clock::time_point end1 = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point begin2 = std::chrono::steady_clock::now();
	res2 = fib_parallel(n, m);
	std::chrono::steady_clock::time_point end2 = std::chrono::steady_clock::now();
	std::cout << "Sequential time = " << std::chrono::duration_cast<std::chrono::microseconds>(end1 - begin1).count() << "[µs]" << std::endl;
	std::cout << "Parallel time = " << std::chrono::duration_cast<std::chrono::microseconds>(end2 - begin2).count() << "[µs]" << std::endl;
	ASSERT_EQ(res1, res2);
}

int main() {
	testing::InitGoogleTest();
	auto s = RUN_ALL_TESTS();
}