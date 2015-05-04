#ifndef __LOG_HPP_
#define __LOG_HPP_

#include "ILogEntry.hpp"
#include <iostream>
#include <vector>
using std::vector;

class Log {
 public:
	Log() : searched_(0), is_consistent_(false) {
		log_entries_.clear();
	};
	~Log() {};
	void Add(ILogEntry *log_entry) {
		std::cout<<"LOG:Added to log, size == "<<log_entries_.size()<<"\n";
		log_entries_.push_back(log_entry);
	}

	ILogEntry *Extract() {
		if (log_entries_.empty())
			return NULL;
		ILogEntry *obj = log_entries_[0];
		log_entries_.erase(log_entries_.begin());
		return obj;
	}

	ILogEntry *Get() {
		if (log_entries_.empty())
			return NULL;
		return log_entries_.front();
	}

	ILogEntry *GetLast() {
		if (log_entries_.empty())
			return NULL;
		return log_entries_.back();
	}

	ILogEntry *GetPrevLast() {
		if ((log_entries_.empty()) || (log_entries_.size() == 1))
			return NULL;
		auto it = log_entries_.end();
		--it;
		--it;
		return *it;
	}

	bool Search(size_t term, size_t idx) {
		//std::cout<<"\nLOG: Searching idx = "<<idx<<" with term = "<< term<<"\n\n";
		for (size_t i = 0; i < log_entries_.size(); ++i) {
			if ((log_entries_[i]->GetTerm() == term) && (log_entries_[i]->GetIndex() == idx)) {
				searched_ = i;
				return true;
			}
		}
		return false;
	}

	ILogEntry *Search(size_t idx) {
		std::cout<<"idx = "<<idx<<", size = "<<log_entries_.size()<<"\n";
		if (idx <= log_entries_.size())
			return log_entries_[idx - 1];
		return NULL;
	}

	void Delete() {
		//log_entries_.resize(searched_ + 1);
	}

	size_t GetLength() {
		return log_entries_.size();
	}

	bool IsConsistent() {
		return is_consistent_;
	}

	void SetConsistent(bool state) {
		is_consistent_ = state;
	}
 private:
	vector<ILogEntry *> log_entries_;
	size_t searched_;
	bool is_consistent_;
};

#endif // __LOG_HPP_
