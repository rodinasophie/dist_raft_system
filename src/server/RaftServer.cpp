#include "RaftServer.hpp"
#include "state_machine/MyStateMachine.hpp"
#include "log/MyLogEntry.hpp"
#include "socket/UnixSocket.hpp"

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
	receiver_ = 0;
	rpc_ = NULL;
	rq_rpc_ = new RequestVoteRPC();
	ae_rpc_ = new AppendEntryRPC();
	log_ = new Log();
	log_entry_ = NULL;
	timer_ = new Timer(std::chrono::milliseconds(elect_timeout_));
	for (size_t i = 0; i < servers_.size(); ++i) {
		next_idx_.push_back(0);
		match_idx_.push_back(0);
	}
	sm_ = new MyStateMachine();
}

RaftServer::~RaftServer() {
	if (sfd_serv_for_serv_)
		delete sfd_serv_for_serv_;

	if (sfd_serv_for_client_)
		delete sfd_serv_for_client_;

	for (size_t i = 0; i < servs_as_clients_.size(); ++i) {
		delete servs_as_clients_[i];
	}
	delete timer_;
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
	sfd_serv_for_client_->Bind(stoi(servers_[me]->port_client));

	if (me < servers_.size() - 1) {
		sfd_serv_for_serv_ = new UnixSocket();
		sfd_serv_for_serv_->Bind(stoi(servers_[me]->port_serv));
	}
	size_t CONNECT_TRIES = 10;
	size_t j = 1, count = 1;
	for (size_t i = 0; i < me; ++i) {
		servs_as_clients_.push_back(new UnixSocket());
		while (count != CONNECT_TRIES) {
			if (!servs_as_clients_.back()->Connect(servers_[me - j]->ip_addr,
						servers_[me - j]->port_serv)) {
				sleep(1);
				++count;
			} else {
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
			if (!sfd_serv_for_serv_->AcceptIncomings()) {
				continue;
			} else {
				++sock_connected;
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
				voted_for_ = 0;
				cur_term_++;
				state_ = CANDIDATE;
				++voted_for_;
				ILogEntry *le = log_->GetLast();
				size_t idx = 0, term = 0;
				if (le) {
					idx = le->GetIndex();
					term = le->GetTerm();
				}
				rq_rpc_->SetData(id_, cur_term_, idx, term);
				SendRPC(*rq_rpc_);
				timer_->Run();
				//start = high_resolution_clock::now();
			}
			string mes = "";
			sfd_serv_for_client_->AcceptIncomings();
			if (!log_entry_) {
				sfd_serv_for_client_->Recv(mes);
			}
			if (mes == "leader") {
				std::stringstream ss;
				ss << leader_id_;
				sfd_serv_for_client_->Send(ss.str());
				continue;
			}

			if (state_ == LEADER) {
				if (mes != "") {
					log_entry_ = new MyLogEntry(mes, log_indx_++, cur_term_);
					log_->Add(log_entry_);
				}
				size_t prev_term = 0,
							 prev_idx = 0;
				ILogEntry *le = log_->GetPrevLast();
				if (le) {
					prev_term = le->GetTerm();
					prev_idx = le->GetIndex();
				}
				ae_rpc_->SetData(id_, cur_term_, prev_idx, prev_term, commit_idx_, mes);
				SendRPC(*ae_rpc_);
				if (mes == "") {
					continue;
				}
				std::cout << mes;
				mes = "OK!";
				sfd_serv_for_client_->Send(mes);
			}
				//std::cout << "Beginning\n";
				//std::cout << "\nHere\n";
				//std::cout<< "Term and index set\n";
				//std::cout << "Added!\n";
				//std::cout << "Log entry was added."<<"Getting it: \n";

			/*if (WE_CAN_APPLY_TO_SM) {
			}*/
		}
		//sfd_serv_for_serv_->AcceptIncomings();
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
		cur_term_ = another_term;
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
			log_->SetConsistency(false);
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
	if (commit_idx_ < rpc->GetLeaderCommitIdx()) {
		commit_idx_ = std::min(rpc->GetLeaderCommitIdx(), log_->GetLast()->GetIndex());
	}
	while (last_applied_ < commit_idx_) {
		++last_applied_;
		sm_->Apply(log_->Search(last_applied_));
	}

	if ((message = rpc->GetLogData()) != "" ) {
		if (log_->IsConsistent()) {
			// XXX:!!! PrevLogIndx - here is a new entry from replicas
			ILogEntry *le = new MyLogEntry(rpc->GetLogData(), rpc->GetPrevLogTerm(),
					rpc->GetPrevLogIdx());
			log_->Add(le);
			return;
		}
		if ((!rpc->GetPrevLogIdx()) ||
				(log_->Search(rpc->GetPrevLogTerm(), rpc->GetPrevLogIdx()))) {
			// intial stage: no entries in leader's log
			// or logs are consistent
			log_->Delete(); // deletes the tail of incorrect entries
			s += "+,";
			log_->SetConsistency(true);
		} else {
			s += "-,";
		}
		s += ss.str();
		SendResponse(s);
	}
}

void RaftServer::SendRPC(RPC &rpc) {
	if (sfd_serv_for_serv_) {
		sfd_serv_for_serv_->SendToAll(rpc.ToSend());
	}
	for (size_t i = 0; i < servs_as_clients_.size(); ++i) {
		servs_as_clients_[i]->Send(rpc.ToSend());
	}
}

bool RaftServer::ReceiveRPC() {
	size_t cluster_size = servers_.size();
	string message;
	bool mes_got = false;
	if (sfd_serv_for_serv_) {
		if (sfd_serv_for_serv_->Recv(message)) {
			mes_got = true;
			sock_ = sfd_serv_for_serv_;
		}
	}
	if (!mes_got) {
		for (size_t i = 0; i < servs_as_clients_.size(); ++i) {
			if (servs_as_clients_[i]->Recv(message)) {
				sock_ = servs_as_clients_[i];
				receiver_ = i;
				mes_got = true;
				break;
			}
		}
	}
	rpc_ = NULL;
	size_t term;
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
				// !A,+,term
				term = stoi(message.substr(5));
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
								next_idx_[i] = log_indx_;// FIXME: what idx?
								match_idx_[i] = 0;
							}
							string str("");
							ILogEntry *le = log_->GetPrevLast();
							size_t prev_idx = 0,
										 prev_term = 0;
							if (le) {
								prev_term = le->GetTerm();
								prev_idx = le->GetIndex();
							}
							ae_rpc_->SetData(id_, cur_term_, prev_idx, prev_term, commit_idx_, str);
							SendRPC(*ae_rpc_); // establishing authority
							std::cout << "I'm a leader! " <<id_ << "\n";
						}
					}
				} else {
				// AppendEntry Resp
					if (message[3] == '+') {
						if (!log_->GetLast()) {
							return true;
						}
						while (next_idx_[receiver_] <= log_->GetLast()->GetIndex()) {
							match_idx_[receiver_] = next_idx_[receiver_] - 1;
							std::cout << "Here not null should be!\n";
							MyLogEntry *le = (MyLogEntry*)log_->Search(next_idx_[receiver_]);
							ae_rpc_->SetData(id_, cur_term_, le->GetIndex(), le->GetTerm(), commit_idx_,
									le->GetLogData());
							std::cout << "It seems really not NULL\n";
							next_idx_[receiver_]++;
							SendResponse(ae_rpc_->ToSend());
						}
						size_t replicated = 0;
						for (size_t i = 0; i < match_idx_.size(); ++i) {
							if (match_idx_[i] == log_->GetLast()->GetIndex()) {
								replicated++;
							}
						}
						if (replicated > (cluster_size / 2)) {
							commit_idx_++; // FIXME: ? or equal to log_->GetLast()->GetIndex()
							sm_->Apply(log_->GetLast());
						}
					} else {
						next_idx_[receiver_]--;
						size_t idx = 0,
									 term = 0;
						string str = "";
						MyLogEntry *le = (MyLogEntry *)log_->Search(next_idx_[receiver_] - 1);
						if (le) {
							idx = le->GetIndex();
							term = le->GetTerm();
							str = le->GetLogData();
						}
						ae_rpc_->SetData(id_, cur_term_, idx, term, commit_idx_, str);
						SendResponse(ae_rpc_->ToSend());
					}
				}
		}
		return true;
	}
	return false;
}
