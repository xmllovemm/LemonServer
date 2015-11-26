

#include "icspushsystem.hpp"

namespace ics {

PushSystem::PushSystem()
	:m_ioService(), m_clientManager(0)
{
	
}

PushSystem::~PushSystem()
{

}

void PushSystem::init(const string& ip, uint16_t udp_port)
{
	UdpConnection::socket s(m_ioService);
	asio::ip::udp::endpoint endpoint(asio::ip::address::from_string(ip), udp_port);

//	s.connect(endpoint);

	m_connection.reset(new UdpConnection(std::move(s)));

	start();
}

void PushSystem::send(ProtocolStream& request)
{
	m_connection->directSend(request);
}

void PushSystem::start()
{
	m_connection->start();

	m_ioService.run();
}

}