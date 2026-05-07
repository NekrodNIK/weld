#include <functional>
#include <future>
#include <gtest/gtest.h>
#include <vector>
#include "../src/thread-pool.h"


#define f(i) []() -> void {fib(i);}

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

TEST(ThreadPool, SubmitAllCollection) {
	ThreadPool pool(16);
	std::vector<std::function<void()>> funcs;
	for (int i = 1; i < 40; i++) {
		funcs.push_back([i]() -> void {
			fib(i);
		});
	}
	pool.submit_all(funcs);
	pool.await_termination();
}

TEST(ThreadPool, SubmitAllFunctions) {
	ThreadPool pool(16);
	pool.submit_all(f(1), f(2), f(3), f(4), f(5), f(6), f(7), f(8), 
					f(9), f(10), f(11), f(12), f(13), f(14), f(15), 
					f(16), f(17), f(18), f(19), f(20), f(21), f(22), 
					f(23), f(24), f(25), f(26), f(27), f(28), f(29), 
					f(30), f(31), f(32), f(33), f(34), f(35), f(36), 
					f(37), f(38), f(39), f(40));
	pool.await_termination();
}

TEST(ThreadPool, Chaining) {
	ThreadPool pool(16);
	Tasks tasks(pool);
	std::vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	tasks.foreach(v, [](int& x) -> void { x *= 2;}).wait();
	for (size_t i = 0; i < v.size(); i++) {
		ASSERT_EQ(v[i], (i + 1) * 2);
	}
}


TEST(ThreadPool, Chaining2) {
	ThreadPool pool(16);
	Tasks tasks(pool);
	std::vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	tasks.foreach(v, [](int& x) -> void { x *= 2;}).foreach(v, [](int& x) -> void {x += 1;}).wait();
	for (size_t i = 0; i < v.size(); i++) {
		ASSERT_EQ(v[i], (i + 1) * 2 + 1);
	}
}


TEST(ThreadPool, Chaining3) {
	ThreadPool pool(16);
	Tasks tasks(pool);
	std::vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	tasks.foreach(v, [](int& x) -> void { x *= 2;}).wait();
	for (size_t i = 0; i < v.size(); i++) {
		ASSERT_EQ(v[i], (i + 1) * 2);
	}
	tasks.foreach(v, [](int& x) -> void {x += 1;}).wait();
	for (size_t i = 0; i < v.size(); i++) {
		ASSERT_EQ(v[i], (i + 1) * 2 + 1);
	}
}

TEST(ThreadPool, CmpTime) {
	#ifdef BIG_TEST
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
	#endif 
}


int main() {
	testing::InitGoogleTest();
	auto s = RUN_ALL_TESTS();
}