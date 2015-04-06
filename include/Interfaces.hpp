#ifndef __INTERFACES_HPP_
#define __INTERFACES_HPP_

#include <string>
using std::string;

class IStorage;

class IKey {
 public:
	IKey() {};
	virtual ~IKey() {};
	virtual string &ToString() = 0;
};

class IValue {
 public:
	IValue() {};
	virtual ~IValue() {};
	virtual string &ToString() = 0;
};

class IAction {
 public:
	IAction() {};
	virtual ~IAction() {};
	virtual void Act(IStorage *storage) = 0;
};

class IStorage {
 public:
	IStorage() {};
	virtual ~IStorage() {};
	virtual void Add(IKey *key, IValue * value) = 0;
	virtual void Delete(IKey *key, IValue * value) = 0;
};


#endif // __INTERFACES_HPP_
