

#include "clientmanager.h"
#include "icsclient.h"
#include "log.h"


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
    TcpConnection* tc = new IcsClient(std::move(s));
    tc->do_read();
    
}


void ClientManager::removeClient(TcpConnection* tc)
{
	LOG_DEBUG("call removeClient");
    delete tc;
}

}