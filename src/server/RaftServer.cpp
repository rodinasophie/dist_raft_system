#include "RaftServer.hpp"
#include "state_machine/MyStateMachine.hpp"
#include "log/MyLogEntry.hpp"
#include "socket/UnixSocket.hpp"
#include "lib/counted_ptr.hpp"
#include <thread>

RaftServer::RaftServer(size_t id, vector<server_t *> &servers) : cur_term_(0),
		state_(FOLLOWER), servers_(servers), sfd_serv_for_serv_(NULL),
		sfd_serv_for_client_(new UnixSocket()), commit_idx_(0),
		last_applied_(0), id_(id) {
	voted_for_ = 0;
	i_voted_ = false;
	log_indx_ = 1;
	sock_ = NULL;
	srand(time(NULL));
	elect_timeout_ = 150 + rand() % 51;  // 150 - 300 ms
	// << "My timeout is "<<elect_timeout_<<"\n";
	last_snapshot_idx_ = 0;
	last_snapshot_term_ = 0;
	resp_ = "";
	rpc_ = NULL;
	rq_rpc_ = new RequestVoteRPC();
	ae_rpc_ = new AppendEntryRPC();
#ifdef _SNAPSHOTTING
	insn_rpc_ = new InstallSnapshotRPC();
#endif
	is_snapshotting_ = false;
	log_ = new Log();
	log_entry_ = NULL;
	timer_ = new Timer(std::chrono::milliseconds(elect_timeout_));
	for (size_t i = 0; i < servers_.size(); ++i) {
		next_idx_.push_back(0);
		match_idx_.push_back(0);
		non_empty_le_.push_back(false);
	}
	sm_ = new MyStateMachine();
}

RaftServer::~RaftServer() {
	if (sfd_serv_for_serv_)
		delete sfd_serv_for_serv_;

	if (sfd_serv_for_client_)
		delete sfd_serv_for_client_;

	for (size_t i = 0; i < servs_as_clients_.size(); ++i) {
		if (servs_as_clients_[i])
			delete servs_as_clients_[i];
	}

	if (timer_)
		delete timer_;

	if (log_)
		delete log_;

	if (rq_rpc_)
		delete rq_rpc_;

	if (ae_rpc_)
		delete ae_rpc_;

	if (insn_rpc_)
		delete insn_rpc_;

	if (sm_)
		delete sm_;
};

size_t RaftServer::SetId() {
	string ip = UnixSocket::GetExternalIP();
	for (size_t i = 0; i < servers_.size(); ++i) {
		if (servers_[i]->ip_addr == ip)
			return servers_[i]->id;
	}
	const size_t INCORRECT_ID = 1000;
	return INCORRECT_ID;
}

