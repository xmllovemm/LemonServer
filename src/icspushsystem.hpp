
#ifndef _ICS_PUSH_SYSTEM_H
#define _ICS_PUSH_SYSTEM_H

#include "connection.hpp"
//#include "messagehandler.hpp"

namespace ics {

class PushSystem {
public:
	PushSystem();

	void init(const string& ip, uint16_t udp_port);

	~PushSystem();

	void send(ProtocolStream& request);

	void start();

private:
//	void tryRetransport();

private:
	asio::io_service	m_ioService;
	ClientManager<UdpConnection> m_clientManager;
	std::unique_ptr<UdpConnection>	m_connection;
	std::unordered_map<uint16_t, MemoryChunk_ptr> m_msgList;
};


}

#endif // _ICS_PUSH_SYSTEM_H
