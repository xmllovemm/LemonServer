

#ifndef _Connection_MANAGER_H
#define _Connection_MANAGER_H

#include "config.hpp" 
#include "connection.hpp"
#include "log.hpp"
#include <map>
#include <mutex>

namespace ics {

template<class Connection>
class ClientManager
{
public:
	typedef typename Connection::socket socket;

	ClientManager(int thread_num)
	{

	}

	~ClientManager()
	{

	}

	void createConnection(socket&& s)
	{
		Connection* c = new Connection(std::move(s), *this);
		c->start();
	}

	void addConnection(const std::string& name, Connection* conn)
	{
		if (name.empty() || conn == nullptr)
		{
			return;
		}
		std::lock_guard<std::mutex> lock(m_ConnectionLock);
		auto& it = m_ConnectionMap[name];
		if (it != nullptr)
		{
			it->replaced();
			delete it;
		}
		it = conn;
	}

	void removeConnection(const std::string& name)
	{
		std::lock_guard<std::mutex> lock(m_ConnectionLock);
		auto it = m_ConnectionMap.find(name);
		if (it != m_ConnectionMap.end())
		{
			delete it->second;
			m_ConnectionMap.erase(it);
		}
	}

	Connection* findConnection(const std::string& name)
	{
		auto it = m_ConnectionMap.find(name);
		return it->second;
	}

private:
	std::map<std::string, Connection*>	m_ConnectionMap;
	std::mutex	m_ConnectionLock;
	//    std::shared_ptr<std::thread>	m_thread;

};

}

#endif	// _Connection_MANAGER_H
