#include "RaftServer.hpp"
#include "state_machine/MyStateMachine.hpp"
#include "log/MyLogEntry.hpp"
#include "socket/UnixSocket.hpp"
#include <errno.h>

RaftServer::RaftServer(size_t id, vector<server_t *> &servers) : cur_term_(0),
		state_(FOLLOWER), servers_(servers), sfd_serv_for_serv_(NULL),
		sfd_serv_for_client_(new UnixSocket()), last_committed_(0),
		last_applied_(0), id_(id) {
	leader_id_ = -1;
	voted_for_ = 0;
	i_voted_ = false;
	sock_ = NULL;
	srand(time(NULL));
	elect_timeout_ = 150 + rand() % 150; // 150 - 300 ms
	timer_ = new Timer(elect_timeout_);
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

	// std::cout << "server #" << id_ << "has established connection" << std::endl;
	// XXX: Here connection already should be established between all servers

	string message;
	//size_t voted_for_ = 0;

	// Finding the leader
	timer_->Run();
	while (1) {
		//std::cout << leader_id_ << std::endl;
		/*if (ReceiveRPC(rpc_)) {
			if (rpc_)
				rpc_->Act(this);
			timer_->Run();
		} else {
			// Initiate new election
			if (timer_->TimedOut()) {
				cur_term_++;
				state_ = CANDIDATE;
				++voted_for_;
				RequestVoteRPC rpc(id_, cur_term_);
				SendRPC(rpc);
				timer_->Run();
			}

			if (WE_CAN_APPLY_TO_SM) {
				sm_->Apply();
			}*/
			string mes;
			// Client's requests
			sfd_serv_for_client_->AcceptIncomings();
			if (!sfd_serv_for_client_->Recv(mes)) {
				continue;
			}
			std::stringstream ss;
			ss << leader_id_;
			sfd_serv_for_client_->Send(ss.str());
			/*ILogEntry *log_entry = new MyLogEntry();
				log_entry->SetData(mes);
				log_entry->SetIndex(0); // TODO: indices are not set yet
				log_entry->SetTerm(cur_term_);
				log_->Add(log_entry);
				if (leader_id_ == (int)id_) {
					AppendEntryRPC rpc(id_, cur_term_, mes);
					SendRPC(rpc);
			}*/
		//}
		}
		//sfd_serv_for_serv_->AcceptIncomings();
}

void RaftServer::SendResponse(std::string &resp) {
	if (sock_)
		sock_->Send(resp);
	sock_ = NULL;
}

void RaftServer::ActWhenRequestVote() {
	int another_term = rpc_->GetTransmitterTerm();
	if ((another_term > cur_term_) ||
			((another_term == cur_term_) && (!i_voted_))) {
		cur_term_ = another_term;
		string resp;
		std::stringstream ss;
		ss << cur_term_;
		resp = "+," + ss.str();
		SendResponse(resp);
		i_voted_ = true;
	} else {
		string resp;
		std::stringstream ss;
		ss << cur_term_;
		resp = "-," + ss.str();
		SendResponse(resp);
	}
}

void RaftServer::ActWhenAppendEntry() {
	int another_term = rpc_->GetTransmitterTerm();
	if (another_term < cur_term_)
		return;
	string message;
	AppendEntryRPC *rpc = (AppendEntryRPC *)rpc_;
	// Another server wants to establish authority
	if ((message = rpc->GetLogData()) == "") {
		leader_id_ = rpc->GetTransmitterId();
		cur_term_ = another_term;
		state_ = FOLLOWER;
	} else {
		ILogEntry *log_entry = new MyLogEntry();
		log_entry->SetData(message);
		log_->Add(log_entry);
		// TODO: understand when log entry is committed and apply to state machine
		// sm_->Apply();
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

bool RaftServer::ReceiveRPC(RPC *rpc) {
	size_t cluster_size = servers_.size();
	string message;
	rpc = NULL;
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
	int term;
	if (mes_got) {
		switch (message[0]) {
			case 'R':
				rpc = new RequestVoteRPC(message);
				break;
			case 'A':
				rpc = new AppendEntryRPC(message);
				break;
			case '+':
				term = stoi(message.substr(2));
				if (term > cur_term_) {
					state_ = FOLLOWER;
					break;
				}
				if (state_ == CANDIDATE) {
					voted_for_++;
					if (voted_for_ > (cluster_size / 2)) {
						state_ = LEADER;
						leader_id_ = id_;
						string str("");
						AppendEntryRPC rpc1(id_, cur_term_, str);
						SendRPC(rpc1); // establishing authority
					}
				}
				break;
			case '-':
				term = stoi(message.substr(2));
				if (term > cur_term_) {
					state_ = FOLLOWER;
					break;
				}
				break;
		}
		return true;
	}
	return false;
}
