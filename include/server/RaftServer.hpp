#ifndef __RAFT_SERVER_HPP_
#define __RAFT_SERVER_HPP_

#include "ConsensusServer.hpp"
#include "log/Log.hpp"
#include "timer/Timer.hpp"
#include "socket/UnixSocket.hpp"
#include "lib/lib.hpp"
#include "include/Mutex.hpp"
#include "include/ThreadSafeString.hpp"
#include "state_machine/IStateMachine.hpp"
#include <set>
#include <atomic>
using std::set;

class RPC;
class RequestVoteRPC;
class AppendEntryRPC;
class InstallSnapshotRPC;

class RaftServer : public ConsensusServer {
 public:
    RaftServer(size_t id, vector<server_t *> &servers);
    ~RaftServer();

    void Run();

    void ActWhenRequestVote();
    void ActWhenAppendEntry();
    void ActWhenInstallSnapshot();

 private:
    size_t cur_term_;
    enum {
        FOLLOWER,
        CANDIDATE,
        LEADER,
    } state_;
    vector<server_t *> servers_;
    Socket *sfd_serv_for_serv_,         // as server for other servers
           *sfd_serv_for_client_;       // as server for clients(./client)
    vector<Socket *> servs_as_clients_; // as client for other servers

    Socket *sock_;

    size_t commit_idx_,
        last_applied_,
        id_,
        leader_id_,
        voted_for_,
        log_indx_,
        elect_timeout_,
        last_snapshot_idx_,
        last_snapshot_term_;
    std::string resp_, filename_;
    bool i_voted_;
    std::atomic<bool> is_snapshotting_;
    high_resolution_clock::time_point start;
    vector<size_t> next_idx_;
    vector<size_t> match_idx_;
    vector<bool> non_empty_le_;
    set <size_t> shutted_servers_;
    RPC *rpc_;
    RequestVoteRPC *rq_rpc_;
    AppendEntryRPC *ae_rpc_;
    InstallSnapshotRPC *insn_rpc_;
    Log *log_;
    ILogEntry *log_entry_;
    Timer *timer_;
    IStateMachine *sm_;

    ThreadSafeString snapshot_;
    Mutex mtx_;
    const size_t MAX_LOG_SIZE_ = 100; // TODO: set it wisely

    enum Rpc {
        APPEND_ENTRY,
        REQUEST_VOTE,
        INSTALL_SNAPSHOT,
    };

    size_t SetId();
    bool ReceiveRPC();
    void SendRPCToAll(Rpc type, bool is_empty = true);
    void SendResponse(std::string resp);
    void SendAppendEntry(size_t to);
    void SendInstallSnapshot(size_t to);
    void InitSMFromSnapshot(string snapshot);
    void CreateSnapshot();
};

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
 using RPC::SetData;
 public:
    AppendEntryRPC() {}
    void SetData(size_t id, size_t cur_term, size_t prev_log_idx,
            size_t prev_log_term, size_t commit_idx, size_t log_en_term,
            size_t log_en_idx, string &log_record) {
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

        ss.str("");
        log_en_term_ = log_en_term;
        ss << log_en_term_;
        data_ += "," + ss.str();

        ss.str("");
        log_en_idx_ = log_en_idx;
        ss << log_en_idx_;
        data_ += "," + ss.str();

        log_record_ = log_record;
    }
    ~AppendEntryRPC() {}

    void SetData(string mes) {
        // A,id,term,prev_log_idx,prev_log_term,commit_idx,
        // log_entry_term, log_entry_idx,log_entry
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
        first = pos + 1;
        pos = mes.find(",", pos + 1);
        log_en_term_ = stoi(mes.substr(first, pos - first));
        first = pos + 1;
        pos = mes.find(",", pos + 1);
        log_en_idx_ = stoi(mes.substr(first, pos - first));
        log_record_ = mes.substr(pos + 1);
    }

    string ToSend() {
        // XXX: Should not be called after SetData(mes) initialization(data_ not set)
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

    size_t GetLogEntryTerm() {
        return log_en_term_;
    }

    size_t GetLogEntryIdx() {
        return log_en_idx_;
    }

    size_t GetLeaderCommitIdx() {
        return commit_idx_;
    }
 private:
    std::string log_record_;
    size_t prev_log_idx_,
        prev_log_term_,
        log_en_term_,
        log_en_idx_,
        commit_idx_;
};

class RequestVoteRPC : public RPC {
 using RPC::SetData;
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
        // XXX: Should not be called after SetData(mes) initialization(data_ not set)
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
    std::mutex mtx_;
};

class InstallSnapshotRPC : public RPC {
 using RPC::SetData;
 public:
    InstallSnapshotRPC() {}
    ~InstallSnapshotRPC() {}
    // Packing
    // I,id,cur_term,last_incl_idx,last_incl_term,offset,raw_data,done
    void SetData(size_t id, size_t cur_term, size_t last_incl_idx,
            size_t last_incl_term, size_t offset, const string raw_data, bool done) {
        RPC::SetData(id, cur_term);
        std::stringstream ss;
        last_incl_idx_ = last_incl_idx;
        ss << last_incl_idx_;
        data_ += "," + ss.str();

        ss.str("");
        last_incl_term_ = last_incl_term;
        ss << last_incl_term_;
        data_ += "," + ss.str();

        ss.str("");
        offset_ = offset;
        ss << offset_;
        data_ += "," + ss.str();

        data_ += "," + raw_data;

        ss.str("");
        done_ = done;
        ss << done_;
        data_ += "," + ss.str();
        // TODO: raw data is missing
    }
    // Unpacking
    void SetData(string mes) {
        // I,id,cur_term,last_incl_idx,last_incl_term,offset,raw_data,done
        size_t pos = mes.find(",");
        pos = mes.find(",", pos + 1);
        id_ = stoi(mes.substr(2, pos - 2));
        size_t fir = pos + 1;
        pos = mes.find(",", pos + 1);
        cur_term_ = stoi(mes.substr(fir, pos - fir));
        fir = pos + 1;
        pos = mes.find(",", pos + 1);
        last_incl_idx_ = stoi(mes.substr(fir, pos - fir));
        fir = pos + 1;
        pos = mes.find(",", pos + 1);
        last_incl_term_ = stoi(mes.substr(fir, pos - fir));
        fir = pos + 1;
        pos = mes.find(",", pos + 1);
        offset_ = stoi(mes.substr(fir, pos - fir));
        fir = pos + 1;
        pos = mes.find(",", pos + 1);
        raw_data_ = mes.substr(fir, pos - fir);
        done_ = stoi(mes.substr(pos + 1));
    }

    string ToSend() {
        // XXX: Should not be called after SetData(mes)
        // initialization(data_ not set)
        return "I," + data_;
    }

    void Act(RaftServer *raftserver) {
        raftserver->ActWhenInstallSnapshot();
    }

    size_t GetLastInclIdx() {
        return last_incl_idx_;
    }

    size_t GetLastInclTerm() {
        return last_incl_term_;
    }

    size_t GetSnapshotOffset() {
        return offset_;
    }

    bool ShapshotIsDone() {
        return done_;
    }

    string GetRawData() {
        return raw_data_;
    }
 private:
    size_t last_incl_idx_, last_incl_term_,
        offset_;
    bool done_;
    string raw_data_;
};

#endif // __RAFT_SERVER_HPP_
