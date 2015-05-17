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
	string my_ip = "";
	for (size_t i = 0; i < servers_.size(); ++i) {
		server = servers_[i];
		if (sock_->Connect(my_ip, server->ip_addr, server->port_client) < 0) {
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
			int recv_count = 0;
			while (recv_count != MAX_CONNECT_TRIES_) {
				try {
					if (!sock_->Recv(rsp)) {
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
						++recv_count;
					} else {
						std::cout<<"Recved\n";
						break;
					}
				} catch (std::exception &e) {}
			}
			if (recv_count == MAX_CONNECT_TRIES_) {
				count = 0;
				std::cout<<"Leader doesnt answer\n";
				continue; // our serv has shutted down, connect to another server
			}
			int id = stoi(rsp);
			std::cout<<"id = "<<id<<"\n";
			// server doesn't know who the leader is
			if (id < 0) {
				++count;
				if (count == MAX_CONNECT_TRIES_) { // leader is not chosen
					return false;
				}
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
					std::cout<<"Connecting to ip " <<servers_[j]->ip_addr<<", port = "<<servers_[j]->port_client<<"\n";
					if (sock_->Connect(my_ip, servers_[j]->ip_addr, servers_[j]->port_client) < 0) {
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
