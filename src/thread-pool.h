#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include <concepts>
#include <ranges>


class ThreadPool {
private:
	struct TaskQueue {
		std::queue<std::function<void()>> tasks;
		std::mutex mutex;
		std::condition_variable cv;	
	};

	std::vector<std::unique_ptr<TaskQueue>> queues;
	std::vector<std::thread> threads;
	std::atomic<bool> running;
	std::atomic<size_t> current_worker;

	void worker(size_t thread_index) {
		assert(thread_index < threads.size());
		while (true) {
			std::function<void()> current_task;
			{
				std::unique_lock<std::mutex> lock(queues[thread_index]->mutex);
				queues[thread_index]->cv.wait(lock, [this, thread_index] () -> bool {
					return !running || !queues[thread_index]->tasks.empty();
				});
				if (!running && queues[thread_index]->tasks.empty()) {
					return;
				}
				current_task = std::move(queues[thread_index]->tasks.front());
				queues[thread_index]->tasks.pop();
			}
			current_task();
		}
	}

public:

	ThreadPool(size_t core_pool_size) : running(true), current_worker(0) {
		threads.reserve(core_pool_size);
		for (size_t i = 0; i < core_pool_size; i++) {
			queues.emplace_back(std::make_unique<TaskQueue>());
			threads.emplace_back(&ThreadPool::worker, this, i);
		}
	}

	~ThreadPool() {
		running = false;
		for (auto& q : queues) {
			q->cv.notify_one();
		}
		for (auto& t : threads) {
			t.join();
		}
	}

	template<typename Function, typename... Args>
	auto submit(Function&& func, Args&&... args) -> std::future<decltype(func(args...))> {
		using ret_type = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<ret_type()>>(std::bind(std::forward<Function>(func), 
			std::forward<Args>(args)...));
		std::future<ret_type> result = task->get_future();
		std::function<void()> wrapper = [task]() -> void { (*task)(); };
		size_t index = current_worker.fetch_add(1, std::memory_order_relaxed) % queues.size();
		{
			std::lock_guard<std::mutex> lock(queues[index]->mutex);
			queues[index]->tasks.push(std::move(wrapper));
		}
		queues[index]->cv.notify_one();
		return result;
	}

	template<typename... Functions>
	requires (std::invocable<Functions> && ...)
	void submit_all(Functions&&... funcs) {
		std::vector<std::future<void>> futures;
		futures.reserve(sizeof...(funcs));
		(futures.push_back(submit([func = std::forward<Functions>(funcs)]() mutable {func();})), ...);
		for (auto& f : futures) {
			f.get();
		}
	}

	template<typename Collection>
	requires std::invocable<typename Collection::value_type>
	void submit_all(const Collection& tasks) {
		std::vector<std::future<void>> futures;
		futures.reserve(tasks.size());
			for (const auto& task : tasks) {
	        futures.push_back(submit(task));
	    }
        for (auto& fut : futures) {
        	fut.get();
    	}
	}

	void await_termination() {
		running = false;
		for (auto& q : queues) {
			q->cv.notify_one();
		}
		bool ready = false;
		size_t time_to_sleep = 12;
		size_t max_time = 1000;
		while (!ready) {
			ready = true;
			for (auto& q : queues) {
				{
					std::lock_guard<std::mutex> lock(q->mutex);
					if (!q->tasks.empty()) {
						ready = false;
						break;
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(time_to_sleep));
			time_to_sleep = std::min(time_to_sleep * 2, max_time);
		}
	}

};

struct Chain {
private:
	ThreadPool& pool;
	std::vector<std::future<void>> futures;

public:

	Chain(ThreadPool& pool_) : pool(pool_) {}

 	template <typename C, typename F>
    requires(std::ranges::range<C> && std::invocable<F, std::ranges::range_reference_t<C>>)
    Chain& foreach(C& collection, F&& func) {
    	for (auto& c : collection) {
    		futures.push_back(pool.submit([&func, &c]() { func(c); }));
    	}
    	return *this;
    }

    template <typename C, typename F>
    requires(std::ranges::range<C> && std::invocable<F, std::ranges::range_reference_t<C>>)
    Chain& foreach(C&& collection, F&& func) {
    	for (auto& c : collection) {
    		futures.push_back(pool.submit([&func, &c]() { func(c); }));
    	}
    	return *this;
    }

    void wait() {
    	for (auto& f : futures) {
    		f.get();
    	}
    	futures.clear();
    }
};

struct Tasks {
	ThreadPool& pool;

	Tasks(ThreadPool& pool_) : pool(pool_) {}

	template <typename C, typename F>
    requires(std::ranges::range<C> && std::invocable<F, std::ranges::range_reference_t<C>>)
    Chain foreach(C& collection, F&& func) {
    	Chain chain(pool);
    	chain.foreach(collection, std::forward<F>(func));
    	return chain;
    }
};