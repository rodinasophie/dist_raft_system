#ifndef __THREAD_SAFE_STRING_HPP_
#define __THREAD_SAFE_STRING_HPP_

#include "include/Mutex.hpp"
#include <string>

class ThreadSafeString {
 public:
     ThreadSafeString() {};
     ~ThreadSafeString() {};
    const std::string Get() {
        m_.Lock();
        string str = data_;
        m_.Unlock();
        return str;
    }

    void Set(std::string str) {
        m_.Lock();
        data_ = str;
        m_.Unlock();
    }

 private:
    Mutex m_;
    std::string data_;
};
#endif // __THREAD_SAFE_STRING_HPP_
