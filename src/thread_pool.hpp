#pragma once

#include "singleton.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/thread/thread.hpp>
#include <future>

template<size_t NUMBER_OF_THREADS>
class thread_pool : public singleton<thread_pool<NUMBER_OF_THREADS>> {
	friend singleton<thread_pool<NUMBER_OF_THREADS>>;

	boost::asio::io_context ioc_;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
	boost::thread_group thread_pool_;

	thread_pool() : work_guard_(boost::asio::make_work_guard(ioc_)) {
		for (size_t i = 0; i < NUMBER_OF_THREADS; ++i) {
			thread_pool_.create_thread(
				boost::bind(&boost::asio::io_context::run, &ioc_)
			);
		}
	}
public:
	static constexpr size_t number_of_threads() noexcept {
		return NUMBER_OF_THREADS;
	}
	boost::asio::io_context& ctx() noexcept { return ioc_; }
	void stop() noexcept { 
		ioc_.stop();
		thread_pool_.join_all();
	}
};

using thread_pool_1 = thread_pool<1>;
using thread_pool_2 = thread_pool<2>;
using thread_pool_3 = thread_pool<3>;
using thread_pool_4 = thread_pool<4>;
using thread_pool_5 = thread_pool<5>;
using thread_pool_6 = thread_pool<6>;
using thread_pool_7 = thread_pool<7>;
using thread_pool_8 = thread_pool<8>;

template<bool UseFuture, typename Executor, typename Func, typename... Args>
std::enable_if_t<!UseFuture, void>
async(Executor& ctx, Func&& job, Args&&... args) {
	boost::asio::post(ctx, std::bind(std::forward<Func>(job), std::forward<Args>(args)...));
}

template<bool UseFuture, typename Executor, typename Func, typename... Args>
std::enable_if_t<UseFuture, std::future<std::invoke_result_t<Func, Args...>>>
async(Executor& ctx, Func&& job, Args&&... args) {
	using boost::asio::use_future;
	return boost::asio::post(ctx, use_future(
		std::bind(std::forward<Func>(job), std::forward<Args>(args)...)
	));
}

template<typename ThreadPool, typename ForwardIterator, typename Func>
void exec_parallel(ThreadPool& pool, ForwardIterator begin, ForwardIterator end, Func&& func) {
	const size_t n = std::distance(begin, end);
	
	assert(n >= ThreadPool::number_of_threads());

	const size_t n_per_thread = n / ThreadPool::number_of_threads();
	const size_t remainder = n % ThreadPool::number_of_threads();
	const size_t ix_end = n - remainder;

	auto part_job = [] (Func fn, ForwardIterator it, ForwardIterator end) {
		for (; it != end; ++it) fn(*it);
	};

	for (size_t i = 0; i < ix_end; i += n_per_thread) {
		auto b = begin, e = begin;

		std::advance(b, i);
		std::advance(e, i + n_per_thread + ((remainder && (i + n_per_thread) >= ix_end) ? remainder : 0));
		
		boost::asio::post(
			pool.ctx(), 
			std::bind(part_job, func, b, e)
		);
	}
	
}
	