void RaftServer::Run() {
	 //XXX: Uncomment for really distributed testing
	 //id_ = SetId();
#ifdef _SNAPSHOTTING
		filename_ = "/home/sonya/dist_raft_system/snapshot" + std::to_string(id_);
		ofstream f;
		f.open(filename_);

		 //TODO
		if (false) {
			string snapshot;
			f >> snapshot;
			InitSMFromSnapshot(snapshot);
		}
#endif  //_SNAPSHOTTING
	size_t me = servers_.size();
	for (size_t i = 0; i < servers_.size(); ++i) {
		if (servers_[i]->id == id_) {
			me = i;
			break;
		}
	}
	if (me == servers_.size()) {
		 //TODO: Set logging: ERROR: No suitable server!!!
		return;
	}
	sfd_serv_for_client_->Bind(servers_[me]->ip_addr,
				stoi(servers_[me]->port_client));
	if (me < servers_.size() - 1) {
		sfd_serv_for_serv_ = new UnixSocket();
		sfd_serv_for_serv_->Bind(servers_[me]->ip_addr, stoi(servers_[me]->port_serv));
	}
	size_t CONNECT_TRIES = 10;
	size_t j = 1, count = 1;
	for (size_t i = 0; i < me; ++i) {
		servs_as_clients_.push_back(new UnixSocket());
		while (count != CONNECT_TRIES) {
			int sfd;
			if ((sfd = servs_as_clients_.back()->Connect(servers_[me]->ip_addr,
							servers_[me - j]->ip_addr, servers_[me - j]->port_serv)) < 0) {
				std::chrono::seconds(1);
				++count;
			} else {
				servers_[me - j]->sfd = sfd;
				count = 1;
				break;
			}
		}
		if (count == CONNECT_TRIES) {
			 //TODO: Set logging: ERROR, server was not binded, cant work!
			return;
		}
		++j;
	}
	size_t sock_to_connect = servers_.size() - me - 1,
				 sock_connected = 0;
	while (sock_to_connect != sock_connected) {
		if (sfd_serv_for_serv_) {
			string ip_addr;
			int sfd;
			if ((sfd = sfd_serv_for_serv_->AcceptIncomings(ip_addr)) >= 0) {
				for (size_t i = 0; i < servers_.size(); ++i) {
					if (ip_addr == servers_[i]->ip_addr) {
						servers_[i]->sfd = sfd;
						++sock_connected;
						break;
					}
				}
				continue;
			}
		}
	}

	// << "server #" << id_ << "has established connection" << std::endl;
	 //XXX: Here connection already should be established between all servers

	string message;
	// Finding the leader
	timer_->Run();
	while (1) {
		rpc_ = NULL;
		if (ReceiveRPC()) {
			if (rpc_) {
				rpc_->Act(this);
			}
			timer_->Run();
		}
		 //Initiate new election
		if ((timer_->TimedOut()) && (state_ != LEADER)) {
			//<<"I wanna be a leader\n";
			voted_for_ = 0;
			cur_term_++;
			state_ = CANDIDATE;
			++voted_for_;
			i_voted_ = true;
			SendRPCToAll(REQUEST_VOTE);
			timer_->Run();
		}
		string mes = "";
			for (auto it = shutted_servers_.begin(); it != shutted_servers_.end();) {
				int sfd;
				if ((sfd = servs_as_clients_[*it]->Connect(
								servers_[me]->ip_addr,
								servers_[me - (*it + 1)]->ip_addr,
								servers_[me - (*it + 1)]->port_serv)) >= 0) {
					servers_[me - (*it + 1)]->sfd = sfd;
					shutted_servers_.erase(it++);
					//<<"Erased\n";
				} else {
					++it;
				}
			}
			string ip_addr;
			if (sfd_serv_for_serv_) {
				int sfd = sfd_serv_for_serv_->AcceptIncomings(ip_addr);
				if (sfd >= 0) {
					for (size_t i = 0; i < servers_.size(); ++i) {
						if (ip_addr == servers_[i]->ip_addr) {
							servers_[i]->sfd = sfd;
							//<<"\n\nServer "<<i <<" is restored\n\n";
						}
					}
				}
			}
			if (sfd_serv_for_client_->AcceptIncomings(ip_addr) >= 0) {
				//<<"New client connection\n";
			}
			if (!log_entry_) {
				try {
					if (sfd_serv_for_client_->Recv(mes) > 0 ) {
						//<<"mes from client\n";
					}
				} catch (exception &e) {
					//<<"Client is dead\n";
					 //client is dead, it doesn't matter
				}
			}
			if (mes != "") {
				// << "We received from client "<<mes<<"\n";
			}
			if (mes == "leader") {
				std::stringstream ss;
				ss << leader_id_;
				//<<"Sending leader "<< leader_id_<<"\n";
				sfd_serv_for_client_->Send(ss.str());
				continue;
			}

			if (state_ == LEADER) {
				if (mes != "") {
					//<<"\nAdding log entry with idx = "<<log_indx_<<"\n\n";
					log_entry_ = new MyLogEntry(mes, cur_term_, log_indx_++);
					counted_ptr<ILogEntry> cptr(log_entry_);
					log_->Add(cptr);
					match_idx_[me] = log_entry_->GetIndex();
				}
				if (log_entry_) {
					 //we have committed the last client entry
					if (commit_idx_ == log_entry_->GetIndex()) {
						sfd_serv_for_client_->Send(resp_);
						resp_ = "";
						log_entry_ = NULL;
					}
				}
				if (mes != "")
					SendRPCToAll(APPEND_ENTRY, false);
				else
					SendRPCToAll(APPEND_ENTRY, true);
			}
#ifdef _SNAPSHOTTING
			size_t size;
			if (((size = log_->GetLengthInBytes()) >= MAX_LOG_SIZE_) &&
					(!is_snapshotting_.load(std::memory_order_relaxed))) {
				//<<"Making snapshot\n";
				is_snapshotting_ = true;
				std::thread t(&RaftServer::CreateSnapshot, this);
				t.detach();
			}
#endif
		}
}

