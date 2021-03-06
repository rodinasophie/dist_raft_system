#ifndef __TIME_TEST_HPP_
#define __TIME_TEST_HPP_

#include "tests/RaftTest.hpp"
#include "client/RaftClient.hpp"
#include "socket/UnixSocket.hpp"
#include "timer/Timer.hpp"
#include "lib/lib.hpp"
#include "log/MyLogEntry.hpp"
#include "include/MyResponse.hpp"
#include <thread>
#include <iostream>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <atomic>
#include <map>
using std::map;

enum TestType {
    GENERAL,
    LEADER_CRASH,
    CANDIDATE_CRASH,
    LEADER_ELECTION,
};
class TimeTest : public RaftTest {
 public:
    void Run(enum TestType test_type, const char *filename) {
#ifdef _SNAPSHOTTING
#undef _SNAPSHOTTING
#endif // _SNAPSHOTTING
        switch (test_type) {
            case GENERAL:
                RunGeneralTest(filename);
                break;
            case LEADER_CRASH:
                RunLeaderCrashTest(filename);
                break;
            case CANDIDATE_CRASH:
                RunCandidateCrashTest(filename);
                break;
            case LEADER_ELECTION:
                RunLeaderElectionTest(filename);
                break;
        };
    }
    void SetLatency(size_t latency) {
        UnixSocket::latency = latency;
    }
 private:
    void RunLeaderElectionTest(const char *filename) {
        for (size_t i = 0; i < 1000; ++i) {
            RunServerCluster(filename);
            sleep(5);
            std::thread t(&TimeTest::RunKillerClient, this, filename);
            t.join();
            KillAll();
        }
    }

    void RunGeneralTest(const char *filename) {
        RunServerCluster(filename);
        sleep(1);
        std::thread t(&TimeTest::RunClient, this, filename);
        t.join();
        KillAll();
    }

    void RunLeaderCrashTest(const char *filename) {
        leader_id_ = -1;
        RunServerCluster(filename);
        sleep(1);
        std::thread t(&TimeTest::RunClient, this, filename);
        while (leader_id_.load(std::memory_order_relaxed) < 0) {}
        while (servers_.size() > ((cluster_size_ / 2) + 1)) {
            sleep(1);
            KillLeader();
        }
        t.join();
        KillAll();
    }

    void KillAll() {
        for (auto it = servers_.begin(); it != servers_.end(); ++it) {
            kill(it->second, SIGINT);
        }
    }

    void RunCandidateCrashTest(const char *filename) {
        leader_id_ = -1;
        RunServerCluster(filename);
        sleep(1);
        std::thread t(&TimeTest::RunClient, this, filename);
        while (leader_id_.load(std::memory_order_relaxed) < 0) {}
        while (servers_.size() > ((cluster_size_ / 2) + 1)) {
            sleep(1);
            KillCandidate();
        }
        t.join();
        KillAll();
    }

    void KillCandidate() {
        int server_to_kill;
        while (1) {
            server_to_kill = rand() % cluster_size_;
            if ((server_to_kill == leader_id_) ||
                    (servers_.find(server_to_kill) == servers_.end())) {
                if (servers_.size() == 1)
                    return;
                continue;
            }
            break;
        }
        kill(servers_[server_to_kill], SIGINT);
        servers_.erase(servers_.find(server_to_kill));
    }

    void KillLeader() {
        int server_to_kill = leader_id_;
        kill(servers_[server_to_kill], SIGINT);
        servers_.erase(servers_.find(server_to_kill));
    }

    void RunServerCluster(const char *filename) {
        json servers = json::parse_file(filename);
        pid_t parent = getpid();
        const char *program = "../out/server";
        pid_t pid;
        cluster_size_ = servers.size();
        for (size_t i = 0; i < servers.size(); ++i) {
            if (getpid() == parent) {
                // parent process
                std::cout<<"Fork at "<<parent<<"\n";
                pid = fork();
                sleep(1);
                if (pid > 0) {
                    servers_[i] = pid;
                }
                if (pid == 0) {
                    // child process
                    char **args1 = (char **)malloc(4 * sizeof(char*));
                    args1[0] = strdup(program);
                    args1[1] = strdup(std::to_string(i).c_str());
                    args1[2] = strdup(filename);
                    args1[3] = NULL;
                    execvp(program, args1);
                }
            }
        }
    }

