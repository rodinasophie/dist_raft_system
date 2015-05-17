#include "RaftServer.hpp"
#include "state_machine/MyStateMachine.hpp"
#include "log/MyLogEntry.hpp"
#include "socket/UnixSocket.hpp"
#include "lib/counted_ptr.hpp"

RaftServer::RaftServer(size_t id, vector<server_t *> &servers) : cur_term_(0),
		state_(FOLLOWER), servers_(servers), sfd_serv_for_serv_(NULL),
		sfd_serv_for_client_(new UnixSocket()), commit_idx_(0),
		last_applied_(0), id_(id) {
	voted_for_ = 0;
	i_voted_ = false;
	log_indx_ = 1;
	sock_ = NULL;
	srand(time(NULL));
	elect_timeout_ = 150 + rand() % 150; // 150 - 300 ms
	std::cout << "My timeout is "<<elect_timeout_<<"\n";
	resp_ = "";
	rpc_ = NULL;
	rq_rpc_ = new RequestVoteRPC();
	ae_rpc_ = new AppendEntryRPC();
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

	if (sm_)
		delete sm_;
};

void RaftServer::Run() {
	size_t me = servers_.size();
	for (size_t i = 0; i < servers_.size(); ++i) {
		if (servers_[i]->id == id_) {
			me = i;
			break;
		}
	}
	if (me == servers_.size()) {
		// TODO: Set logging: ERROR: No suitable server!!!
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
			// TODO: Set logging: ERROR, server was not binded, cant work!
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

	std::cout << "server #" << id_ << "has established connection" << std::endl;
	// XXX: Here connection already should be established between all servers

	string message;
	// Finding the leader
	timer_->Run();
	while (1) {
		if (ReceiveRPC()) {
			if (rpc_) {
				rpc_->Act(this);
			}
			timer_->Run();
		} else {
			// Initiate new election
			if ((timer_->TimedOut()) && (state_ != LEADER)) {
				//std::cout<<"I wanna be a leader\n";
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
				//std::cout<<"Here\n";
				int sfd;
				if ((sfd = servs_as_clients_[*it]->Connect(
								servers_[me]->ip_addr,
								servers_[me - (*it + 1)]->ip_addr,
								servers_[me - (*it + 1)]->port_serv)) >= 0) {
					servers_[me - (*it + 1)]->sfd = sfd;
					shutted_servers_.erase(it++);
					std::cout<<"Erased\n";
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
							std::cout<<"\n\nServer "<<i <<" is restored\n\n";
						}
					}
				}
			}
			if (sfd_serv_for_client_->AcceptIncomings(ip_addr) >= 0)
				std::cout<<"New client connection\n";
			if (!log_entry_) {
				try {
					if (sfd_serv_for_client_->Recv(mes) > 0 ) {
						std::cout<<"mes from client\n";
					}
				} catch (exception &e) {
					std::cout<<"Client is dead\n";
					// client is dead, it doesn't matter
				}
			}
			if (mes != "")
				std::cout << "We received from client "<<mes<<"\n";
			if (mes == "leader") {
				std::stringstream ss;
				ss << leader_id_;
				std::cout<<"Sending leader "<< leader_id_<<"\n";
				sfd_serv_for_client_->Send(ss.str());
				continue;
			}

			if (state_ == LEADER) {
				if (mes != "") {
					std::cout<<"\nAdding log entry with idx = "<<log_indx_<<"\n\n";
					log_entry_ = new MyLogEntry(mes, cur_term_, log_indx_++);
					counted_ptr<ILogEntry> cptr(log_entry_);
					log_->Add(cptr);
					match_idx_[me] = log_entry_->GetIndex();
				}
				if (log_entry_) {
					// we have committed the last client entry
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
				if (mes == "") {
					continue;
				}
			}
		}
	}
}

void RaftServer::SendResponse(std::string resp) {
	if (sock_) {
		sock_->Send(resp);
	}
	sock_ = NULL;
}

// FIXME: Maybe - are not neccessary
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
		// TODO: check that logs are as up-to-date as it is possible
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
	//std::cout<<"app_en\n";
	size_t another_term = rpc_->GetTransmitterTerm();
	// reject if stale leader
	if (another_term < cur_term_) { // >=
		return;
	} else if (another_term > cur_term_) {
		cur_term_ = another_term;
		state_ = FOLLOWER;
	}
	leader_id_ = rpc_->GetTransmitterId();
	string message;
	std::stringstream ss;
	string s = "!A,";
	AppendEntryRPC *rpc = (AppendEntryRPC *)rpc_;
	ss << cur_term_;

	// Update commit_idx
	if (commit_idx_ < rpc->GetLeaderCommitIdx()) {
		if (!log_->GetLast()) { // empty log
			commit_idx_ = rpc->GetLeaderCommitIdx();
		} else {
			commit_idx_ = std::min(rpc->GetLeaderCommitIdx(), log_->GetLast()->GetIndex());
		}
	}

	while (last_applied_ < commit_idx_) {
		ILogEntry *le = NULL;
		if ((le = log_->Search(last_applied_ + 1))) {
			sm_->Apply(le);
			++last_applied_;
			std::cout << "Applying to sm\n";
		} else {
			break;
		}
	}
	// Leader's log is empty
	// Previous entry is found, adding current entry
	if ((!rpc->GetPrevLogIdx()) || (log_->Search(rpc->GetPrevLogTerm(), rpc->GetPrevLogIdx()))) {
		log_->Delete(); // deletes the tail of incorrect entries, only if matching entries are found
		if (rpc->GetLogData() != "") {
			counted_ptr<ILogEntry> cptr(new MyLogEntry(rpc->GetLogData(),
				rpc->GetLogEntryTerm(), rpc->GetLogEntryIdx()));
			log_->Add(cptr);
			log_indx_ = rpc->GetLogEntryIdx() + 1;
			std::cout << "Added to log2\n";
			std::cout<<"Log indx == "<<log_indx_<<"\n";
		}
		s += "+,";
	} else {
		s += "-,";
	}
	s += ss.str();
	ss.str("");
	ss << id_;
	s += "," + ss.str();
	SendResponse(s);
}

