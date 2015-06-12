#ifndef __MY_STATE_MACHINE_HPP_
#define __MY_STATE_MACHINE_HPP_

#include "MyStorage.hpp"
#include "IStateMachine.hpp"
#include "include/Interfaces.hpp"
#include "include/Mutex.hpp"

#include <fstream>
#include <iostream>

class MyStateMachine : public IStateMachine {
 public:
	MyStateMachine() {
		storage_ = new MyStorage();
	}
	~MyStateMachine() {}

	std::string Apply(ILogEntry *log_entry) {
		mtx_.Lock();
		IAction *action = log_entry->GetAction();
		string str = action->Act(storage_);
		mtx_.Unlock();
		return str;
	}

	void Reset() {
		mtx_.Lock();
		storage_->Reset();
		mtx_.Unlock();
	}

	string CreateSnapshot(std::string filename) {
	//std::cout<<"Begin2*\n";
		mtx_.Lock();
	//std::cout<<"Begin2*\n";
		string contents = storage_->GetContents();
	//std::cout<<"Begin2*\n";
		ofstream f;
		contents = "S:"+contents;
		//std::cout<<"Contents: "<<contents<<"\n";
	//std::cout<<"Begin2*\n";
		f.open(filename);
	//std::cout<<"Begin2*\n";
		f << contents;
		f.close();
		mtx_.Unlock();
		return contents;
	}
 private:
	MyStorage *storage_;
	Mutex mtx_;
};

#endif // __MY_STATE_MACHINE_HPP_