void RaftServer::CreateSnapshot() {
	//<<"Begin1\n";
	size_t last_snapshot_idx = commit_idx_,
				 last_snapshot_term = log_->Search(commit_idx_)->GetTerm();
	 //while snapshotting operations at sm are not possible
	string snsht = sm_->CreateSnapshot(filename_);
	//<<"Begin2\n";
	mtx_.Lock();
	size_t from = log_->GetFirst()->GetIndex();
	//<<"Begin3\n";
	log_->Delete(from, last_snapshot_idx);
	//<<"Begin4\n";
	last_snapshot_idx_ = last_snapshot_idx;
	last_snapshot_term_ = last_snapshot_term;
	is_snapshotting_ = false;
	mtx_.Unlock();
	//<<"Begin5\n";
	snapshot_.Set(snsht);
}

void RaftServer::SendResponse(std::string resp) {
	if (sock_) {
		sock_->Send(resp);
	}
	sock_ = NULL;
}

 //FIXME: Maybe - are not neccessary
void RaftServer::ActWhenRequestVote() {
	size_t another_term = rpc_->GetTransmitterTerm();
	string resp = "";
	std::stringstream ss;
	if ((another_term > cur_term_) ||
			((another_term == cur_term_) && (!i_voted_))) {
		if (another_term > cur_term_)
			state_ = FOLLOWER;
		cur_term_ = another_term;
		i_voted_ = false;
		ss << cur_term_;
		 //TODO: check that logs are as up-to-date as it is possible
		rq_rpc_ = (RequestVoteRPC*)rpc_;
		ILogEntry *le = log_->GetLast();
		size_t idx = 0, term = 0;
		if (le) {
			term = le->GetTerm();
			idx = le->GetIndex();
		}
		if ((rq_rpc_->GetLastLogTerm() > term) ||
				((rq_rpc_->GetLastLogTerm() == term) &&
				 (rq_rpc_->GetLastLogIdx() >= idx))) {
			resp = "!R,+," + ss.str();
			i_voted_ = true;
		}
	}
	if (resp == "") {
		ss << cur_term_;
		resp = "!R,-," + ss.str();
	}
	SendResponse(resp);
}

