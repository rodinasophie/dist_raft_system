#ifndef __IRESPONSE_HPP_
#define __IRESPONSE_HPP_

#include <string>
using std::string;

class IResponse {
 public:
	IResponse() {};
	virtual ~IResponse() {};
	virtual void SetData(string str) = 0;
};

#endif // __IRESPONSE_HPP_
