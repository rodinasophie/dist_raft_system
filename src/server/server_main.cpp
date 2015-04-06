#include "lib/lib.hpp"
#include "server/RaftServer.hpp"

int main(int argc, char **argv) {
	if (argc != 2) {
		// TODO: Set logging
		// ./server id
		return -1;
	}
	size_t id = atoi(argv[1]);
	vector<server_t *> servers_arr;
	json servers = json::parse_file("../src/server/servers_data.json");

  for (size_t i = 0; i < servers.size(); ++i) {
		try {
			json &server = servers[i];
			servers_arr.push_back(new server_t);
			servers_arr[i]->id  = server["id"].as<size_t>();
			servers_arr[i]->ip_addr = server["ip-addr"].as<std::string>();
			servers_arr[i]->port_serv  = server["port_serv"].as<std::string>();
			servers_arr[i]->port_client = server["port_client"].as<std::string>();
		} catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
		}
	}
	ConsensusServer *server = new RaftServer(id, servers_arr);
	server->Run();

	for (size_t i = 0; i < servers_arr.size(); ++i) {
		delete servers_arr[i];
	}
	delete server;
}