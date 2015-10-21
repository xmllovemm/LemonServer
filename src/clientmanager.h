

#ifndef _CLIENT_MANAGER_H
#define _CLIENT_MANAGER_H

#include "config.h" 
#include <map>

namespace ics {

class ClientManager
{
public:
	ClientManager(int thread_num);

	~ClientManager();

	void addClient(asio::ip::tcp::socket s);

	void removeClient();

private:
//	std::map<std::string, TcpConnection*>	m_client_map;
	//	std::shared_ptr<std::thread>	m_thread;
};

}

#endif	// _CLIENT_MANAGER_H