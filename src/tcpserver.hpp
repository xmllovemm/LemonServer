/*
ics server
*/



#ifndef _ICS_TCP_SERVER_H
#define _ICS_TCP_SERVER_H

#include "config.hpp"
#include <asio.hpp>
#include <string>
#include <thread>


namespace ics {

class TcpServer {
public:
	typedef std::function<void(asio::ip::tcp::socket&&)> AddClientHandler;

	TcpServer(asio::io_service& service);

	void init(const std::string& addr, AddClientHandler do_add_client);

	~TcpServer();

	void run_on_thread();

	void stop();
private:
	TcpServer() = delete;

	void do_accept();

private:
	asio::io_service&		m_io_service;
	asio::ip::tcp::acceptor	m_acceptor;
	asio::ip::tcp::socket	m_clientSocket;
	std::thread*			m_io_service_thread;
	AddClientHandler		m_do_add_client;

};

} // end namespace ics
#endif	// end _ICS_TCP_SERVER_H
