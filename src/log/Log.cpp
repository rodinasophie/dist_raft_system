#include "log/Log.hpp"

size_t Log::idx_to_find_ = 0;
void Log::Add(counted_ptr<ILogEntry> &ptr) {
	m_.Lock();
	log_entries_.push_back(ptr);
	std::cout<<"Adding idx = "<<ptr.get()->GetIndex()<<
		", term = "<<ptr.get()->GetTerm()<<"\n";
	std::cout<<"LOG:Added to log, size == "<<log_entries_.size()<<"\n";
	m_.Unlock();
}

ILogEntry *Log::GetFirst() {
	m_.Lock();
	if (log_entries_.empty()) {
		m_.Unlock();
		return NULL;
	}
	ILogEntry *le = log_entries_.front().get();
	m_.Unlock();
	return le;
}

ILogEntry *Log::GetLast() {
	m_.Lock();
	if (log_entries_.empty()) {
		m_.Unlock();
		return NULL;
	}
	ILogEntry *le = log_entries_.back().get();
	m_.Unlock();
	return le;
}

ILogEntry *Log::GetPrevLast() {
	m_.Lock();
	if ((log_entries_.empty()) || (log_entries_.size() == 1)) {
		m_.Unlock();
		return NULL;
	}
	auto it = log_entries_.end();
	--it;
	--it;
	ILogEntry *le = it->get();
	m_.Unlock();
	return le;
}

bool Log::Search(size_t term, size_t idx) {
	m_.Lock();
	//std::cout<<"\nLOG: Searching idx = "<<idx<<" with term = "<< term<<"\n\n";
	for (auto it = log_entries_.begin(); it != log_entries_.end(); it++) {
		if ((it->get()->GetTerm() == term) &&
					(it->get()->GetIndex() == idx)) {
			m_.Unlock();
			return true;
		}
	}
	m_.Unlock();
	return false;
}

ILogEntry *Log::Search(size_t idx) {
	m_.Lock();
	//std::cout<<"Search for idx = "<< idx-1<<"\n";
	if ((idx <= log_entries_.size()) && (idx >= 1)) {
		Log::idx_to_find_ = idx;
		auto it = std::find_if(log_entries_.begin(), log_entries_.end(), IsInLog);
		ILogEntry *le = it->get();
	m_.Unlock();
		return le;
	}
		m_.Unlock();
	return NULL;
}

void Log::Delete(size_t from, size_t to) {
	m_.Lock();
	Log::idx_to_find_ = from;
	auto it_from = std::find_if(log_entries_.begin(), log_entries_.end(), IsInLog);
	Log::idx_to_find_ = to;
	auto it_to = std::find_if(log_entries_.begin(), log_entries_.end(), IsInLog);
	for (auto it = it_from; it != it_to; ++it) {
		if (it->get())
		delete it->get();
	}
	log_entries_.erase(it_from, ++it_to);
	m_.Unlock();
}

size_t Log::GetLengthInBytes() {
	m_.Lock();
	size_t size = sizeof(log_entries_);
	m_.Unlock();
	return size;
}

bool Log::IsInLog(counted_ptr<ILogEntry> ptr) {
	if (ptr.get()->GetIndex() == Log::idx_to_find_) {
		return true;
	}
	return false;
}

