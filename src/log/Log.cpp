#include "log/Log.hpp"
#include <sys/time.h>
#include <unistd.h>

size_t Log::idx_to_find_ = 0;
void Log::Add(counted_ptr<ILogEntry> &ptr) {
    m_.Lock();
    log_entries_.push_back(ptr);
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
    ILogEntry *le_prev = GetPrevLast(),
                        *le_last = GetLast();
    if ((le_last) && (idx > le_last->GetIndex())) {
        return false;
    }
    m_.Lock();
    if (le_prev) {
        if ((le_prev->GetIndex() == idx) && (le_prev->GetTerm() == term)) {
            m_.Unlock();
            return true;
        }
    }
    if (le_last) {
        if ((le_last->GetIndex() == idx) && (le_last->GetTerm() == term)) {
            m_.Unlock();
            return true;
        }
    }
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
    if (idx >= 1) {
        ILogEntry *le_prev = GetPrevLast(),
                            *le_last = GetLast();
        if ((le_last) && (idx > le_last->GetIndex())) {
            return NULL;
        }
        m_.Lock();
        if (le_prev) {
            if (le_prev->GetIndex() == idx) {
                m_.Unlock();
                return le_prev;
            }
        }
        if (le_last) {
            if (le_last->GetIndex() == idx) {
                m_.Unlock();
                return le_last;
            }
        }

        Log::idx_to_find_ = idx;
        auto it = std::find_if(log_entries_.begin(), log_entries_.end(), IsInLog);
        if (it != log_entries_.end()) {
            ILogEntry *le = it->get();
            m_.Unlock();
            return le;
        }
    }
    m_.Unlock();
    return NULL;
}

void Log::Delete(size_t from, size_t to) {
    if (from > to)
        return;
    m_.Lock();
    Log::idx_to_find_ = from;
    auto it_from = std::find_if(log_entries_.begin(), log_entries_.end(), IsInLog);
    auto it_to = it_from;
    if (from != to) {
        Log::idx_to_find_ = to;
        it_to = std::find_if(log_entries_.begin(), log_entries_.end(), IsInLog);
    }
    ++it_to;
    log_entries_.erase(it_from, it_to);
    m_.Unlock();
}

size_t Log::GetLengthInBytes() {
    m_.Lock();
    size_t size = sizeof(list<counted_ptr<ILogEntry>>) +
        sizeof(counted_ptr<ILogEntry>) * log_entries_.size();
    m_.Unlock();
    return size;
}

bool Log::IsInLog(counted_ptr<ILogEntry> ptr) {
    if (ptr.get()->GetIndex() == Log::idx_to_find_) {
        return true;
    }
    return false;
}

