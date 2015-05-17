#ifndef __MY_STORAGE_HPP_
#define __MY_STORAGE_HPP_

#include <map>
#include <iostream>
#include "include/Interfaces.hpp"

// FIXME: Check errors!!!

class MyStorage : public IStorage {
 public:
	MyStorage() {};
	~MyStorage() {};
	string Add(IKey *key, IValue *value) {
		storage_[key->ToString()] = value->ToString();
		return "+";
	}

	string Delete(IKey *key) {
		if (storage_.erase(key->ToString())) {
			return "+";
		}
		return "-";
	}

	string Get(IKey *key) {
		try {
			string str = storage_.at(key->ToString());
			std::cout<<"Returning "<<str<<"\n";
			return str;
		} catch (std::out_of_range &e) {
			//std::cout<<"Returning -\n";
			return "-";
		}
	}

 private:
	std::map<string, string> storage_;
};

#endif // __MY_STORAGE_HPP_
