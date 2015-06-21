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
		//// << "Connected in client\n";
		while (1) {
			if (!sock_->Send(leader)) {
				return false;
			} else {
				//<<"Sent asking leader from "<<i<<"\n";
			}
			int recv_count = 0;
			while (recv_count != 200) {
				try {
					if (!sock_->Recv(rsp)) {
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
						++recv_count;
					} else {
						////<<"Recved\n";
						break;
					}
				} catch (std::exception &e) {
					//<<"Exception happened\n";
					return false;
				}
			}
			if (recv_count == 200) {
				count = 0;
				//<<"Leader doesnt answer\n";
				continue; // our serv has shutted down, connect to another server
			}
			int id = stoi(rsp);
			leader_id_ = id;
			//<<"Recved id = "<<id<<"\n";
			// server doesn't know who the leader is
			if (id < 0) {
				++count;
				if (count == 200) { // leader is not chosen
					return false;
				}
				std::this_thread::sleep_for(std::chrono::seconds(2));
				continue;
			}
			////<<"I got message "<< id<<"\n";
			if (server->id == (size_t)id) {
				sock_connected_ = true;
				//// << "My leader is "<<id<<"\n";
				return true;
			}
			for (size_t j = 0; j < servers_.size(); ++j) {
				if (servers_[j]->id == (size_t)id) {
					delete sock_;
					sock_ = new UnixSocket();
					////<<"Connecting to ip " <<servers_[j]->ip_addr<<",port = "
						//<<servers_[j]->port_client<<"\n";
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
			////// << "We sent request\n";
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

bool RaftClient::GetNewLeader() {
	delete sock_;
	sock_ = new UnixSocket();
	size_t count = 0;
	while (!Connect()) {
			++count;
			if (count == 1200) {
				return false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
	}
	return true;
}

size_t RaftClient::GetLeaderId() {
	return leader_id_;
}

bool RaftClient::GetResponse(IResponse *resp) {
	if (!sock_connected_)
		return false;
	string response = "";
	try {
		if (!sock_->Recv(response)) {
			return false;
		}
	} catch (std::exception &e) {
		throw e;
		return false;
	}
	resp->SetData(response);
	return true;
}