void RaftServer::ActWhenAppendEntry() {
	size_t another_term = rpc_->GetTransmitterTerm();
	//reject if stale leader
	if (another_term < cur_term_) {  //>=
		return;
	} else if (another_term > cur_term_) {
		cur_term_ = another_term;
		state_ = FOLLOWER;
	}
	leader_id_ = rpc_->GetTransmitterId();
	//<<"New leader "<<leader_id_<<" for id="<<id_<<"\n";
	string message;
	std::stringstream ss;
	string s = "!A,";
	AppendEntryRPC *rpc = (AppendEntryRPC *)rpc_;
	ss << cur_term_;

	//Update commit_idx
	if (commit_idx_ < rpc->GetLeaderCommitIdx()) {
		ILogEntry *le = log_->GetLast();
		if (!le) {  //empty log
			commit_idx_ = rpc->GetLeaderCommitIdx();
		} else {
			commit_idx_ = std::min(rpc->GetLeaderCommitIdx(), le->GetIndex());
		}
	}

	while (last_applied_ < commit_idx_) {
		ILogEntry *le = NULL;
		if ((le = log_->Search(last_applied_ + 1))) {
			sm_->Apply(le);
			++last_applied_;
			// << "Applying to sm\n";
		} else {
			break;
		}
	}
	 //Leader's log is empty
	 //Previous entry is found, adding current entry
#ifdef _SNAPSHOTTING
	mtx_.Lock();
#endif
	if ((!rpc->GetPrevLogIdx()) ||
			(log_->Search(rpc->GetPrevLogTerm(), rpc->GetPrevLogIdx())) ||
#ifdef _SNAPSHOTTING
			((rpc->GetPrevLogIdx() <= last_snapshot_idx_) &&
			 (rpc->GetPrevLogTerm() <= last_snapshot_term_))) {
#else
			(0)) {
#endif  //_SNAPSHOTTING
		//deletes the tail of incorrect entries, only if matching entries are found
		ILogEntry *le_last = log_->GetLast(),
							*le_first = log_->GetFirst();
		if (le_last) {
			if ((rpc->GetPrevLogIdx() <= last_snapshot_idx_) &&
					(rpc->GetPrevLogTerm() <= last_snapshot_term_)) {
				//discard the whole log
				if (le_first)
					log_->Delete(le_first->GetIndex(), le_last->GetIndex());
			} else if (rpc->GetPrevLogIdx()) {
				ILogEntry *le = log_->Search(rpc->GetPrevLogIdx());
				if (le)
					log_->Delete(le->GetIndex() + 1, le_last->GetIndex());
			}
		}
		if (rpc->GetLogData() != "") {
			counted_ptr<ILogEntry> cptr(new MyLogEntry(rpc->GetLogData(),
				rpc->GetLogEntryTerm(), rpc->GetLogEntryIdx()));
			log_->Add(cptr);
			log_indx_ = rpc->GetLogEntryIdx() + 1;
			// << "Added to log2\n";
			//<<"Log indx == "<<log_indx_<<"\n";
		}
		s += "+,";
	} else {
		//<<"Going to send - from "<<id_<<", prevlog="<<rpc->GetPrevLogIdx()<<", prevterm="<< rpc->GetPrevLogTerm()<<"\n";
		s += "-,";
	}
#ifdef _SNAPSHOTTING
	mtx_.Unlock();
#endif  //_SNAPSHOTTING
	s += ss.str();
	ss.str("");
	ss << id_;
	s += "," + ss.str();
	SendResponse(s);
}

