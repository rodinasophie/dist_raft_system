#ifndef __CONSENSUS_SERVER_HPP_
#define __CONSENSUS_SERVER_HPP_

class ConsensusServer {
 public:
	ConsensusServer() {};
	virtual ~ConsensusServer() {};
	virtual void Run() = 0;
};

#endif // __CONSENSUS_SERVER_HPP_
