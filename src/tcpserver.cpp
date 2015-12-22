

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

void TcpServer::init(const char* name, const std::string& addr, AddClientHandler do_add_client)
{
	std::regex pattern("^((\\d{1,3}\\.){3}\\d{1,3}):(\\d{1,5})$");
	std::match_results<std::string::const_iterator> result;

	if (!std::regex_match(addr, result, pattern))
	{
		throw IcsException("address=%s don't match format 'ip:port'", addr.c_str());
	}

//	asio::ip::tcp::resolver resolver(m_io_service);
//	auto endp = resolver.resolve(	asio::ip::tcp::resolver::query(result[1])		, std::strtol(result[3].str().c_str(), nullptr, 10)));

	asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(result[1]), std::strtol(result[3].str().c_str(), nullptr, 10));

	m_acceptor.open(endpoint.protocol());

	m_acceptor.set_option(asio::socket_base::reuse_address(true));

	m_acceptor.bind(endpoint);

	m_acceptor.listen();

	m_do_add_client = do_add_client;

	LOG_DEBUG("The " << name << " tcp server starts to listen at " << addr);

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
	if (m_io_service_thread)
	{
		m_io_service_thread->join();
	}
	m_acceptor.close();
}

void TcpServer::do_accept()
{
	// accept client
	m_acceptor.async_accept(m_clientSocket,
		[this](std::error_code ec)
		{
			if (!ec)
			{
				try {
					m_do_add_client(std::move(m_clientSocket));
				}
				catch (IcsException& ex)
				{
					LOG_ERROR("create tcp connection error=" << ex.message());
				}
				catch (std::exception& ex)
				{
					LOG_ERROR("create tcp connection error=" << ex.what());
				}
				catch (...)
				{
					LOG_ERROR("create tcp connection unknown error=");
				}
			}
			else
			{
				LOG_ERROR("listen error=" << ec.message());
			}
			// accept next client
			do_accept();
		});
}

} // end namespace ics
