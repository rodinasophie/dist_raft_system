#ifndef __LIB_HPP_
#define __LIB_HPP_

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

using std::string;
using std::vector;
using namespace std;
#include "jsoncons/json.hpp"

using jsoncons::json;

typedef struct server {
	size_t id;
	size_t sfd;
	string ip_addr;
	string port_serv;
	string port_client;
} server_t;

#endif // __LIB_HPP_
