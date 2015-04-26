#ifndef __RAFT_SERVER_HPP_
#define __RAFT_SERVER_HPP_

#include "ConsensusServer.hpp"
#include "log/Log.hpp"
#include "timer/Timer.hpp"
#include "socket/UnixSocket.hpp"
#include "lib/lib.hpp"
#include "state_machine/IStateMachine.hpp"

class RPC;
class RequestVoteRPC;
class AppendEntryRPC;

class RaftServer : public ConsensusServer {
 public:
	RaftServer(size_t id, vector<server_t *> &servers);
	~RaftServer();

	void Run();

	void ActWhenRequestVote();
	void ActWhenAppendEntry();

 private:
	size_t cur_term_;
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

	size_t commit_idx_,
				 last_applied_,
				 id_,
				 leader_id_,
				 voted_for_,
				 log_indx_,
				 elect_timeout_,
				 receiver_;

	bool i_voted_;
	high_resolution_clock::time_point start;
	vector<size_t> next_idx_;
	vector<size_t> match_idx_;

	RPC *rpc_;
	RequestVoteRPC *rq_rpc_;
	AppendEntryRPC *ae_rpc_;
	Log *log_;
	ILogEntry *log_entry_;
	Timer *timer_;
	IStateMachine *sm_;

	bool ReceiveRPC();
	void SendRPC(RPC &rpc);
	void SendResponse(std::string resp);
};

// RPC protocol:
// 1. current_term, leader|candidate id
//

class RPC {
 public:
	RPC() {}
	virtual ~RPC() {}
	virtual void SetData(size_t id, size_t cur_term) {
		id_ = id;
		cur_term_ = cur_term;
		std::stringstream ss_id, ss_cur_term;
		ss_id << id_;
		ss_cur_term << cur_term_;
		data_ = ss_id.str() + "," + ss_cur_term.str();
	};
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
	AppendEntryRPC() {}
	void SetData(size_t id, size_t cur_term, size_t prev_log_idx,
			size_t prev_log_term, size_t commit_idx, string &log_record) {
		RPC::SetData(id, cur_term);
		std::stringstream ss;
		prev_log_idx_ = prev_log_idx;
		ss << prev_log_idx_;
		data_ += "," + ss.str();
		ss.str("");
		prev_log_term_ = prev_log_term;
		ss << prev_log_term_;
		data_ += "," + ss.str();
		ss.str("");
		commit_idx_ = commit_idx;
		ss << commit_idx_;
		data_ += "," + ss.str();
		log_record_ = log_record;
	}
	~AppendEntryRPC() {}

	void SetData(string mes) {
		// A,id,term,prev_log_idx,prev_log_term,commit_idx,log_entry
		size_t pos = mes.find(",");
		pos = mes.find(",", pos + 1);
		id_ = stoi(mes.substr(2, pos - 2));
		size_t first = pos + 1;
		pos = mes.find(",", pos + 1);
		cur_term_ = stoi(mes.substr(first, pos - first));
		first = pos + 1;
		pos = mes.find(",", pos + 1);
		prev_log_idx_ = stoi(mes.substr(first, pos - first));
		first = pos + 1;
		pos = mes.find(",", pos + 1);
		prev_log_term_ = stoi(mes.substr(first, pos - first));
		first = pos + 1;
		pos = mes.find(",", pos + 1);
		commit_idx_ = stoi(mes.substr(first, pos - first));
		pos = mes.find(",", pos + 1);
		log_record_ = mes.substr(pos + 1);
	}

	string ToSend() {
		return "A," + data_ + "," + log_record_;
	}

	void Act(RaftServer *raftserver) {
		raftserver->ActWhenAppendEntry();
	}

	string &GetLogData() {
		return log_record_;
	}

	size_t GetPrevLogIdx() {
		return prev_log_idx_;
	}

	size_t GetPrevLogTerm() {
		return prev_log_term_;
	}

	size_t GetLeaderCommitIdx() {
		return commit_idx_;
	}
 private:
	std::string log_record_;
	size_t prev_log_idx_,
				 prev_log_term_,
				 commit_idx_;
};

class RequestVoteRPC : public RPC {
 public:
	RequestVoteRPC() {}
	~RequestVoteRPC() {}
	void SetData(size_t id, size_t cur_term, size_t last_log_idx,
			size_t last_log_term) {
		RPC::SetData(id, cur_term);
		std::stringstream ss;
		last_log_idx_ = last_log_idx;
		ss << last_log_idx_;
		data_ += "," + ss.str();
		last_log_term_ = last_log_term;
		ss.str("");
		ss << last_log_term_;
		data_ += "," + ss.str();
	}
	void SetData(string mes) {
		// R,id,term,last_log_idx,last_log_term
		size_t pos = mes.find(",");
		std::cout << "Sending " <<mes<<"\n";
		pos = mes.find(",", pos + 1);
		id_ = stoi(mes.substr(2, pos - 2));
		size_t fir = pos + 1;
		pos = mes.find(",", pos + 1);
		cur_term_ = stoi(mes.substr(fir, pos - fir));
		fir = pos + 1;
		pos = mes.find(",", pos + 1);
		last_log_idx_ = stoi(mes.substr(fir, pos - fir));
		last_log_term_ = stoi(mes.substr(pos + 1));
	}

	void Act(RaftServer *raftserver) {
		raftserver->ActWhenRequestVote();
	}

	string ToSend() {
		return "R," + data_;
	}

	size_t GetLastLogIdx() {
		return last_log_idx_;
	}

	size_t GetLastLogTerm() {
		return last_log_term_;
	}

 private:
	size_t last_log_idx_,
				 last_log_term_;
};

#endif // __RAFT_SERVER_HPP_
