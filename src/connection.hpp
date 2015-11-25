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
#include "log.hpp"
#include "messagehandler.hpp"
#include "icsprotocol.hpp"
#include "clientmanager.hpp"
#include "mempool.hpp"
#include <asio.hpp>
#include <asio.hpp>
#include <string>
#include <mutex>


extern ics::MemoryPool mp;

namespace ics {

///*
class MessageHandlerImpl;
class TerminalHandler;
class ProxyTerminalHandler;
class WebHandler;
class CenterServerHandler;
//*/

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
		, m_replaced(false)
		, m_client_manager(cm)
		, m_request(mp)
		, m_isSending(false)
		, m_sendSerialNum(0)
	{

	}

	~IcsConnection()
	{
		if (m_msgHandler && !m_connName.empty())
		{
			m_client_manager.removeTerminalConn(m_connName);
		}
		m_socket.close();
		LOG_DEBUG(m_connName << " has been deleted");
	}

	void start()
	{
		do_read();
		do_write();
	}

	// 该终端被替换
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
			conn->m_msgHandler->dispatch(*conn, message, response);
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
			MemoryChunk_ptr mc = message.toMemoryChunk();

			for (auto it = connList.begin(); it != connList.end(); it++)
			{
				// 找到该链接直接发送
				(*it)->trySend(mc->clone(mp));
			}

			message.release();
		}
		return ret;
	}

	// 直接发送到该链接对端
	void directSend(ProtocolStream& message)
	{
		message.getHead()->setSendNum(m_sendSerialNum);
		message.serailzeToData();
		trySend(message.toMemoryChunk());
	}
