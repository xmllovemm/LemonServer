//
//  tcpconnection.hpp
//  ics-server
//
//  Created by lemon on 15/10/22.
//  Copyright © 2015年 personal. All rights reserved.
//

#ifndef _ICS_CONNECTION_H
#define _ICS_CONNECTION_H


#include "config.hpp"
#include "icsprotocol.hpp"
#include "clientmanager.hpp"
#include "icsclient.hpp"
#include "log.hpp"

namespace ics {

class MessageHandlerImpl;

template<class Connection>
class ClientManager;

typedef asio::ip::tcp icstcp;

typedef asio::ip::udp icsudp;

// ICS协议连接类
template<class ProtocolType = icstcp>
class IcsConnection {
public:
	typedef typename asio::basic_stream_socket<ProtocolType> socket;

	typedef  IcsConnection<ProtocolType> _thisType;

	typedef typename socket::shutdown_type shutdown_type;

	IcsConnection(socket&& s, ClientManager<_thisType>& cm)
		: m_socket(std::forward<socket>(s))
		, m_messageHandler(nullptr)
		, m_client_manager(cm)
		, m_isSending(false)
		, m_sendSerialNum(0)
	{

	}

	~IcsConnection()
	{
		if (m_messageHandler != nullptr)
		{
			if (!m_connName.empty())
			{
				m_client_manager.removeTerminalConn(m_connName);
			}
			delete m_messageHandler;
		}
		LOG_DEBUG(m_connName << " has been deleted");
	}

	void start()
	{
		do_read();
		//	do_write();	// nothing to write
	}

	void replaced(bool flag = true)
	{
		m_replaced = flag;
	}

	// 获取客户端ID
	const std::string& name() const
	{
		return m_connName;
	}

	// 设置客户端ID
	void setName(std::string& name)
	{
		if (!name.empty() && name != m_connName)
		{
			m_client_manager.addTerminalConn(name, this);
			m_connName = std::move(name);
		}
	}

	// 转发消息给终端
	bool forwardToTerminal(const std::string& name, ProtocolStream& message)
	{
		bool ret = false;
		auto conn = m_client_manager.findTerminalConn(name);

		if (conn == nullptr)
		{
			LOG_DEBUG("the connection: " << name << " not found");
			return ret;
		}

		try {
			ProtocolStream response(mp);
			conn->m_messageHandler->dispatch(*conn, message, response);
			if (!response.empty())
			{
				response.getHead()->setSendNum(conn->m_sendSerialNum++);
				response.serailzeToData();
				conn->trySend(message.toMemoryChunk());
			}
			ret = true;
		}
		catch (IcsException& ex)
		{
			LOG_ERROR("forward message std::exception:" << ex.message());
		}
		catch (otl_exception& ex)
		{
			LOG_ERROR("forward message otl_exception:" << ex.msg);
		}
		catch (...)
		{
			LOG_ERROR("forward message unknown error");
		}

		return ret;
	}

	// 转发消息到全部的中心服务器
	bool forwardToCenter(ProtocolStream& message)
	{
		bool ret = false;
		auto connList = m_client_manager.getCenterConnList();

		// 有中心服务器连接
		if (!connList.empty())
		{
			message.serailzeToData();
			MemoryChunk mc = message.toMemoryChunk();

			for (auto it = connList.begin(); it != connList.end(); it++)
			{
				// 找到该链接直接发送
				(*it)->trySend(mc.clone(mp));
			}

			message.release();
		}
		return ret;
	}

	void do_read()
	{
		// wait response
		m_socket.async_read_some(asio::buffer(m_recvBuff, sizeof(m_recvBuff)),
			[this](const std::error_code& ec, std::size_t length)
		{
			// no error and handle message
			if (!ec && handleData(m_recvBuff, length))
			{
				// continue to read		
				do_read();
			}
			else
			{
				do_error(shutdown_type::shutdown_both);
			}
		});
	}

	void do_write()
	{
		trySend();
	}

	void do_error(shutdown_type type)
	{
		m_socket.shutdown(type);
	}

	void toHexInfo(const char* info, uint8_t* data, std::size_t length)
	{
#ifndef NDEBUG
		char buff[1024];
		for (size_t i = 0; i < length && i < sizeof(buff) / 3; i++)
		{
			std::sprintf(buff + i * 3, " %02x", data[i]);
		}
		LOG_DEBUG(m_connName << " " << info << " " << length << " bytes...");
#endif
	}