    void RunKillerClient(const char *filename) {
        vector<server_t *> servers_arr;
        json servers = json::parse_file(filename);

        for (size_t i = 0; i < servers.size(); ++i) {
            try {
                json& server = servers[i];
                servers_arr.push_back(new server_t);
                servers_arr[i]->id  = server["id"].as<size_t>();
                servers_arr[i]->ip_addr = server["ip-addr"].as<std::string>();
                servers_arr[i]->port_serv  = server["port_serv"].as<std::string>();
                servers_arr[i]->port_client = server["port_client"].as<std::string>();
            } catch (const std::exception& e) {
                std::cerr << e.what() << std::endl;
            }
        }
        ConsensusClient *client = new RaftClient(servers_arr);
        if (!client->Connect()) {
            std::cout<<"returning\n";
            return;
        }
        ILogEntry *log_entry = new MyLogEntry(ADD, "x", "5");
        client->SendRequest(log_entry);
        MyResponse resp;
        while (!client->GetResponse(&resp)) {}
        string str = resp.GetData();
        delete log_entry;
        leader_id_ = client->GetLeaderId();
        struct timeval start, end;
        const char *filename1 = "/home/sonya/dist_raft_system/time";
        ofstream f;
        f.open(filename1, std::ofstream::app);
        KillLeader();
        usleep(1000);
        gettimeofday(&start, NULL);
        if (!client->GetNewLeader()) {
            f.close();
            delete client;
            return;
        }
        gettimeofday(&end, NULL);
        std::cout<<"Hello+\n";
        f<<((end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec));
        f<<"\n";
        f.close();
        delete client;
    }

    void RunClient(const char *filename) {
        vector<server_t *> servers_arr;
        json servers = json::parse_file(filename);

        for (size_t i = 0; i < servers.size(); ++i) {
            try {
                json& server = servers[i];
                servers_arr.push_back(new server_t);
                servers_arr[i]->id  = server["id"].as<size_t>();
                servers_arr[i]->ip_addr = server["ip-addr"].as<std::string>();
                servers_arr[i]->port_serv  = server["port_serv"].as<std::string>();
                servers_arr[i]->port_client = server["port_client"].as<std::string>();
            } catch (const std::exception& e) {
                std::cerr << e.what() << std::endl;
            }
        }

        ConsensusClient *client = new RaftClient(servers_arr);
        if (!client->Connect()) {
            std::cout << "Not Connected\n";
            return;
        } else {
            std::cout << "Connected\n";
        }
        ILogEntry *log_entry = new MyLogEntry(ADD, "x", "5");
        client->SendRequest(log_entry);
        std::cout << "Sending log entry: "<< log_entry->ToSend()<<"\n";
        MyResponse resp;
        while (!client->GetResponse(&resp)) {}
        string str = resp.GetData();
        cout << "Response: "<<str << endl;
        delete log_entry;
        struct timeval start, end;
        const char *filename1 = "/home/sonya/dist_raft_system/time";
        ofstream f;
        f.open(filename1);
        for (size_t i = 0; i < MAX_CLIENT_REQUEST; ++i) {
            ILogEntry *log_entry1 = new MyLogEntry(GET, "x", "");
            leader_id_ = client->GetLeaderId();
            gettimeofday(&start, NULL);
            client->SendRequest(log_entry1);
            try {
                while (!client->GetResponse(&resp)) {}
            } catch (std::exception &e) {
                delete log_entry1;
                // XXX: normally this request should be resend
                continue;
            }
            string str = resp.GetData();
            gettimeofday(&end, NULL);
            f << (end.tv_usec - start.tv_usec);
            f<<"\n";
            delete log_entry1;
        }
        f.close();
        delete client;
    }
    const size_t MAX_CLIENT_REQUEST = 1000;
    std::atomic<int> leader_id_;
    size_t cluster_size_;
    map<size_t, pid_t> servers_; // server_id, pid
};

#endif // __TIME_TEST_HPP_
