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
    size_t GetLeaderId();
    bool GetNewLeader();

 private:
    Socket *sock_;
    size_t leader_id_;
    bool sock_connected_;
    vector<server_t *> servers_;
    const int MAX_CONNECT_TRIES_ = 20;
};

#endif // __RAFT_CLIENT_HPP_