	void trySend()
	{
		std::lock_guard<std::mutex> lock(m_sendLock);
		if (!m_isSending && !m_sendList.empty())
		{
			MemoryChunk& chunk = m_sendList.front();
			m_socket.async_send(asio::buffer(chunk.getBuff(), chunk.getUsedSize()),
				[this](const std::error_code& ec, std::size_t length)
			{
				if (!ec && length == m_sendList.front().getUsedSize())
				{
					MemoryChunk& mc = m_sendList.front();
					this->toHexInfo("send", (uint8_t*)mc.getBuff(), mc.getUsedSize());
					mp.put(mc);
					m_sendList.pop_front();
					m_isSending = false;
					trySend();
				}
				else
				{
					LOG_DEBUG(m_connName << " send data error");
					do_error(shutdown_type::shutdown_both);
				}
			});
		}
	}

	void trySend(MemoryChunk&& mc)
	{
		{
			std::lock_guard<std::mutex> lock(m_sendLock);
			m_sendList.push_back(mc);
		}
		trySend();
	}

	void replyResponse(uint16_t ackNum)
	{
		ProtocolStream response(mp);
		response.getHead()->init(protocol::MessageId::MessageId_min, ackNum);
		response.getHead()->setSendNum(m_sendSerialNum++);

		trySend(response.toMemoryChunk());
	}

	void assertIcsMessage(ProtocolStream& message)
	{
		if (m_messageHandler == nullptr)
		{
			if (message.size() < sizeof(ProtocolHead)+ProtocolHead::CrcCodeSize)
			{
				IcsException("first package's size:%d is not more than IcsMsgHead", (int)message.size());
			}

			// 根据消息ID判断处理类型
			protocol::MessageId msgid = message.getHead()->getMsgID();
			if (msgid > protocol::MessageId::T2C_min && msgid < protocol::MessageId::T2C_max)
			{
				m_messageHandler = new TerminalHandler();
			}
			else if (msgid > protocol::MessageId::W2C_min && msgid < protocol::MessageId::W2C_max)
			{
				m_messageHandler = new WebHandler();
			}
			else if (msgid == protocol::MessageId::T2T_forward_msg)
			{
				m_messageHandler = new ProxyTerminalHandler();
			}

			m_messageHandler = new TerminalHandler();
		}
	}

	bool handleData(uint8_t* buf, std::size_t length)
	{
		bool ret = false;

		// show debug info
		this->toHexInfo("recv", buf, length);

		ProtocolStream request(buf, length);
		ProtocolHead* head = request.getHead();

		try {
			request.verify();

			if (head->isResponse())	// ignore response message from terminal
			{
				return true;
			}

			assertIcsMessage(request);

			ProtocolStream response(mp);

			m_messageHandler->handle(*this, request, response);

			if (head->needResposne())
			{
				response.getHead()->init(protocol::MessageId::MessageId_min, head->getSendNum());	// head->getMsgID()
			}

			if (!response.empty() || head->needResposne())
			{
				response.getHead()->setSendNum(m_sendSerialNum++);
				response.serailzeToData();
				trySend(response.toMemoryChunk());
			}
			ret = true;
		}
		catch (IcsException& ex)
		{
			LOG_ERROR("handle message [" << (int)(head->getMsgID()) << "] std::exception:" << ex.message());
		}
		catch (otl_exception& ex)
		{
			LOG_ERROR("handle message [" << (int)(head->getMsgID()) << "] otl_exception:" << ex.msg);
		}
		catch (...)
		{
			LOG_ERROR("handle message [" << (int)(head->getMsgID()) << "] unknown error");
		}

		return ret;
	}



private:
	// tcp socket
	socket	m_socket;

	// replaced by another TcpConnection
	bool	m_replaced;

	MessageHandlerImpl*		m_messageHandler;
	std::string             m_connName;
	ClientManager<_thisType>&	m_client_manager;

	// recv area
	uint8_t			m_recvBuff[512];

	// send area
	std::list<MemoryChunk> m_sendList;
	std::mutex		m_sendLock;
	bool			m_isSending;
	uint16_t		m_sendSerialNum;
};



}

#endif // _ICS_CONNECTION_H
