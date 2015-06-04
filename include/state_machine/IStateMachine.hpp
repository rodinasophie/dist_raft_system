#ifndef __ISTATE_MACHINE_HPP_
#define __ISTATE_MACHINE_HPP_

#include "log/ILogEntry.hpp"

class IStateMachine {
 public:
	IStateMachine() {};
	virtual ~IStateMachine() {};
	virtual std::string Apply(ILogEntry *log_entry) = 0;
	virtual void Reset() = 0;
	virtual string CreateSnapshot(std::string filename) = 0;
};

#endif // __ISTATE_MACHINE_HPP_
