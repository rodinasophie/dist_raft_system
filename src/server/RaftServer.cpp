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
				std::cout<<"I wanna be a leader\n";
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
				std::cout<<"Before\n";
				SendRPC(*rq_rpc_);
				std::cout<<"After\n";
				timer_->Run();
			}
			string mes = "";
			sfd_serv_for_client_->AcceptIncomings();
			if (!log_entry_) {
				sfd_serv_for_client_->Recv(mes);
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
				}
				size_t prev_term = 0,
							 prev_idx = 0;
				ILogEntry *le = log_->GetPrevLast();
				if (le) {
					prev_term = le->GetTerm();
					prev_idx = le->GetIndex();
				}
				if (log_entry_) {
					std::cout<<"COMMIT IDX = "<<log_entry_->GetIndex()<<"\n";
					if (commit_idx_ == log_entry_->GetIndex()) {
						sfd_serv_for_client_->Send(resp_);
						resp_ = "";
						log_entry_ = NULL;
					}
				}
				ae_rpc_->SetData(id_, cur_term_, prev_idx, prev_term, commit_idx_, mes);
				SendRPC(*ae_rpc_);
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
			log_->SetConsistent(false);
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
		std::cout << "Last_app = "<<last_applied_<<", commit_idx = "<<commit_idx_<<"\n";
	}
	while (last_applied_ < commit_idx_) {
		++last_applied_;
		ILogEntry *le = NULL;
		if ((le = log_->Search(last_applied_))) {
			sm_->Apply(le);
			std::cout << "Applying to sm\n";
		} else {
			--last_applied_;
			break;
		}
	}
	//if ((message = rpc->GetLogData()) != "" ) {
		/*if ((log_->IsConsistent()) && (last_applied_ != commit_idx_)) {
			// XXX:!!! PrevLogIndx - here is a new entry from replicas
			ILogEntry *le = new MyLogEntry(rpc->GetLogData(), rpc->GetPrevLogTerm(),
					rpc->GetPrevLogIdx());
			std::cout << "Added to log\n";
			log_->Add(le);
			return;
		}*/
		// We are copying log entries but heartbeat is got
		if ((last_applied_ != commit_idx_) && (rpc->GetLogData() == "")) {
			return;
		}
		if ((!rpc->GetPrevLogIdx()) || // FIXME: here is problem
				(log_->Search(rpc->GetPrevLogTerm(), rpc->GetPrevLogIdx()))) {
			// intial stage: no entries in leader's log
			// or logs are consistent
			log_->Delete(); // deletes the tail of incorrect entries
			if (rpc->GetLogData() != "") {
				std::cout<<"Before "<<rpc->GetLogData()<<"\n";

				counted_ptr<ILogEntry> cptr(new MyLogEntry(rpc->GetLogData(),
						rpc->GetTransmitterTerm(), rpc->GetPrevLogIdx() + 1));
				std::cout<<"After getting\n";
				log_->Add(cptr);
				log_indx_ = rpc->GetPrevLogIdx() + 2;
				std::cout << "Added to log2\n";
			}
			s += "+,";
			log_->SetConsistent(true);
		} else {
			s += "-,";
		}
		s += ss.str();
		ss.str("");
		ss << id_;
		s += "," + ss.str();
		SendResponse(s);
	//}

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
	string message = "";
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
				mes_got = true;
				break;
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
							std::cout<<"Sending "<<ae_rpc_->ToSend()<<"\n";
							SendRPC(*ae_rpc_); // establishing authority
							std::cout << "I'm a leader! " <<id_ << "\n";
						}
					}
				} else {
				// AppendEntry Resp
					//std::cout << "Receive resp: "<<message<<"\n";
					if (message[3] == '+') {
						if (!log_->GetLast()) {
							return true;
						}
						size_t pos = message.find_last_of(",");
						receiver = stoi(message.substr(pos + 1));

						//std::cout<<"next_idx[recv] = "<<next_idx_[receiver]<<
							//		", log_->last_idx = "<<log_->GetLast()->GetIndex()<<"\n";
						if (next_idx_[receiver] == log_->GetLast()->GetIndex()) {
							std::cout<<"receiver == "<<receiver<<"\n";
							match_idx_[receiver] = next_idx_[receiver];
							++next_idx_[receiver];
						} else {
							if (next_idx_[receiver] <= log_->GetLast()->GetIndex()) {
								std::cout<<"next_idx[recv] = "<<next_idx_[receiver]<<
									", log_->last_idx = "<<log_->GetLast()->GetIndex()<<"\n";
								match_idx_[receiver] = next_idx_[receiver];
								MyLogEntry *le = (MyLogEntry*)log_->Search(next_idx_[receiver] - 1);
								ae_rpc_->SetData(id_, cur_term_, le->GetIndex(), le->GetTerm(), commit_idx_,
										le->GetLogData());
								next_idx_[receiver]++;
								SendResponse(ae_rpc_->ToSend());
							}
						}
						if (commit_idx_ == log_->GetLast()->GetIndex())
							return true;
						size_t replicated = 1; // intially leader has already replicated
						for (size_t i = 0; i < match_idx_.size(); ++i) {
							std::cout<<"SERVER: for id = "<< i<<" match_idx = "<<match_idx_[i]<<"\n";
							if (match_idx_[i] == log_->GetLast()->GetIndex()) {
								replicated++;
							}
						}
						std::cout<<"Replicated at "<<replicated<<"\n";
						if (replicated > (cluster_size / 2)) {
							commit_idx_++; // FIXME: ? or equal to log_->GetLast()->GetIndex()
							resp_ = sm_->Apply(log_->GetLast());
							std::cout << "Leader: applying to sm, commit_idx_ = "<<commit_idx_<<"\n";
						}
					} else {
						next_idx_[receiver]--;
						size_t idx = 0,
									 term = 0;
						string str = "";
						MyLogEntry *le = (MyLogEntry *)log_->Search(next_idx_[receiver] - 1);
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
