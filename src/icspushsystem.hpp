
#ifndef _ICS_PUSH_SYSTEM_H
#define _ICS_PUSH_SYSTEM_H

#include "icsconnection.hpp"
#include "icsprotocol.hpp"
#include "config.hpp"
#include <asio.hpp>
#include <unordered_map>

namespace ics {

template<class Protocol>
class IcsConnection;

class PushSystem {
public:
	PushSystem();

	void init(const std::string& ip, uint16_t udp_port);

	~PushSystem();

	void send(ProtocolStream& request);

	void start();

private:
//	void tryRetransport();

private:
	asio::io_service	m_ioService;
	std::unique_ptr<IcsConnection<asio::ip::udp>>	m_connection;
	std::unordered_map<uint16_t, MemoryChunk> m_msgList;
};


}

#endif // _ICS_PUSH_SYSTEM_H
