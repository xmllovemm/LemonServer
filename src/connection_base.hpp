

#ifndef _CONNECTION_BASE_H
#define _CONNECTION_BASE_H

#include "config.hpp"
#include <asio.hpp>
namespace ics {

//template<class Protocol>
class ConnectionImpl {
public:
	typedef typename asio::ip::tcp::socket socket;

	ConnectionImpl(socket&& s)
		: m_socket(std::move(s))
	{

	}

	~ConnectionImpl()
	{
		asio::error_code ec;
		m_socket.close(ec);
	}

	void start()
	{

	}

private:
	socket	m_socket;
};

}

#endif // 