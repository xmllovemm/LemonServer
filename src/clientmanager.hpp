

#ifndef _Connection_MANAGER_H
#define _Connection_MANAGER_H

#include "config.hpp" 
#include "connection.hpp"
#include "log.hpp"
#include <map>
#include <mutex>
#include <list>

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

	void addTerminalConn(const std::string& name, Connection* conn)
	{
		if (name.empty() || conn == nullptr)
		{
			return;
		}
		std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
		auto& it = m_terminalConnMap[name];
		if (it != nullptr)
		{
			it->replaced();
			delete it;
		}
		it = conn;
	}

	void removeTerminalConn(const std::string& name)
	{
		std::lock_guard<std::mutex> lock(m_terminalConnMapLock);
		auto it = m_terminalConnMap.find(name);
		if (it != m_terminalConnMap.end())
		{
			delete it->second;
			m_terminalConnMap.erase(it);
		}
	}

	Connection* findTerminalConn(const std::string& name)
	{
		auto it = m_terminalConnMap.find(name);
		return it->second;
	}

	void addCenterConn(Connection* conn)
	{

	}

	const std::list<Connection*> getCenterConnList()
	{
		return m_centerConnList;
	}

private:
	std::map<std::string, Connection*>	m_terminalConnMap;
	std::mutex	m_terminalConnMapLock;

	std::list<Connection*> m_centerConnList;
	//    std::shared_ptr<std::thread>	m_thread;

//	std::unordered_map<std::string, std::shared_ptr<Connection>> m_connectionMap;
};

}

#endif	// _Connection_MANAGER_H