void RaftServer::SendRPCToAll(Rpc type, bool is_empty) {
	string entry = "";
	if (type == APPEND_ENTRY) {
		if (sfd_serv_for_serv_) {
			for (size_t i = id_ + 1; i < servers_.size(); ++i) {
				if (sfd_serv_for_serv_->SetReceiver(servers_[i]->sfd)) {
					size_t prev_idx = next_idx_[i] - 1;
					if (non_empty_le_[i])
						continue;
#ifdef _SNAPSHOTTING
					 //we have snapshotted already
					if (snapshot_.Get() != "") {
						 //не путать с пустым логом
						if (!log_->Search(next_idx_[i]) && (next_idx_[i] < (last_snapshot_idx_ + 1))) {
							mtx_.Lock();
							 //TODO: Set partitioning of the log, and send all parts
							 //while () {}
							 //XXX: Now the whole snapshot is sent at one time
							insn_rpc_->SetData(id_, cur_term_, last_snapshot_idx_,
									last_snapshot_term_, 0, snapshot_.Get(), true);
							next_idx_[i] = last_snapshot_idx_ + 1;
							//<<"Sending InstallSnapshot\n";
							sfd_serv_for_serv_->Send(insn_rpc_->ToSend());
							non_empty_le_[i] = true;
							mtx_.Unlock();
							continue;
						}
					}
#endif  //_SNAPSHOTTING
					ILogEntry *le = log_->Search(prev_idx);
					size_t prev_term;
					prev_term = (le) ? le->GetTerm() : 0;
					le = NULL;
#ifdef _SNAPSHOTTING
					mtx_.Lock();
					if (prev_idx == last_snapshot_idx_) {
						prev_term = last_snapshot_term_;
					}
					mtx_.Unlock();
#endif  //_SNAPSHOTTING
					le = log_->Search(prev_idx + 1);
					int log_term = 0,
							log_idx = 0;

					if (le) {
						log_term = le->GetTerm();
						log_idx = le->GetIndex();
					}
					if (!is_empty) {
						non_empty_le_[i] = true;
						if (!le) {
							//<<"Bad Error: prev_term = "<<prev_term<<", prev_idx = "<<prev_idx<<"\n";
						}
						entry = le->GetLogData();
					}
					ae_rpc_->SetData(id_, cur_term_, prev_idx, prev_term, commit_idx_,
							log_term, log_idx, entry);
					sfd_serv_for_serv_->Send(ae_rpc_->ToSend());
				}
			}
		}
		for (size_t i = 0; i < servs_as_clients_.size(); ++i) {
			if (shutted_servers_.find(id_ - (i + 1)) != shutted_servers_.end()) {
				continue;
			}
			size_t prev_idx = next_idx_[id_ - (i + 1)] - 1; // id_ == me
			if (non_empty_le_[id_ - (i + 1)])
				continue;
#ifdef _SNAPSHOTTING
			if (snapshot_.Get() != "") { // we have snapshotted already
				 //TODO: if we restore after crashing don't forget to set snapshot_
				if (!log_->Search(next_idx_[id_ - (i + 1)]) && (next_idx_[id_ - (i + 1)] < (last_snapshot_idx_ + 1))) {
					mtx_.Lock();
					 //TODO: Set partitioning of the log, and send all parts
					 //while () {}
					insn_rpc_->SetData(id_, cur_term_, last_snapshot_idx_,
							last_snapshot_term_, 0, snapshot_.Get(), true);
					next_idx_[id_ - (i + 1)] = last_snapshot_idx_ + 1;
					servs_as_clients_[i]->Send(insn_rpc_->ToSend());
					non_empty_le_[id_ - (i + 1)] = true;
					mtx_.Unlock();
					continue;
				}
			}
#endif  //_SNAPSHOTTING

			ILogEntry *le = log_->Search(prev_idx);
			size_t prev_term;
			prev_term = (le) ? le->GetTerm() : 0;
#ifdef _SNAPSHOTTING
			mtx_.Lock();
			if (prev_idx == last_snapshot_idx_) {
				prev_term = last_snapshot_term_;
			}
			mtx_.Unlock();
#endif  //_SNAPSHOTTING
			le = NULL;
			le = log_->Search(prev_idx + 1);
			int log_term = 0,
					log_idx = 0;
			if (le) {
				log_term = le->GetTerm();
				log_idx = le->GetIndex();
			}
			if (!is_empty) {
				non_empty_le_[id_ - (i + 1)] = true;
				entry = le->GetLogData();
			}
			ae_rpc_->SetData(id_, cur_term_, prev_idx, prev_term, commit_idx_,
							log_term, log_idx, entry);
			servs_as_clients_[i]->Send(ae_rpc_->ToSend());
		}
	} else if (type == REQUEST_VOTE) {
		ILogEntry *le = log_->GetLast();
		size_t idx = 0,
					 term = 0;
		if (le) {
			idx = le->GetIndex();
			term = le->GetTerm();
		}
		rq_rpc_->SetData(id_, cur_term_, idx, term);
		if (sfd_serv_for_serv_) {
			sfd_serv_for_serv_->SendToAll(rq_rpc_->ToSend());
		}
		for (size_t i = 0; i < servs_as_clients_.size(); ++i) {
			servs_as_clients_[i]->Send(rq_rpc_->ToSend());
		}
	} else {
		 // INSTALL_SNAPSHOT
#ifdef _SNAPSHOTTING
		 //XXX: nothing to do
#endif
	}
}

