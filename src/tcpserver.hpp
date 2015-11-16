/*
ics server
*/



#ifndef _ICS_TCP_SERVER_H
#define _ICS_TCP_SERVER_H

#include "config.hpp"
#include <string>
#include <thread>


namespace ics {

class TcpServer {
public:
	typedef std::function<void(asio::ip::tcp::socket&&)> addClientHandler;

	TcpServer(asio::io_service& service, const std::string& ip, int port, addClientHandler do_add_client);

	TcpServer(asio::io_service& service, int port, addClientHandler do_add_client);

	~TcpServer();

	void run();

	void run_on_thread();

	void stop();

	asio::io_service& getService();

private:
	TcpServer() = delete;

	void do_accept();

private:
	asio::io_service&		m_io_service;
	asio::ip::tcp::acceptor	m_acceptor;
	asio::ip::tcp::socket	m_client_socket;
	std::thread*			m_io_service_thread;

	std::function<void(asio::ip::tcp::socket)> m_do_add_client;

};

} // end namespace ics
#endif	// end _ICS_TCP_SERVER_H
