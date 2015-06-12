#ifndef __ILOG_ENTRY_HPP_
#define __ILOG_ENTRY_HPP_

#include "include/Interfaces.hpp"
#include<iostream>
#include <string>
using std::string;

class ILogEntry {
 public:
	ILogEntry() : term_(0), idx_(1) {}
	virtual ~ILogEntry() {};

	virtual string &ToSend() = 0;

	virtual IKey *GetKey() = 0;
	virtual IValue *GetValue() = 0;
	virtual IAction *GetAction() = 0;
	virtual string &GetLogData() = 0;

	void SetIndex(size_t idx) {
		idx_ = idx;
		//std::cout<<"Setting idx_ to "<<idx_<< " or "<<idx<<"\n";
	}

	size_t GetIndex() {
		return idx_;
	}

	void SetTerm(size_t term) {
		term_ = term;
	}

	size_t GetTerm() {
		return term_;
	}

 protected:
	size_t term_, idx_;
};

#endif // __ILOG_ENTRY_HPP_
