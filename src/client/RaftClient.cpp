#include "client/RaftClient.hpp"
#include "socket/UnixSocket.hpp"

RaftClient::RaftClient(vector<server_t *> &servers_arr) : sock_connected_(false), servers_(servers_arr) {
	sock_ = new UnixSocket();
}

RaftClient::~RaftClient() {
	if (sock_)
		delete sock_;
}

bool RaftClient::Connect() {
	if (!sock_)
		return false;
	size_t ileader = 0;
	for (size_t i = 0; i < servers_.size(); ++i) {
		if (!sock_->Connect(servers_[i]->ip_addr, servers_[i]->port_client)) {
			continue;
		}
		string leader = "leader",
					 rsp;
		if (!sock_->Send(leader)) {
			return false;
		}
		sock_->Recv(rsp);
		int id = stoi(rsp);
		if (id < 0) {
			continue;
		}
		if (servers_[i]->id == (size_t)id) {
			sock_connected_ = true;
			return true;
		}
		for (size_t j = 0; j < servers_.size(); ++j) {
			if (servers_[j]->id == (size_t)id) {
				ileader = j;
				break;
			}
		}
		if (!sock_->Connect(servers_[ileader]->ip_addr, servers_[ileader]->port_client)) {
			return false;
		}
		break;
	}
	return false;
}

bool RaftClient::SendRequest(ILogEntry *log_entry) {
	if (!sock_connected_)
		return false;
	while (1) {
		if (sock_->Send(log_entry->ToSend())) {
			return true;
		}
		delete sock_;
		sock_ = new UnixSocket();
		if (!Connect()) {
			return false;
		}
	}
}

bool RaftClient::GetResponse(IResponse *resp) {
	if (!sock_connected_)
		return false;
	string response;
	if (!sock_->Recv(response))
		return false;
	resp->SetData(response);
	return true;
}
