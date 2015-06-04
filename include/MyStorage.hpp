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

	void Reset() {
		storage_.clear();
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

	std::string GetContents() {
		std::string res = "";
		for (auto it = storage_.begin(); it != storage_.end();
				++it) {
			res += it->first + ":" + it->second + "|";
		}
		res.resize(res.length() - 1);
		return res;
	}
 private:
	std::map<string, string> storage_;
};

#endif // __MY_STORAGE_HPP_
