

#include "clientmanager.hpp"
#include "icsclient.hpp"
#include "log.hpp"


namespace ics{

ClientManager::ClientManager(int thread_num)
{

}

ClientManager::~ClientManager()
{
	LOG_DEBUG("call ~ClientManager()");
}

void ClientManager::addClient(asio::ip::tcp::socket s)
{
	LOG_DEBUG("call ClientManager add client");
	TcpConnection* tc = new IcsClient(std::move(s), *this);
    tc->do_read();
    
}


void ClientManager::removeClient(TcpConnection* tc)
{
	LOG_DEBUG("call removeClient");
    delete tc;
}

}