void RaftServer::InitSMFromSnapshot(string snapshot) {
	vector<string> key_values;
	size_t pos = 0, fir = 0;
	while ((pos = snapshot.find('|', fir)) != string::npos) {
		key_values.push_back(snapshot.substr(fir, pos - 1));
		fir = pos + 1;
	}
	for (size_t i = 0; i < key_values.size(); ++i) {
		pos = key_values[i].find(':');
		sm_->Apply(new MyLogEntry(ADD, key_values[i].substr(0, pos - 1),
				key_values[i].substr(pos + 1)));
	}
}

void RaftServer::ActWhenInstallSnapshot() {
	 //TODO: normally it should depend on whether
	//or not all parts of snapshot are transmitted
	//<<"Install snapshot received\n";
	InstallSnapshotRPC *rpc = (InstallSnapshotRPC *)rpc_;
	if (!log_->Search(rpc->GetLastInclTerm(), rpc->GetLastInclIdx())) {
		//discard all log
		snapshot_.Set(rpc->GetRawData());
		ILogEntry *le_first = log_->GetFirst(),
							*le_last = log_->GetLast();
		if ((le_first) && (le_last)) {
			size_t from = le_first->GetIndex(),
						 to = le_last->GetIndex();
			log_->Delete(from, to);
		}
	} else {
	  //discard from beginning to last_incl_idx_
		snapshot_.Set(rpc->GetRawData());
		ILogEntry *le = log_->GetFirst();
		if (le) {
			size_t from = le->GetIndex(),
						 to = rpc->GetLastInclIdx();
			log_->Delete(from, to);
		}
	}
	//reset sm from snapshot
	sm_->Reset();
	 //TODO: do this after restart
	InitSMFromSnapshot(snapshot_.Get());
	 //XXX: Linux specific call!!!
	//writing snapshot to file
	pid_t pid = fork();
	if (pid < 0) {
		 //well, that's a problem
	}
	if (pid == 0) { //child pid
		ofstream f;
		f.open(filename_);
		f << snapshot_.Get();
		f.close();
	}
	string resp = "!I," + std::to_string(cur_term_);
	SendResponse(resp);
}


