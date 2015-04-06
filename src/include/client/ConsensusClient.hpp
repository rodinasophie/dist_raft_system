#ifndef __CONSENSUS_CLIENT_HPP_
#define __CONSENSUS_CLIENT_HPP_

#include "log/ILogEntry.hpp"
#include "include/IResponse.hpp"

class ConsensusClient {
 public:
	ConsensusClient() {};
	virtual ~ConsensusClient() {};
	virtual bool Connect() = 0;
	virtual bool SendRequest(ILogEntry *log_entry) = 0;
	virtual bool GetResponse(IResponse *resp) = 0;
};

#endif // __CONSENSUS_CLIENT_HPP_
