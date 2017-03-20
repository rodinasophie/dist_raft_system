#ifndef __MY_RESPONSE_HPP_
#define __MY_RESPONSE_HPP_

#include "IResponse.hpp"

class MyResponse : public IResponse {
 public:
    MyResponse() {}
    ~MyResponse() {}
    void SetData(string str) {
        s_ = str;
    }

    string &GetData() {
        return s_;
    }

 private:
    string s_;
};

#endif // __MY_RESPONSE_HPP_
