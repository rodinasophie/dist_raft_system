#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/param.h>
#include <string.h>
#include <stdio.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <unistd.h>
#include <thread>
#include <fcntl.h>
#include <iostream>
#include <algorithm>
#include <exception>
#include "../../include/socket/UnixSocket.hpp"

UnixSocket::UnixSocket() : sfd_(-1), client_sfd_(-1), state_(NOT_INITED) {}

UnixSocket::~UnixSocket() {
	if (sfd_ != -1)
		close(sfd_);
	if (client_sfd_ != -1)
		close(client_sfd_);
}

void UnixSocket::Reset() {
	if (state_ != CLIENT)
		return;
	state_ = NOT_INITED;
	close(sfd_);
}

int UnixSocket::Connect(string &my_name, string &server_name, string &port_name) {
  if (state_ != NOT_INITED)
		return -1;
	struct addrinfo options;
  memset(&options, 0, sizeof(options));
  options.ai_family   = AF_UNSPEC;
  options.ai_socktype = SOCK_STREAM;

  struct addrinfo *result;
  if (getaddrinfo(server_name.c_str(), port_name.c_str(), &options, &result)
			!= 0) {
    return -1;
  }
  int sfd = -1;
	struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_port        = 0;
	addr.sin_addr.s_addr = inet_addr(my_name.c_str());

  for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1) {
			break;
		}
		if (my_name != "") {
			if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
				freeaddrinfo(result);
				close(sfd);
				return -1;
			}
		}
    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
		}
		//perror("");

    close(sfd);
    sfd = -1;
  }

  if (sfd == -1) {
    freeaddrinfo(result);
    return -1;
  }
  freeaddrinfo(result);
	sfd_ = sfd;
	state_ = CLIENT;
	return sfd;
}
// XXX: Method is not suitable for one machine testing
std::string UnixSocket::GetExternalIP() {
	struct ifaddrs *ifaddr, *ifa;
	int family;
	char host[NI_MAXHOST];
	if (getifaddrs(&ifaddr) == -1) {
		exit(EXIT_FAILURE);
	}
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (!strcmp(ifa->ifa_name, "wlan0")) {
			family = ifa->ifa_addr->sa_family;
			if (family == AF_INET) {
				int s;
				s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host,
						NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
				if (s) {
					string str(host);
					return str;
				}
			}
		}
	}
	// abort if ip cannot be found
	abort();
}

bool UnixSocket::Bind(string ip, int server_port) {
	if (state_ != NOT_INITED) // we cannot bind twice or more times
		return false;
	sfd_ = socket(PF_INET, SOCK_STREAM, 0);
	if (sfd_ == -1)
		return false;

	struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(server_port);
	addr.sin_addr.s_addr = inet_addr(ip.c_str());
	// addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sfd_, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		//perror("");
		return false;
  }
	state_ = SERVER;
	if (listen(sfd_, 100) == -1) {
		//perror("");
		return false;
	}
	return true;
}

int UnixSocket::AcceptIncomings(string &client_ip_addr) {
	if (state_ != SERVER)
		return -1;
	fcntl(sfd_, F_SETFL, O_NONBLOCK);
	int client_sfd = -1;
	struct sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(client_addr));
	socklen_t client_addr_size = sizeof(client_addr);
	client_sfd = accept(sfd_, (struct sockaddr *)&client_addr, &client_addr_size);
	if (client_sfd == -1) {
		return -1;
	}
	client_ip_addr = inet_ntoa(client_addr.sin_addr);
	clients_.push_back(client_sfd);
	return client_sfd;
}
size_t UnixSocket::latency;
size_t UnixSocket::Send(string data) {
  if (state_ == NOT_INITED) {
		// TODO: Set logging
		return 0;
	}
	size_t size = data.length();
	const char *to_send = data.c_str();
	size_t remaining_bytes = size + 1;

  ssize_t ret;
	int sfd = (state_ == CLIENT) ? sfd_ : client_sfd_;

	if (sfd < 0) /* No client to send to */
		return 0;
	errno = 0;
	usleep(UnixSocket::latency);
	while ((remaining_bytes > 0) &&
			   ((ret = send(sfd, to_send, remaining_bytes, MSG_NOSIGNAL)) > 0)) {
		remaining_bytes -= ret;
		to_send += ret;
	}
	if (errno == EPIPE) {
		return 0;
	}
	return size - remaining_bytes;
}

bool UnixSocket::GetReadyClient() { // sets client_sfd_
	if (state_ != SERVER)
		return false;
	if (!ready_fds_.empty()) {
		client_sfd_ = ready_fds_.back();
		ready_fds_.pop_back();
		return true;
	}

	struct timeval timeout;
  fd_set fds;
  FD_ZERO(&fds);
	int max_fd = 0;
	for (size_t i = 0; i < clients_.size(); ++i) {
		FD_SET(clients_[i], &fds);
		max_fd = MAX(max_fd, clients_[i]);
  }
  timeout.tv_sec = 0;
  timeout.tv_usec = 20;

	int res = select(max_fd + 1, &fds, NULL, NULL, &timeout);
	if (res <= 0) {
    return false;
  }

  for (size_t i = 0; i < clients_.size(); ++i) {
		if (FD_ISSET(clients_[i], &fds)) {
			ready_fds_.push_back(clients_[i]);
			break;
		}
	}
	client_sfd_ = ready_fds_.front();

	ready_fds_.pop_front();
	return true;
}

size_t UnixSocket::Recv(string &data) {
	data = "";
	if (state_ == NOT_INITED)
		return 0;
	if (state_ == SERVER)
		if (!GetReadyClient()) {
			return 0;
		}
	int sfd = (state_ == CLIENT) ? sfd_ : client_sfd_;
	char buffer;
  ssize_t ret;
	while ((ret = recv(sfd, &buffer, 1, MSG_DONTWAIT)) >= 0) {
		data += buffer;
		if (buffer == '\0')
			break;
	}
  if (ret == -1) {
		return 0;
  }
	// if shutdown signal is received
	if (!strlen(data.c_str())) {
		clients_.erase(std::remove(clients_.begin(), clients_.end(),
					client_sfd_), clients_.end());
		data = "";
		std::exception e;
		throw e;
		return 0;
	}
	data.resize(data.length() - 1); // cutting \0 symbol
	return data.length();
}

bool UnixSocket::SetReceiver(int sfd) {
	if (state_ != SERVER)
		return false;
	if (find(clients_.begin(), clients_.end(), sfd) != clients_.end()) {
		client_sfd_ = sfd;
		return true;
	}
	return false;
}

size_t UnixSocket::SendToAll(const string &data) {
	if (state_ != SERVER)
		return 0;
	size_t res = 0;
	for (size_t i = 0; i < clients_.size(); ++i) {
		client_sfd_ = clients_[i];
		res += Send(data);
	}
	return res;
}
