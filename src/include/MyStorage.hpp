#ifndef __MY_STORAGE_HPP_
#define __MY_STORAGE_HPP_

#include <map>

#include "include/Interfaces.hpp"

class MyStorage : public IStorage {
 public:
	MyStorage() {};
	~MyStorage() {};
	void Add(IKey *key, IValue *value) {
		storage_[key->ToString()] = value->ToString();
	}

	void Delete(IKey *key, IValue *value) {
		storage_.erase(key->ToString());
	}

 private:
	std::map<string, string> storage_;
};

#endif // __MY_STORAGE_HPP_
