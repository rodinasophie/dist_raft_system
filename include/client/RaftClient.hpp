#ifndef __RAFT_CLIENT_HPP_
#define __RAFT_CLIENT_HPP_

#include "ConsensusClient.hpp"
#include "lib/lib.hpp"
#include "socket/Socket.hpp"

class RaftClient : public ConsensusClient {
 public:
	RaftClient(vector<server_t *> &servers_arr);
	~RaftClient();

	bool Connect();
	bool SendRequest(ILogEntry *log_entry);
	bool GetResponse(IResponse *resp);

 private:
	Socket *sock_;
	bool sock_connected_;
	vector<server_t *> servers_;
};

#endif // __RAFT_CLIENT_HPP_