void RaftServer::SendRPCToAll(Rpc type, bool is_empty) {
	string entry = "";
	if (type == APPEND_ENTRY) {
		//std::cout<<"Sending RPC\n";
		if (sfd_serv_for_serv_) {
			for (size_t i = id_ + 1; i < servers_.size(); ++i) {
				if (sfd_serv_for_serv_->SetReceiver(servers_[i]->sfd)) {
					int prev_idx = next_idx_[i] - 1; // XXX: i == id in this impl
					ILogEntry *le = log_->Search(prev_idx);
					int prev_term;
					prev_term = (le) ? le->GetTerm() : 0;
					le = NULL;
					le = log_->Search(prev_idx + 1);
					int log_term = 0,
							log_idx = 0;
					if (non_empty_le_[i])
						continue;
					if (le) {
						log_term = le->GetTerm();
						log_idx = le->GetIndex();
					}
					if (!is_empty) {
						non_empty_le_[i] = true;
						entry = le->GetLogData();
					}
					ae_rpc_->SetData(id_, cur_term_, prev_idx, prev_term, commit_idx_,
							log_term, log_idx, entry);
					sfd_serv_for_serv_->Send(ae_rpc_->ToSend());
				}
			}
		}
		for (size_t i = 0; i < servs_as_clients_.size(); ++i) {
			int prev_idx = next_idx_[id_ - (i + 1)] - 1; // id_ == me
			ILogEntry *le = log_->Search(prev_idx);
			int prev_term;
			prev_term = (le) ? le->GetTerm() : 0;
			le = NULL;
			le = log_->Search(prev_idx + 1);
			int log_term = 0,
					log_idx = 0;
			if (non_empty_le_[i])
				continue;
			if (le) {
				log_term = le->GetTerm();
				log_idx = le->GetIndex();
			}
			if (!is_empty) {
				non_empty_le_[i] = true;
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

	}
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
			std::cout<<"server client is dead\n";
			// one of clients is dead, accept him
		}
	}
	if (!mes_got) {
		for (size_t i = 0; i < servs_as_clients_.size(); ++i) {
			if (shutted_servers_.find(i) != shutted_servers_.end()) {
				//std::cout<<"continue "<<i<<"\n";
				continue;
			}
			try {
				if (servs_as_clients_[i]->Recv(message)) {
					sock_ = servs_as_clients_[i];
					mes_got = true;
					break;
				}
			} catch (exception &e) {
				// we need to connect again from time to time
				shutted_servers_.insert(i);
				non_empty_le_[id_ - (i + 1)] = false;
				servs_as_clients_[i]->Reset();
				std::cout<<"server server is dead\n";
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
			case '!': // this is response from follower
				// !R,+,term
				// !A,+,term,id
				if (message[1] == 'R') {
					term = stoi(message.substr(5));
				} else {
					size_t pos = message.find(",", 5);
					term = stoi(message.substr(5, pos - 5));
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
							// XXX: Here new leader is chosen
							state_ = LEADER;
							leader_id_ = id_;
							//size_t log_length = log_->GetLength();

							for (size_t i = 0; i < next_idx_.size(); ++i) {
								next_idx_[i] = log_indx_;
								match_idx_[i] = 0;
								non_empty_le_[i] = false;
							}
							SendRPCToAll(APPEND_ENTRY, true); // establishing authority
							std::cout << "I'm a leader! " <<id_ << "\n";
						}
					}
				} else {
				// AppendEntry Resp
					receiver = stoi(message.substr(message.find_last_of(",") + 1)); // this info is redundant, id isn't neccessary

					if (message[3] == '+') {
						if (!log_->GetLast()) { // empty log, only heartbeats
							return true;
						}
						// if non-empty append entry was sent
						if (non_empty_le_[receiver]) {
							match_idx_[receiver] = next_idx_[receiver];
							next_idx_[receiver]++;
							non_empty_le_[receiver] = false;
							if (next_idx_[receiver] <= log_->GetLast()->GetIndex()) {
								SendAppendEntry(receiver);
							}
						}
						if (commit_idx_ == log_->GetLast()->GetIndex())
							return true;
						size_t replicated = 1; // intially leader has already replicated
						for (size_t i = 0; i < match_idx_.size(); ++i) {
							if (match_idx_[i] == log_->GetLast()->GetIndex()) {
								replicated++;
							}
						}
						if (replicated > (cluster_size / 2)) {
							commit_idx_++; // FIXME: ? or equal to log_->GetLast()->GetIndex()
							std::cout<<"Replicated\n";
							resp_ = sm_->Apply(log_->GetLast());
						}
					} else {
						next_idx_[receiver]--;
						SendAppendEntry(receiver);
					}
				}
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
	string str = ""; // empty mes before logs matching entries are not found
	MyLogEntry *le = (MyLogEntry *)log_->Search(next_idx_[receiver] - 1);
	if (le) {
		prev_idx = le->GetIndex();
		prev_term = le->GetTerm();
	}
	le = NULL;
	std::cout<<"next_idx_[receover] = "<<next_idx_[receiver]<<"\n"; 
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
