#ifndef __MY_LOG_ENTRY_HPP_
#define __MY_LOG_ENTRY_HPP_

#include "ILogEntry.hpp"

enum Action {
	ADD,
	DELETE,
	SET,
	GET,
};

class MyLogEntry : public ILogEntry {
 public:
	MyLogEntry(Action action, string key, string value) {
		action_ = new KeyValAction(action, this);
		key_ = new StringKey(key);
		value_ = new StringValue(value);
	};

	MyLogEntry() {};
	~MyLogEntry() {};

	void SetData(string &str) {
		switch (str[0]) {
			case 'a':
				action_->SetAction(ADD);
				break;
			case 'd':
				action_->SetAction(DELETE);
				break;
			case 's':
				action_->SetAction(SET);
				break;
			case 'g':
				action_->SetAction(GET);
				break;
		}
		size_t pos = str.find(",", 4);
		key_->Set(str.substr(4, pos - 4));
		value_->Set(str.substr(pos + 1, str.length() - pos + 1));
	}

	// this method creates a string for sending
	string &ToSend() {
		message_ = "";
		switch (action_->GetAction()) {
			case ADD:
				message_ += "add,";
				break;
			case DELETE:
				message_ += "del,";
				break;
			case SET:
				message_ += "set,";
				break;
			case GET:
				message_ += "get,";
				break;
		}
		message_ += key_->ToString() + "," + value_->ToString();
		return message_;
	}

	IKey *GetKey() {
		return key_;
	}

	IValue *GetValue() {
		return value_;
	}

	IAction *GetAction() {
		return action_;
	}

 private:
	class StringKey : public IKey {
	 public:
		StringKey(string key) : key_(key) {};
		~StringKey() {};

		string &ToString() {
			return key_;
		}
		void Set(string key) {
			key_ = key;
		}
	 private:
		string key_;
	};

	class StringValue : public IValue {
	 public:
		StringValue(string value) : value_(value) {};
		~StringValue() {};

		string &ToString() {
			return value_;
		}
		void Set(string value) {
			value_ = value;
		}
	 private:
		string value_;
	};

	class KeyValAction : public IAction {
	 public:
		KeyValAction(Action action, MyLogEntry *le) : my_action_(action), le_(le) {};
		~KeyValAction() {};
		Action GetAction() {
			return my_action_;
		}
		void SetAction(Action action) {
			my_action_ = action;
		}
		void Act(IStorage *storage) {
			switch (my_action_) {
				case ADD:
					storage->Add(le_->GetKey(), le_->GetValue());
					break;
				case DELETE:
					storage->Delete(le_->GetKey(), le_->GetValue());
					break;
				case SET:
					storage->Add(le_->GetKey(), le_->GetValue());
					break;
				case GET:
					storage->Add(le_->GetKey(), le_->GetValue());
					break;
			}
		}
	  private:
			Action my_action_;
			MyLogEntry *le_;
	};

	string message_;
	StringKey *key_;
	StringValue *value_;
	KeyValAction *action_;
};

#endif // __MY_LOG_ENTRY_HPP_
