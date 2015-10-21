

#include "clientmanager.h"
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

}


void ClientManager::removeClient()
{
	LOG_DEBUG("call removeClient");

}

}