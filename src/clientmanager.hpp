

#ifndef _CLIENT_MANAGER_H
#define _CLIENT_MANAGER_H

#include "icsconnection.hpp"
#include "config.hpp"
#include <asio.hpp>
#include <mutex>
#include <list>
#include <unordered_map>
#include <memory>

namespace ics {

template<class Protocol>
class IcsConnection;

typedef asio::ip::tcp icstcp;

typedef asio::ip::udp icsudp;

template<class TerminalProtoType, class CenterProtoType>
class ClientManager
{
public:
	ClientManager(){}

	~ClientManager(){}

	// 将链接名为name的终端链接加入管理器
	void addTerminalConn(const std::string& name, IcsConnection<TerminalProtoType>* conn)
	{
		if (name.empty() || conn == nullptr)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(m_terminalConnLock);
		auto& it = m_terminalConnList[name];
		if (it != nullptr)
		{
			it->replaced();
			delete it;
		}
		it = conn;

		//		LOG_DEBUG("add " << name << ", total client:" << m_terminalConnList.size());
	}

	// 将链接名为name的终端链接移除管理器
	void removeTerminalConn(const std::string& name)
	{
		std::lock_guard<std::mutex> lock(m_terminalConnLock);
		auto it = m_terminalConnList.find(name);
		if (it != m_terminalConnList.end())
		{
			m_terminalConnList.erase(it);
			//			LOG_DEBUG("remove " << name << ", total client:" << m_terminalConnList.size());
		}
	}

	// 查找链接名为name的终端链接
	IcsConnection<TerminalProtoType>* findTerminalConn(const std::string& name)
	{
		auto it = m_terminalConnList.find(name);
		return it->second;
	}

	// 将中心服务器链接加入管理器
	void addCenterConn(IcsConnection<CenterProtoType>* conn)
	{
		if (conn)
		{
			std::lock_guard<std::mutex> lock(m_centerConnLock);
			m_centerConnList.push_back(conn);
		}
	}

	// 将中心服务器链接移除管理器
	void removeCenterConn(IcsConnection<CenterProtoType>* conn)
	{
		if (conn)
		{
			std::lock_guard<std::mutex> lock(m_centerConnLock);
			for (auto it = m_centerConnList.begin(); it != m_centerConnList.end(); it++)
			{
				if (*it == conn)
				{
					m_centerConnList.erase(it);
					break;
				}
			}
		}
	}

	// 查找全部中心服务器链接
	std::list<IcsConnection<CenterProtoType>*>& getCenterConnList()
	{
		return m_centerConnList;
	}

private:
	// 终端链接列表
	std::unordered_map<std::string, IcsConnection<TerminalProtoType>*>	m_terminalConnList;
	std::mutex	m_terminalConnLock;

	// 中心服务器连接列表
	std::list<IcsConnection<CenterProtoType>*> m_centerConnList;
	std::mutex	m_centerConnLock;
};

}

#endif	// _CLIENT_MANAGER_H
