#ifndef __MY_STATE_MACHINE_HPP_
#define __MY_STATE_MACHINE_HPP_

#include "MyStorage.hpp"
#include "IStateMachine.hpp"
#include "include/Interfaces.hpp"

class MyStateMachine : public IStateMachine {
 public:
	MyStateMachine() {
		storage_ = new MyStorage();
	}
	~MyStateMachine() {}

	void Apply(ILogEntry *log_entry) {
		IAction *action = log_entry->GetAction();
		action->Act(storage_);
	}
 private:
	MyStorage *storage_;
};

#endif // __MY_STATE_MACHINE_HPP_
