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

	void Reset();
	int Connect(string &my_name, string &server_name, string &server_port);

	bool Bind(string ip, int server_port);
	int AcceptIncomings(string &client_ip_addr);
	bool SetReceiver(int sfd);
	// If Send() was called from server's side,
	// the recipient is the last client whose message was received
	size_t Send(string data);
	size_t Recv(string &data);
	size_t SendToAll(const string &data);

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
