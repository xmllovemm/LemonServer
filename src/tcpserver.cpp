

#include "tcpserver.hpp"
#include "log.hpp"


namespace ics{

TcpServer::TcpServer(asio::io_service& service, const std::string& ip, int port, addClientHandler do_add_client)
	: m_io_service(service)
	, m_acceptor(m_io_service, asio::ip::tcp::endpoint(asio::ip::address::from_string(ip), port))
	, m_client_socket(m_io_service)
	, m_io_service_thread(nullptr)
	, m_do_add_client(do_add_client)
{
	LOG_DEBUG("Tcp server starts to listen at " << ip << ":" << port);
}


TcpServer::TcpServer(asio::io_service& service, int port, addClientHandler do_add_client)
	: m_io_service(service)
	, m_acceptor(m_io_service, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
	, m_client_socket(m_io_service)
	, m_io_service_thread(nullptr)
	, m_do_add_client(do_add_client)
{
	LOG_DEBUG("Tcp server starts to listen at localhost:" << port);
}

TcpServer::~TcpServer()
{
	
}

void TcpServer::run()
{
	LOG_DEBUG("Tcp server starts to accept client");
	do_accept();
}

void TcpServer::run_on_thread()
{
	if (m_io_service_thread != nullptr)
	{
		delete m_io_service_thread;
	}
	m_io_service_thread = new std::thread([this](){
									run();
								});
}

void TcpServer::stop()
{
	LOG_DEBUG("Tcp server stops");
	m_io_service_thread->join();
}

asio::io_service& TcpServer::getService()
{
	return m_io_service;
}

void TcpServer::do_accept()
{
	// accept client
	m_acceptor.async_accept(m_client_socket,
		[this](std::error_code ec)
		{
			if (!ec)
			{
				m_do_add_client(std::move(m_client_socket));
			}

			// accept next client
			do_accept();
		});
}

} // end namespace ics
