#include "clientmanager.hpp"
#include "icsclient.hpp"
#include "log.hpp"


namespace ics{

ClientManager::ClientManager(int thread_num)
{

}

ClientManager::~ClientManager()
{
}

void ClientManager::addClient(asio::ip::tcp::socket s)
{
	TcpConnection* tc = new IcsConnection(std::move(s), *this);
    tc->start();
}


void ClientManager::removeClient(TcpConnection* tc)
{
    delete tc;
}

}