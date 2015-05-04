#include "client/RaftClient.hpp"
#include "socket/UnixSocket.hpp"
#include <thread>
#include <chrono>

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

	server_t *server;
	int count = 0;
	for (size_t i = 0; i < servers_.size(); ++i) {
		server = servers_[i];
		if (!sock_->Connect(server->ip_addr, server->port_client)) {
			continue;
		}
		string leader = "leader",
					 rsp;
		std::cout << "Connected in client\n";
		while (1) {
			if (!sock_->Send(leader)) {
				return false;
			} else {
				std::cout<<"Sent asking leader\n";
			}
			while (!sock_->Recv(rsp)) {
			}
			int id = stoi(rsp);
			if (id < 0) {
				++count;
				if (count == MAX_CONNECT_TRIES_) { // leader is not chosen
					return false;
				}
				std::cout<<"Sleeping\n";
				std::this_thread::sleep_for(std::chrono::seconds(2));
				continue;
			}
			std::cout<<"I got message "<< id<<"\n";
			if (server->id == (size_t)id) {
				sock_connected_ = true;
				std::cout << "My leader is "<<id<<"\n";
				return true;
			}
			for (size_t j = 0; j < servers_.size(); ++j) {
				if (servers_[j]->id == (size_t)id) {
					delete sock_;
					sock_ = new UnixSocket();
					if (!sock_->Connect(servers_[j]->ip_addr, servers_[j]->port_client)) {
						return false;
					}
					server = servers_[j];
					break;
				}
			}
		}
	}
	return false;
}

bool RaftClient::SendRequest(ILogEntry *log_entry) {
	if (!sock_connected_)
		return false;
	int count = 0;
	while (1) {
		if (sock_->Send(log_entry->ToSend())) {
			std::cout << "We sent request\n";
			return true;
		}
		// if we cannot send message to server,
		// trying to reconnect and start sending
		delete sock_;
		sock_ = new UnixSocket();
		if (!Connect()) {
			++count;
			if (count == MAX_CONNECT_TRIES_) {
				return false;
			}
			std::this_thread::sleep_for(std::chrono::seconds(2));
			continue;
		}
	}
}

bool RaftClient::GetResponse(IResponse *resp) {
	if (!sock_connected_)
		return false;
	string response = "";
	if (!sock_->Recv(response))
		return false;
	resp->SetData(response);
	return true;
}
