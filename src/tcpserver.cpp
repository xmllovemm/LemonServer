

#include "tcpserver.hpp"
#include "log.hpp"
#include "icsexception.hpp"
#include <regex>


namespace ics{


TcpServer::TcpServer(asio::io_service& service)
: m_io_service(service)
, m_acceptor(service)
, m_clientSocket(m_io_service)
, m_io_service_thread(nullptr)
{

}


TcpServer::~TcpServer()
{
	
}

void TcpServer::init(const std::string& addr, AddClientHandler do_add_client)
{
	std::regex pattern("^((\\d{1,3}\\.){3}\\d{1,3}):(\\d{1,5})$");
	std::match_results<std::string::const_iterator> result;

	if (!std::regex_match(addr, result, pattern))
	{
		throw IcsException("address=%s isn't match ip:port", addr.c_str());
	}

	asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(result[1]), std::strtol(result[3].str().c_str(), nullptr, 10));

	m_acceptor.open(endpoint.protocol());

	m_acceptor.bind(endpoint);

	m_acceptor.listen();

	m_do_add_client = do_add_client;

	LOG_DEBUG("Tcp server starts to listen at " << addr);

	do_accept();
}

void TcpServer::run_on_thread()
{
	if (m_io_service_thread != nullptr)
	{
		delete m_io_service_thread;
	}
	m_io_service_thread = new std::thread([this](){
									do_accept();
								});
}

void TcpServer::stop()
{
	LOG_DEBUG("Tcp server stops");
	if (m_io_service_thread)
	{
		m_io_service_thread->join();
	}
}

void TcpServer::do_accept()
{
	// accept client
	m_acceptor.async_accept(m_clientSocket,
		[this](std::error_code ec)
		{
			if (!ec)
			{
				m_do_add_client(std::move(m_clientSocket));
			}

			// accept next client
			do_accept();
		});
}

} // end namespace ics
