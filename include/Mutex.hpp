#ifndef __MUTEX_HPP_
#define __MUTEX_HPP_

#include <mutex>

class Mutex {
 public:
    Mutex() {};
    ~Mutex() {
        m_.unlock();
    };
    void Lock() {
        while (!m_.try_lock()) {}
    }

    void Unlock() {
        m_.unlock();
    }
 private:
    std::mutex m_;
};

#endif // __MUTEX_HPP_
