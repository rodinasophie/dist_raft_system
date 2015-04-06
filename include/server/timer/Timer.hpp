#ifndef __TIMER_HPP_
#define __TIMER_HPP_

#include <future>

class Timer {
 public:
	Timer(int timeout) : timeout_(timeout) {}
	~Timer() {}

	void Run() {
		future_ = std::async(std::launch::async, Sleep, timeout_);
	}

	bool TimedOut() {
		auto ticks = future_.wait_for(std::chrono::milliseconds(0));
		return (ticks == std::future_status::ready);
	}

 private:
	static void Sleep(int timeout) {
		std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
	}

	std::future<void> future_;
	int timeout_;
};

#endif // __TIMER_HPP_
