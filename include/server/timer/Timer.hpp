#ifndef __TIMER_HPP_
#define __TIMER_HPP_

#include <chrono>
#include <iostream>
using namespace std::chrono;
class Timer {
 public:
	Timer(milliseconds timeout) : timeout_(timeout) {}
	~Timer() {}

	void Run() {
		start_ = high_resolution_clock::now();
	}

	bool TimedOut() {
		if (duration_cast<milliseconds>(high_resolution_clock::now() - start_) > timeout_)
			return true;
		return false;
	}

 private:
	milliseconds timeout_;
	high_resolution_clock::time_point start_;
};

#endif // __TIMER_HPP_