bool RaftServer::ReceiveRPC() {
	size_t cluster_size = servers_.size();
	string message = "";
	bool mes_got = false;
	if (sfd_serv_for_serv_) {
		try {
			if (sfd_serv_for_serv_->Recv(message)) {
				mes_got = true;
				sock_ = sfd_serv_for_serv_;
			}
		} catch (exception &e) {
			//<<"server client is dead\n";
			//one of clients is dead, accept him
		}
	}
	if (!mes_got) {
		for (size_t i = 0; i < servs_as_clients_.size(); ++i) {
			if (shutted_servers_.find(i) != shutted_servers_.end()) {
				continue;
			}
			try {
				if (servs_as_clients_[i]->Recv(message)) {
					sock_ = servs_as_clients_[i];
					mes_got = true;
					break;
				}
			} catch (exception &e) {
				//we need to connect again from time to time
				shutted_servers_.insert(i);
				non_empty_le_[id_ - (i + 1)] = false;
				servs_as_clients_[i]->Reset();
				//<<"server server is dead\n";
				continue;
			}
		}
	}
	rpc_ = NULL;
	size_t term;
	size_t receiver;
	if (mes_got) {
		switch (message[0]) {
			case 'R':
				rq_rpc_->SetData(message);
				rpc_ = rq_rpc_;
				break;
			case 'A':
				ae_rpc_->SetData(message);
				rpc_ = ae_rpc_;
				break;
#ifdef _SNAPSHOTTING
			case 'I':
				insn_rpc_->SetData(message);
				rpc_ = insn_rpc_;
				break;
#endif
			case '!':  //this is response from follower
				//!R,+,term
				//!A,+,term,id
				//!I,term
				if (message[1] == 'R') {
					term = stoi(message.substr(5));
				} else if (message[1] == 'A') {
					size_t pos = message.find(",", 5);
					term = stoi(message.substr(5, pos - 5));
				} else {
					term = stoi(message.substr(3));
				}
				if (term > cur_term_) {
					state_ = FOLLOWER;
					break;
				}
				if (message[1] == 'R') {
					if (state_ == CANDIDATE) {
						if (message[3] == '+') {
							voted_for_++;
						}
						if (voted_for_ > (cluster_size / 2)) {
							 //XXX: Here new leader is chosen
							state_ = LEADER;
							leader_id_ = id_;
							//size_t log_length = log_->GetLength();

							for (size_t i = 0; i < next_idx_.size(); ++i) {
								next_idx_[i] = log_indx_;
								match_idx_[i] = 0;
								non_empty_le_[i] = false;
							}
							SendRPCToAll(APPEND_ENTRY, true);//  establishing authority
							// << "I'm a leader! " <<id_ << "\n";
						}
					}
				} else if (message[1] == 'A') {
				 //AppendEntry Resp
					receiver = stoi(message.substr(message.find_last_of(",") + 1));//  this info is redundant, id isn't neccessary

					if (message[3] == '+') {
						ILogEntry *le = log_->GetLast();
						if (!le) {  //empty log, only heartbeats
							return true;
						}
						 //if non-empty append entry was sent
						if (non_empty_le_[receiver]) {
							match_idx_[receiver] = next_idx_[receiver];
							next_idx_[receiver]++;
							non_empty_le_[receiver] = false;
							if (next_idx_[receiver] <= le->GetIndex()) {
								SendAppendEntry(receiver);
							}
						}
						if (commit_idx_ == le->GetIndex())
							return true;
						size_t replicated = 1; // intially leader has already replicated
						for (size_t i = 0; i < match_idx_.size(); ++i) {
							if (match_idx_[i] == le->GetIndex()) {
								replicated++;
							}
						}
						if (replicated > (cluster_size / 2)) {
							commit_idx_++;  //FIXME: ? or equal to log_->GetLast()->GetIndex()
							//<<"Replicated\n";
							resp_ = sm_->Apply(le);
						}
					} else {
						next_idx_[receiver]--;
						SendAppendEntry(receiver);
					}
				} else {
					 //InstallSnapshot response
					receiver = stoi(message.substr(message.find_last_of(",") + 1));  //this info is redundant, id isn't neccessary
					non_empty_le_[receiver] = false;
					 //XXX: do nothing here, only terminfo is useful(handled above)
				}
				break;
		}
		return true;
	}
	return false;
}

void RaftServer::SendAppendEntry(size_t receiver) {
	size_t prev_idx = 0,
				 prev_term = 0,
				 log_en_idx = 0,
				 log_en_term = 0;
	string str = "";  //empty mes before logs matching entries are not found
	MyLogEntry *le = (MyLogEntry *)log_->Search(next_idx_[receiver] - 1);
	//InstallSnapshot
	if (le) {
		prev_idx = le->GetIndex();
		prev_term = le->GetTerm();
	}
	le = NULL;
	//<<"next_idx_[receover] = "<<next_idx_[receiver]<<"\n";
	le = (MyLogEntry *)log_->Search(next_idx_[receiver]);
	if (le) {
		log_en_idx = le->GetIndex();
		log_en_term = le->GetTerm();
		str = le->GetLogData();
	}
	ae_rpc_->SetData(id_, cur_term_, prev_idx, prev_term, commit_idx_,
			log_en_term, log_en_idx, str);
	SendResponse(ae_rpc_->ToSend());
	if (str != "")
		non_empty_le_[receiver] = true;
}

/*void RaftServer::SendInstallSnapshot(size_t receiver) {
	insn_rpc_->SetData(id_, cur_term_, last_snapshot_idx_,
			last_snapshot_term_, 0, snapshot_, done = true);
	next_idx_[i] = last_snapshot_idx_ + 1;
	sfd_serv_for_serv_->Send(insn_rpc_->ToSend());
}*/
