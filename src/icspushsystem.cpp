

#include "icspushsystem.hpp"

namespace ics {

PushSystem::PushSystem()
	: m_ioService()//, m_clientManager(0)
{
	
}

PushSystem::~PushSystem()
{

}

void PushSystem::init(const std::string& ip, uint16_t udp_port)
{
	asio::ip::udp::socket s(m_ioService);
	asio::ip::udp::endpoint endpoint(asio::ip::address::from_string(ip), udp_port);

	s.connect(endpoint);

	m_connection.reset(new PushClient<asio::ip::udp>(std::move(s)));

	start();
}

void PushSystem::send(ProtocolStream& request)
{
	m_connection->dispatch(request);
}

void PushSystem::start()
{
	m_connection->start();

	m_ioService.run();
}

}