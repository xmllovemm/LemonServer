

#ifndef _CONNECTION_BASE_H
#define _CONNECTION_BASE_H

#include "config.hpp"
#include "icsprotocol.hpp"
#include "mempool.hpp"
#include "log.hpp"
#include <asio.hpp>
#include <asio/placeholders.hpp>
#include <functional>
#include <typeinfo>


extern ics::MemoryPool g_memoryPool;


namespace ics {

template<class Protocol>
class UnknownConnection;

template<class Protocol>
class IcsConnectionImpl {
public:

	typedef typename Protocol::socket socket;

	typedef typename socket::shutdown_type shutdown_type;

	IcsConnectionImpl(socket&& s, const uint8_t* data, std::size_t length)
		: m_socket(std::move(s))
		, m_request(g_memoryPool)
	{
		// 完整的消息
		if (m_request.getHead()->getLength() == length)
		{
			handleData(std::const_cast<uint8_t*>(data), length);
		}
		// 开始接收数据
		do_read();
	}

	virtual ~IcsConnectionImpl()
	{
		m_socket.close();
	}


	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception) = 0;

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception) = 0;

protected:
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
		//		m_socket.shutdown(type);
		delete this;
	}

	void toHexInfo(const char* info, const uint8_t* data, std::size_t length)
	{
#ifndef NDEBUG
		char buff[1024];
		for (size_t i = 0; i < length && i < sizeof(buff) / 3; i++)
		{
			std::sprintf(buff + i * 3, " %02x", data[i]);
		}
		LOG_DEBUG(m_connectionID << " " << info << " " << length << " bytes...");
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
					LOG_DEBUG(m_connectionID << " send data error");
					do_error(shutdown_type::shutdown_both);
				}
			});

		}
	}

	void trySend(MemoryChunk&& mc)
	{
		{
			std::lock_guard<std::mutex> lock(m_sendLock);
			m_sendList.push_back(std::move(mc));
		}
		trySend();
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
		IcsMsgHead* head = m_request.getHead();

		try {
			/// verify ics protocol
			m_request.verify();

			/// ignore response message from terminal
			if (head->isResponse())
			{
				return;
			}


			ProtocolStream response(g_memoryPool);

			/// handle message
			this->handle(m_request, response);

			/// prepare response
			if (head->needResposne())
			{
				response.getHead()->init(MessageId::MessageId_min, head->getSendNum());	// head->getMsgID()
			}

			/// send response message
			if (!response.empty() || head->needResposne())
			{
				response.getHead()->setSendNum(m_serialNum++);
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

protected:
	friend class UnknownConnection<Protocol>;
	// connect socket
	socket	m_socket;

	std::string m_connectionID;

	// recv area
	ProtocolStream m_request;
	std::array<uint8_t, 512> m_recvBuff;

	// send area
	uint16_t m_serialNum;
	std::list<MemoryChunk> m_sendList;
	std::mutex		m_sendLock;
	bool			m_isSending;

};

template<class Protocol>
class IcsTerminal : public IcsConnectionImpl<Protocol>
{
public:
	typedef IcsConnectionImpl<Protocol>		_baseType;
	typedef typename _baseType::socket		socket;
	typedef typename socket::shutdown_type	shutdown_type;

	IcsTerminal(socket&& s, const uint8_t* data, std::size_t length)
		: _baseType(std::move(s), data, length)
	{

	}

	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		LOG_DEBUG("IcsTerminal handle");
	}

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
	{
		LOG_DEBUG("IcsTerminal dispatch");
	}
};

template<class Protocol>
class IcsWeb : public IcsConnectionImpl<Protocol>
{
public:
	typedef IcsConnectionImpl<Protocol>		_baseType;
	typedef typename _baseType::socket		socket;
	typedef typename socket::shutdown_type	shutdown_type;

	IcsWeb(socket&& s, const uint8_t* data, std::size_t length)
		: _baseType(std::move(s), data, length)
	{

	}

	// 处理底层消息
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception)
	{
		LOG_DEBUG("IcsWeb handle");
	}

	// 处理平层消息
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception)
	{
		LOG_DEBUG("IcsWeb dispatch");
	}
};


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
			if (msgid > MessageId::T2C_min && msgid < MessageId::T2C_max)
			{
				IcsConnectionImpl<Protocol>* ic = new IcsTerminal<Protocol>(std::move(m_socket), m_recvBuff.data(), len);
			}
			else if (msgid > MessageId::W2C_min && msgid < MessageId::W2C_max)
			{
				//m_msgHandler = make_unique<WebHandler>();
			}
			else if (msgid == MessageId::T2T_forward_msg)
			{
				//m_msgHandler = make_unique<ProxyTerminalHandler>();
			}
			else
			{
				LOG_WARN("unknow message id = " << msgid);
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
#endif //
