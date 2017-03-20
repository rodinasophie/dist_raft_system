#ifndef __LOG_HPP_
#define __LOG_HPP_

#include "lib/counted_ptr.hpp"
#include "ILogEntry.hpp"
#include "include/Mutex.hpp"
#include <iostream>
#include <algorithm>
#include <list>

using std::list;

class Log {
 public:
    Log() {
        log_entries_.clear();
    };
    ~Log() {
        m_.Unlock();
        for (auto it = log_entries_.begin(); it != log_entries_.end(); ++it) {
            if (it->get())
                delete it->get();
        }
    };
    void Add(counted_ptr<ILogEntry> &ptr);
    ILogEntry *GetFirst();
    ILogEntry *GetLast();
    ILogEntry *GetPrevLast();
    bool Search(size_t term, size_t idx);
    ILogEntry *Search(size_t idx);
    void Delete(size_t from, size_t to);
    size_t GetLengthInBytes();

 private:
    static bool IsInLog(counted_ptr<ILogEntry> ptr);
    list<counted_ptr<ILogEntry>> log_entries_;
    static size_t idx_to_find_;
    Mutex m_;
};

#endif // __LOG_HPP_
