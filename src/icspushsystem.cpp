

#include "icspushsystem.hpp"
#include <regex>


namespace ics {


PushMsgConnection::PushMsgConnection(socket&& s)
	: _baseType(std::move(s))
{
	_baseType::m_name = "PushMsg@" + _baseType::m_name;
}

// 处理底层消息
void PushMsgConnection::handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
{
	LOG_DEBUG("recv response of push message");
}

// 处理平层消息
void PushMsgConnection::dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
{
	_baseType::trySend(request);
}


PushSystem::PushSystem(asio::io_service& ioService, const std::string& addr)
{
	asio::ip::udp::socket s(ioService);

	std::regex pattern("^((\\d{1,3}\\.){3}\\d{1,3}):(\\d{1,5})$");
	std::match_results<std::string::const_iterator> result;

	if (!std::regex_match(addr, result, pattern))
	{
		throw IcsException("address=%s isn't match ip:port", addr.c_str());
	}

	asio::ip::udp::endpoint endpoint(asio::ip::address::from_string(result[1]), std::strtol(result[3].str().c_str(), nullptr, 10));

//	m_serverEndpoint = endpoint;

	s.connect(endpoint);

	m_connection.reset(new PushMsgConnection(std::move(s)));

	m_connection->start();
}

PushSystem::~PushSystem()
{

}


void PushSystem::send(ProtocolStream& request)
{
	m_connection->dispatch(request);
}

}