private:
	void do_read()
	{
		// wait response
		m_socket.async_read_some(asio::buffer(m_recvBuff),
			[this](const std::error_code& ec, std::size_t length)
			{
				// no error and handle message
				if (!ec && this->handleData(m_recvBuff.data(), length))
				{
					// continue to read		
					this->do_read();
				}
				else
				{
					this->do_error(shutdown_type::shutdown_both);
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
		delete this;
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
			MemoryChunk_ptr& chunk = m_sendList.front();
			m_socket.async_send(asio::buffer(chunk->getBuff(), chunk->getUsedSize()),
					[this](const std::error_code& ec, std::size_t length)
						{
							MemoryChunk_ptr& chunk = m_sendList.front();
							if (!ec && length == chunk->getUsedSize())
							{
								this->toHexInfo("send", (uint8_t*)chunk->getBuff(), chunk->getUsedSize());
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

	void trySend(MemoryChunk_ptr&& mc)
	{
		{
			std::lock_guard<std::mutex> lock(m_sendLock);
			m_sendList.push_back(std::move(mc));
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
		if (!m_msgHandler)
		{
			if (message.size() < sizeof(ProtocolHead)+ProtocolHead::CrcCodeSize)
			{
				IcsException("first package's size:%d is not more than IcsMsgHead", (int)message.size());
			}

			// 根据消息ID判断处理类型
			protocol::MessageId msgid = message.getHead()->getMsgID();
			if (msgid > protocol::MessageId::T2C_min && msgid < protocol::MessageId::T2C_max)
			{
				m_msgHandler.reset(new TerminalHandler());
//				m_msgHandler = make_unique<TerminalHandler>();
			}
			else if (msgid > protocol::MessageId::W2C_min && msgid < protocol::MessageId::W2C_max)
			{
				m_msgHandler.reset(new WebHandler());
//				m_msgHandler = make_unique<WebHandler>();
			}
			else if (msgid == protocol::MessageId::T2T_forward_msg)
			{
//				m_msgHandler.reset(new ProxyTerminalHandler());
				m_msgHandler = make_unique<ProxyTerminalHandler>();
			}
			else
			{
				throw IcsException("unkown protocol");
			}
		}
	}

	bool handleData(uint8_t* buf, std::size_t length)
	{
		/// show debug info
		this->toHexInfo("recv", buf, length);

		try {
			while (length != 0)
			{
				/// assemble a complete ICS message and handle it
				if (m_request.assembleMessage(buf, length))
				{
					handleMessage(m_request);
					m_request.rewind();
				}
			}
		}
		catch (IcsException& ex)
		{
			LOG_ERROR(ex.message());
			return false;
		}

		return true;
	}

	void handleMessage(ProtocolStream&	request)
	{
		ProtocolHead* head = m_request.getHead();

		try {
			/// verify ics protocol
			m_request.verify();

			/// it must be one of ICS message
			assertIcsMessage(request);

			/// ignore response message from terminal
			if (head->isResponse())	
			{
				return;
			}


			ProtocolStream response(mp);

			/// handle message
//			m_msgHandler->handle(*this, m_request, response);

			/// prepare response
			if (head->needResposne())
			{
				response.getHead()->init(protocol::MessageId::MessageId_min, head->getSendNum());	// head->getMsgID()
			}

			/// send response message
			if (!response.empty() || head->needResposne())
			{
				response.getHead()->setSendNum(m_sendSerialNum++);
				response.serailzeToData();
				trySend(response.toMemoryChunk());
			}
		}
		catch (IcsException& ex)
		{
			throw IcsException("handle message [%04x] IcsException:", (uint32_t)head->getMsgID(), ex.message().c_str());
		}
		catch (otl_exception& ex)
		{
			throw IcsException("handle message [%04x] otl_exception:", (uint32_t)head->getMsgID(), ex.msg);
		}
		catch (std::exception& ex)
		{
			throw IcsException("handle message [%04x] std::exception:", (uint32_t)head->getMsgID(), ex.what());
		}
		catch (...)
		{
			throw IcsException("handle message [%04x] unknown error", (uint32_t)head->getMsgID());
		}
	}
private:
	// tcp socket
	socket	m_socket;

	// replaced by another TcpConnection
	bool	m_replaced;

	/// message handler
	std::unique_ptr<MessageHandlerImpl> m_msgHandler;

	/// connection name
	std::string             m_connName;

	/// client manager
	ClientManager<_thisType>&	m_client_manager;

	// recv area
//	uint8_t			m_recvBuff[512];
	std::array<uint8_t, 512> m_recvBuff;
	ProtocolStream	m_request;

	// send area
	std::list<MemoryChunk_ptr> m_sendList;
	std::mutex		m_sendLock;
	bool			m_isSending;
	uint16_t		m_sendSerialNum;
};


template<class ProtocolType>
class BaseConnection {
public:
	typedef typename asio::basic_stream_socket<ProtocolType> socket;

	BaseConnection(socket&& s) :m_socket(std::move(s)), m_request(mp)
	{

	}

	void start()
	{
		do_read();
	}

protected:

	void do_read()
	{
		// wait response
		m_socket.async_read_some(m_request.toBuffer(),
			[this](const std::error_code& ec, std::size_t length)
		{
			// no error and handle message
			if (!ec && (length > sizeof(ProtocolHead) + ProtocolHead::CrcCodeSize))
			{
				handleData(m_request, length);
			}

			// 自删除
			delete this;
		});
		
	}

	void handleData(ProtocolStream& request, std::size_t len)
	{
		
		protocol::MessageId msgid = message.getHead()->getMsgID();

		// 根据消息ID判断处理类型
		if (msgid > protocol::MessageId::T2C_min && msgid < protocol::MessageId::T2C_max)
		{
			m_msgHandler.reset(new TerminalHandler());
			//				m_msgHandler = make_unique<TerminalHandler>();
		}
		else if (msgid > protocol::MessageId::W2C_min && msgid < protocol::MessageId::W2C_max)
		{
			m_msgHandler.reset(new WebHandler());
			//				m_msgHandler = make_unique<WebHandler>();
		}
		else if (msgid == protocol::MessageId::T2T_forward_msg)
		{
			//				m_msgHandler.reset(new ProxyTerminalHandler());
			m_msgHandler = make_unique<ProxyTerminalHandler>();
		}
		else
		{
			throw IcsException("unkown protocol");
		}
	}
	
private:
	socket m_socket;
	std::array<uint8_t, 512> m_recvBuff;
	ProtocolStream	m_request;

};

class IcsConnectionHandler {
public:

};

}

#endif // _ICS_CONNECTION_H
