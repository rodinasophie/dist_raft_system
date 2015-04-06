#ifndef __ISTATE_MACHINE_HPP_
#define __ISTATE_MACHINE_HPP_

#include "log/ILogEntry.hpp"

class IStateMachine {
 public:
	IStateMachine() {};
	virtual ~IStateMachine() {};
	virtual void Apply(ILogEntry *log_entry) = 0;
};

#endif // __ISTATE_MACHINE_HPP_
