#ifndef __UNIX_SOCKET_HPP_
#define __UNIX_SOCKET_HPP_

#include <list>
using std::list;

#include <vector>
using std::vector;

#include "Socket.hpp"

class UnixSocket : public Socket {
 public:
	UnixSocket();
	~UnixSocket();

	bool Connect(string &server_name, string &server_port);

	bool Bind(int server_port);
	bool AcceptIncomings();
	size_t SendToAll(string data);

	// If Send() was called from server's side,
	// the recipient is the last client whose message was received
	size_t Send(string data);
	size_t Recv(string &data);

 private:
	int sfd_, client_sfd_;
	vector<int> clients_;
  list<int> ready_fds_;
	enum {
		NOT_INITED,
		CLIENT,
		SERVER,
	} state_;
	bool GetReadyClient();
};

#endif // __UNIX_SOCKET_HPP_
