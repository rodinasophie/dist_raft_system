#ifndef __RAFT_SERVER_HPP_
#define __RAFT_SERVER_HPP_

#include "ConsensusServer.hpp"
#include "log/Log.hpp"
#include "timer/Timer.hpp"
#include "socket/UnixSocket.hpp"
#include "lib/lib.hpp"
#include "state_machine/IStateMachine.hpp"

class RPC;

class RaftServer : public ConsensusServer {
 public:
	RaftServer(size_t id, vector<server_t *> &servers);
	~RaftServer();

	void Run();

	void ActWhenRequestVote();
	void ActWhenAppendEntry();

 private:
	int cur_term_;
	enum {
		FOLLOWER,
		CANDIDATE,
		LEADER,
	} state_;

	vector<server_t *> servers_;
	Socket *sfd_serv_for_serv_,        // as server for other servers
				 *sfd_serv_for_client_;      // as server for clients(./client)
	vector<Socket *> servs_as_clients_; // as client for other servers

	Socket *sock_;

	size_t elect_timeout_,
				 last_committed_,
				 last_applied_, // FIXME: Do we really need it?
				 id_, voted_for_;

	int leader_id_;
	bool i_voted_;
	vector<size_t> next_idx_;
	vector<size_t> match_idx_;

	RPC *rpc_;
	Log *log_;
	Timer *timer_;
	IStateMachine *sm_;

	bool ReceiveRPC(RPC* &rpc);
	void SendRPC(RPC &rpc);
	void SendResponse(std::string &resp);
};

// RPC protocol:
// 1. current_term, leader|candidate id
//

class RPC {
 public:
	RPC(size_t id, int cur_term) : id_(id), cur_term_(cur_term) {
		std::stringstream ss_id, ss_cur_term;
		ss_id << id_;
		ss_cur_term << cur_term_;
		data_ = ss_id.str() + "," + ss_cur_term.str();
	};
	RPC() {}
	virtual ~RPC() {}
	int GetTransmitterId() {
		return id_;
	}
	int GetTransmitterTerm() {
		return cur_term_;
	}

	virtual string ToSend() = 0;
	virtual void Act(RaftServer *raftserver) = 0;

 protected:
	size_t id_, cur_term_;
	string data_;
};

class AppendEntryRPC : public RPC {
 public:
	AppendEntryRPC(size_t id, int cur_term, string &log_record) : RPC(id, cur_term) {
		log_record_ = log_record;
	}

	AppendEntryRPC(string mes) {
		// A,id,term,log_entry
		size_t pos = mes.find(",");
		pos = mes.find(",", pos + 1);
		id_ = stoi(mes.substr(2, pos - 2));
		size_t first = pos + 1;
		pos = mes.find(",", pos + 1);
		cur_term_ = stoi(mes.substr(first, pos - first));
		log_record_ = mes.substr(pos + 1);
	}

	~AppendEntryRPC() {}
	string ToSend() {
		return "A," + data_ + "," + log_record_;
	}
	void Act(RaftServer *raftserver) {
		raftserver->ActWhenAppendEntry();
	}
	string &GetLogData() {
		return log_record_;
	}
 private:
	std::string log_record_;
};

class RequestVoteRPC : public RPC {
 public:
	RequestVoteRPC(size_t id, int cur_term) : RPC(id, cur_term) {}
	RequestVoteRPC(string mes) {
		// R,id,term
		std::cout << mes<<"\n";
		size_t pos = mes.find(",");
		pos = mes.find(",", pos + 1);
		id_ = stoi(mes.substr(2, pos - 2));
		cur_term_ = stoi(mes.substr(pos + 1));
	}
	~RequestVoteRPC() {}
	void Act(RaftServer *raftserver) {
		raftserver->ActWhenRequestVote();
	}
	string ToSend() {
		return "R," + data_;
	}
};

#endif // __RAFT_SERVER_HPP_
