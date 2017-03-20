#include "client/RaftClient.hpp"
#include "log/MyLogEntry.hpp"
#include "include/MyResponse.hpp"
#include "lib/lib.hpp"

#include <iostream>
using namespace std;

int main(int argc, char *argv[]) {
    vector<server_t *> servers_arr;
    // XXX: maybe as an argv argument?
    json servers = json::parse_file("/home/sonya/dist_raft_system/src/"
            "server/servers_data1.json");

  for (size_t i = 0; i < servers.size(); ++i) {
        try {
            json& server = servers[i];
            servers_arr.push_back(new server_t);
            servers_arr[i]->id  = server["id"].as<size_t>();
            servers_arr[i]->ip_addr = server["ip-addr"].as<std::string>();
            std::cout<<servers_arr[i]->ip_addr<<"\n";
            servers_arr[i]->port_serv  = server["port_serv"].as<std::string>();
            servers_arr[i]->port_client = server["port_client"].as<std::string>();
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    ConsensusClient *client = new RaftClient(servers_arr);
    if (!client->Connect()) {
        std::cout << "Not Connected\n";
    } else {
        std::cout << "Connected\n";
    }
    /*ILogEntry *log_entry = new MyLogEntry(ADD, "x", "5");
    client->SendRequest(log_entry);
    std::cout << "Sending log entry: "<< log_entry->ToSend()<<"\n";
    while (!client->GetResponse(&resp)) {}
    string str = resp.GetData();
    cout << "Response: "<<str << endl;
    delete log_entry;*/
    std::cout <<"==================================================================\n";
    ILogEntry *log_entry1 = new MyLogEntry(GET, "x", "");
    MyResponse resp;
    client->SendRequest(log_entry1);
    std::cout << "Sending log entry: "<< log_entry1->ToSend()<<"\n";
    while (!client->GetResponse(&resp)) {}
    string str = resp.GetData();
    cout << "Response: "<<str << endl;
    delete log_entry1;
}
