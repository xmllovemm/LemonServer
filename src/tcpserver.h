/*
ics server
*/



#ifndef _ICS_TCP_SERVER_H
#define _ICS_TCP_SERVER_H

#include "config.h"
#include <string>
#include <thread>

namespace ics {

class TcpConnection {
public:
	TcpConnection(asio::ip::tcp::socket s) : m_socket(std::move(s)){}
		
	virtual ~TcpConnection();

	virtual void do_read() = 0;

	virtual void do_wriete() = 0;

	virtual void do_error() = 0;

protected:
	asio::ip::tcp::socket m_socket;
};


class TcpServer {
public:

	TcpServer() = delete;

	TcpServer(const std::string& ip, int port, std::function<void (asio::ip::tcp::socket)> do_add_client);

	explicit TcpServer(int port, std::function<void(asio::ip::tcp::socket)> do_add_client);

	~TcpServer();

	void run();

	void run_on_thread();

	void stop();

private:
	void do_accept();

private:
	asio::io_service		m_io_service;
	asio::ip::tcp::acceptor	m_acceptor;
	asio::ip::tcp::socket	m_client_socket;
	std::thread*			m_io_service_thread;

	std::function<void(asio::ip::tcp::socket)> m_do_add_client;

};

} // end namespace ics
#endif	// end _ICS_TCP_SERVER_H
