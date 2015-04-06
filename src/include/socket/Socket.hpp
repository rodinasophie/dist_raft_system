#ifndef __SOCKET_HPP_
#define __SOCKET_HPP_

#include <string>
using std::string;

class Socket {
 public:
	Socket() {};
	virtual ~Socket() {};

	virtual bool Connect(string &server_name, string &server_port) = 0;

	virtual bool Bind(int server_port) = 0;
	virtual bool AcceptIncomings() = 0;

	virtual size_t Send(string data) = 0;
	virtual size_t SendToAll(string data) = 0;
	virtual size_t Recv(string &data) = 0;
};

#endif // __SOCKET_HPP_
