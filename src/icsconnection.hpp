

#ifndef _ICS_CONNECTION_H
#define _ICS_CONNECTION_H

#include "icsprotocol.hpp"
#include "mempool.hpp"
#include "log.hpp"
#include "config.hpp"
#include <asio.hpp>
#include <cstdio>

extern ics::MemoryPool g_memoryPool;

namespace ics {

typedef asio::ip::tcp icstcp;

typedef asio::ip::udp icsudp;

/// ICS协议处理基类
template<class Protocol>
class IcsConnection {
public:

	typedef typename Protocol::socket socket;

	typedef typename socket::shutdown_type shutdown_type;

	IcsConnection(socket&& s)
		: m_socket(std::move(s))
		, m_request(g_memoryPool)
		, m_serialNum(0)
		, m_isSending(false)
		, m_replaced(false)
	{
		char buff[64];
		auto endpoint = m_socket.remote_endpoint();
		std::sprintf(buff, "%s:%d", endpoint.address().to_string().c_str(), endpoint.port());
		m_name = buff;

		LOG_DEBUG("Create the connection " << m_name);
	}

	virtual ~IcsConnection()
	{
		m_socket.close();
		LOG_DEBUG("Destroy the connection " << m_name);
	}

	// 按该数据开始事件
	void start(const uint8_t* data, std::size_t length)
	{
		m_request.reset();

		// 处理该消息
		if (handleData(const_cast<uint8_t*>(data), length))
		{

		}
		// 开始接收数据
		do_read();

	}

	// 无初始数据开始事件
	void start()
	{
		m_request.reset();

		// 开始接收数据
		do_read();
	}

	void replaced(bool flag = true)
	{
		m_replaced = flag;
	}

	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception) = 0;

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception) = 0;

protected:
	void trySend(ProtocolStream& msg)
	{
		msg.getHead()->setSendNum(m_serialNum++);
		msg.serailzeToData();
		{
			std::lock_guard<std::mutex> lock(m_sendLock);
			m_sendList.push_back(msg.toMemoryChunk());
		}
		trySend();
	}

private:
	void do_read()
	{
		// wait response
		m_socket.async_receive(asio::buffer(m_recvBuff),
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
				LOG_WARN(this->m_name << " post read error: " << ec.message());
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
	}

	void toHexInfo(const char* info, const uint8_t* data, std::size_t length)
	{
#ifndef NDEBUG
		char buff[1024];
		for (size_t i = 0; i < length && i < sizeof(buff) / 3; i++)
		{
			std::sprintf(buff + i * 3, " %02x", data[i]);
		}
		LOG_DEBUG(m_name << " " << info << " " << length << " bytes...");
#endif
	}

	void trySend()
	{
		std::lock_guard<std::mutex> lock(m_sendLock);
		if (!m_isSending && !m_sendList.empty())
		{

			MemoryChunk& block = m_sendList.front();
			m_socket.async_send(asio::buffer(block.data, block.length),
				[this](const std::error_code& ec, std::size_t length)
			{
				if (!ec)
				{
					MemoryChunk& chunk = m_sendList.front();
					this->toHexInfo("send", chunk.data, chunk.length);
					m_sendList.pop_front();
					m_isSending = false;
					trySend();
				}
				else
				{
					LOG_DEBUG("send data error");
					do_error(shutdown_type::shutdown_both);
				}
			});

		}
	}

	bool handleData(uint8_t* data, std::size_t length)
	{
		/// show debug info
		this->toHexInfo("recv", data, length);

		try {
			while (length != 0)
			{
				/// assemble a complete ICS message and handle it
				if (m_request.assembleMessage(data, length))
				{
					handleMessage(m_request);
					m_request.reset();
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
		// 置0超时计数
		m_timeoutCount = 0;

		IcsMsgHead* head = request.getHead();

		try {
			/// verify ics protocol
			request.verify();

			/// ignore response message from terminal
			if (head->isResponse())
			{
				return;
			}


			ProtocolStream response(g_memoryPool);

			/// handle message
			handle(request, response);

			/// prepare response
			if (head->needResposne())
			{
				response.getHead()->init(MessageId::MessageId_min, head->getSendNum());	// head->getMsgID()
			}

			/// send response message
			if (!response.empty() || head->needResposne())
			{			
				trySend(response);
			}
		}
		catch (IcsException& ex)
		{
			throw IcsException("%s handle message [%04x] IcsException: %s", m_name.c_str(), (uint16_t)head->getMsgID(), ex.message().c_str());
		}
		catch (otl_exception& ex)
		{
			throw IcsException("%s handle message [%04x] otl_exception: %s", m_name.c_str(), (uint16_t)head->getMsgID(), ex.msg);
		}
		catch (std::exception& ex)
		{
			throw IcsException("%s handle message [%04x] std::exception: %s", m_name.c_str(), (uint16_t)head->getMsgID(), ex.what());
		}
		catch (...)
		{
			throw IcsException("%s handle message [%04x] unknown error", m_name.c_str(), (uint16_t)head->getMsgID());
		}
	}

protected:	
	// connect socket
	socket	m_socket;

	// remote addr in dotted decimal format
	std::string	m_name;

	bool			m_replaced;
private:
	// recv area
	ProtocolStream m_request;
	std::array<uint8_t, 512> m_recvBuff;

	// send area
	uint16_t m_serialNum;
	std::list<MemoryChunk> m_sendList;
	std::mutex		m_sendLock;
	bool			m_isSending;


	// 超时计数器: 每次接收到一完整消息置0，超时一次加1，超过3次后链接无效
	int				m_timeoutCount;
};


/// origin connection
template<class Protocol>
class UnknownConnection {
public:
	typedef typename Protocol::socket socket;

	UnknownConnection(socket&& s)
		: m_socket(std::move(s))
	{

	}

	void start()
	{
		m_socket.async_receive(asio::buffer(m_recvBuff)
			, [this](const asio::error_code& ec, std::size_t len)
		{
			readHanler(ec, len);
		});
	}

private:

	void readHanler(const asio::error_code& ec, std::size_t len)
	{
		if (!ec && len > sizeof(IcsMsgHead)+IcsMsgHead::CrcCodeSize)
		{
			auto msgid = ((IcsMsgHead*)m_recvBuff.data())->getMsgID();
			IcsConnection<Protocol>* ic;
			if (msgid > MessageId::T2C_min && msgid < MessageId::T2C_max)
			{
//				ic = new IcsTerminal<Protocol>(std::move(m_socket));
			}
			else if (msgid > MessageId::W2C_min && msgid < MessageId::W2C_max)
			{
//				ic = new IcsWeb<Protocol>(std::move(m_socket));
			}
			else if (msgid == MessageId::T2T_forward_msg)
			{
//				IcsConnection<Protocol>* ic = new IcsTerminal<Protocol>(std::move(m_socket));
			}
			else
			{
				LOG_WARN("unknow message id = " << msgid);
			}

			if (ic)
			{
				ic->start(m_recvBuff.data(), len);
			}
		}
		else
		{
			LOG_WARN("need more data, length = " << len);
		}

		// 不论何种结果删除自身
		delete this;
	}

	socket	m_socket;
	std::array<uint8_t, 64> m_recvBuff;
};


}
#endif // _ICS_CONNECTION_H
