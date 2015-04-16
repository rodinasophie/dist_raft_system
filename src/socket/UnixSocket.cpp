#include <netdb.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/param.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <thread>
#include <fcntl.h>
#include <iostream>
#include <algorithm>

#include "../../include/socket/UnixSocket.hpp"

UnixSocket::UnixSocket() : sfd_(-1), client_sfd_(-1), state_(NOT_INITED) {}

UnixSocket::~UnixSocket() {
	if (sfd_ != -1)
		close(sfd_);
	if (client_sfd_ != -1)
		close(client_sfd_);
}

bool UnixSocket::Connect(string &server_name, string &port_name) {
  if (state_ != NOT_INITED)
		return false;
	struct addrinfo options;
  memset(&options, 0, sizeof(options));
  options.ai_family   = AF_UNSPEC;
  options.ai_socktype = SOCK_STREAM;

  struct addrinfo *result;
  if (getaddrinfo(server_name.c_str(), port_name.c_str(), &options, &result) != 0) {
    return false;
  }
  int sfd = -1;
  for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1) {
			break;
		}
    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
		}
		perror("");

    close(sfd);
    sfd = -1;
  }

  if (sfd == -1) {
    freeaddrinfo(result);
    return false;
  }
  freeaddrinfo(result);
	sfd_ = sfd;
	state_ = CLIENT;
	return true;
}

bool UnixSocket::Bind(int server_port) {
	if (state_ != NOT_INITED) // we cannot bind twice or more times
		return false;
	sfd_ = socket(PF_INET, SOCK_STREAM, 0);
	if (sfd_ == -1)
		return false;

	struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(server_port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sfd_, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    return false;
  }
	state_ = SERVER;
	if (listen(sfd_, 100) == -1) {
		return false;
	}
	return true;
}

bool UnixSocket::AcceptIncomings() {
	if (state_ != SERVER)
		return false;
	fcntl(sfd_, F_SETFL, O_NONBLOCK);
	int client_sfd = -1;
	struct sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(client_addr));
	socklen_t client_addr_size = sizeof(client_addr);
	client_sfd = accept(sfd_, (struct sockaddr *)&client_addr, &client_addr_size);
	if (client_sfd == -1) {
		return false;
	}
	clients_.push_back(client_sfd);
	return true;
}

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
	while ((remaining_bytes > 0) &&
			   ((ret = send(sfd, to_send, remaining_bytes, 0)) > 0)) {
		remaining_bytes -= ret;
		to_send += ret;
	}
	return size - remaining_bytes;
}

size_t UnixSocket::SendToAll(string data) {
	if (state_ != SERVER)
		return 0;
	size_t res = 0;
	for (size_t i = 0; i < clients_.size(); ++i) {
		client_sfd_ = clients_[i];
		res += Send(data);
	}
	return res;
}

bool UnixSocket::GetReadyClient() {
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
  timeout.tv_usec = 200000;

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
	// if shutdown signal is sent
	if (!strlen(data.c_str())) {
		clients_.erase(std::remove(clients_.begin(), clients_.end(), client_sfd_), clients_.end());
		data = "";
		return 0;
	}
	data.resize(data.length() - 1); // cutting \0 symbol
	return data.length();
}
