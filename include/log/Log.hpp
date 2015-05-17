#ifndef __LOG_HPP_
#define __LOG_HPP_

#include "lib/counted_ptr.hpp"
#include "ILogEntry.hpp"
#include <iostream>
#include <vector>
using std::vector;

class Log {
 public:
	Log() : searched_(0), is_consistent_(false) {
		log_entries_.clear();
		matching_entry_found_ = false;
	};
	~Log() {};
	void Add(counted_ptr<ILogEntry> &ptr) {
		log_entries_.push_back(ptr);
		std::cout<<"Adding idx = "<<ptr.get()->GetIndex()<<
			", term = "<<ptr.get()->GetTerm()<<"\n";
		std::cout<<"LOG:Added to log, size == "<<log_entries_.size()<<"\n";
	}

	ILogEntry *Extract() {
		if (log_entries_.empty())
			return NULL;

		ILogEntry *obj = log_entries_[0].get();
		log_entries_.erase(log_entries_.begin());
		return obj;
	}

	ILogEntry *Get() {
		if (log_entries_.empty())
			return NULL;
		return log_entries_.front().get();
	}

	ILogEntry *GetLast() {
		if (log_entries_.empty())
			return NULL;
		return log_entries_.back().get();
	}

	ILogEntry *GetPrevLast() {
		if ((log_entries_.empty()) || (log_entries_.size() == 1))
			return NULL;
		return log_entries_[log_entries_.size() - 2].get();
	}

	bool Search(size_t term, size_t idx) {
		//std::cout<<"\nLOG: Searching idx = "<<idx<<" with term = "<< term<<"\n\n";
		for (size_t i = 0; i < log_entries_.size(); ++i) {
			if ((log_entries_[i].get()->GetTerm() == term) &&
					(log_entries_[i].get()->GetIndex() == idx)) {
				searched_ = i;
				return true;
			}
		}
		return false;
	}

	ILogEntry *Search(size_t idx) {
		//std::cout<<"Search for idx = "<< idx-1<<"\n";
		if ((idx <= log_entries_.size()) && (idx >= 1))
			return log_entries_[idx - 1].get();
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
	bool IsMatchingEntryFound() {
		return matching_entry_found_;
	}
	void SetConsistent(bool state) {
		is_consistent_ = state;
	}

	void SetMatchingEntryFound(bool state) {
		matching_entry_found_ = state;
	}
 private:
	vector<counted_ptr<ILogEntry>> log_entries_;
	size_t searched_;
	bool is_consistent_,
			 matching_entry_found_;
};

#endif // __LOG_HPP_
