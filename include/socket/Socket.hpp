#ifndef __SOCKET_HPP_
#define __SOCKET_HPP_

#include <string>
using std::string;

class Socket {
 public:
	Socket() {};
	virtual ~Socket() {};
	virtual void Reset() = 0;
	virtual int Connect(string &my_name, string &server_name,
	    string &server_port) = 0;

	virtual bool Bind(string ip, int server_port) = 0;
	virtual int AcceptIncomings(string &client_ip_addr) = 0;

	virtual size_t Send(string data) = 0;
	virtual size_t SendToAll(const string &data) = 0;
	virtual bool SetReceiver(int sfd) = 0;
	virtual size_t Recv(string &data) = 0;
};

#endif // __SOCKET_HPP_
