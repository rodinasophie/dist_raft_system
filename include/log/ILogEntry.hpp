#ifndef __ILOG_ENTRY_HPP_
#define __ILOG_ENTRY_HPP_

#include "include/Interfaces.hpp"

#include <string>
using std::string;

class ILogEntry {
 public:
	ILogEntry() : term_(0), idx_(1) {}
	virtual ~ILogEntry() {};

	virtual string &ToSend() = 0;
	virtual void SetData(string &str) = 0;

	virtual IKey *GetKey() = 0;
	virtual IValue *GetValue() = 0;
	virtual IAction *GetAction() = 0;

	void SetIndex(size_t idx) {
		idx_ = idx;
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

 private:
	size_t term_, idx_;
};

#endif // __ILOG_ENTRY_HPP_
