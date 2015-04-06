#ifndef __LOG_HPP_
#define __LOG_HPP_

#include "ILogEntry.hpp"

#include <list>
using std::list;

class Log {
 public:
	Log() {};
	~Log() {};
	void Add(ILogEntry *log_entry) {
		log_entries_.push_back(log_entry);
	}

	ILogEntry *Extract() {
		ILogEntry *obj = log_entries_.front();
		log_entries_.pop_front();
		return obj;
	}

	ILogEntry *Get() {
		return log_entries_.front();
	}
 private:
	list<ILogEntry *> log_entries_;
};

#endif // __LOG_HPP_
