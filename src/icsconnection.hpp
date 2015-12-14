
#ifndef _ICS_CONNECTION_H
#define _ICS_CONNECTION_H

#include "icsprotocol.hpp"
#include "mempool.hpp"
#include "log.hpp"
#include "config.hpp"
#include "timer.hpp"
#include <asio.hpp>
#include <cstdio>

extern ics::MemoryPool g_memoryPool;

namespace ics {

typedef asio::ip::tcp icstcp;

typedef asio::ip::udp icsudp;

/// ICSЭ�鴦�����
template<class Protocol>
class IcsConnection : public std::enable_shared_from_this<IcsConnection<Protocol>> {
public:

	typedef typename Protocol::socket socket;

	typedef typename socket::shutdown_type shutdown_type;

	/// s:�׽��֣�name:������
	IcsConnection(socket&& s, const char* name )
		: m_socket(std::move(s))
		, m_valid(true)
		, m_replaced(false)
//		, m_request(g_memoryPool)
		, m_recvSize(0)
		, m_msgHead(nullptr)
		, m_needSize(sizeof(IcsMsgHead))
		, m_serialNum(0)
		, m_isSending(false)
		, m_timeoutCount(0)
	{
		char buff[126];
		auto endpoint = m_socket.remote_endpoint();
		if (name)
		{
			std::sprintf(buff, "%s@%s:%d", name, endpoint.address().to_string().c_str(), endpoint.port());
		}
		else
		{
			std::sprintf(buff, "%s:%d", endpoint.address().to_string().c_str(), endpoint.port());
		}

		m_name = buff;

		LOG_DEBUG("Create the connection " << m_name);
	}

	virtual ~IcsConnection()
	{
		m_socket.close();
		LOG_DEBUG("Destroy the connection " << m_name);
	}

	// �������ݿ�ʼ�¼�
	void start(const uint8_t* data, std::size_t length)
	{
		// �������Ϣ
		if (handleData(const_cast<uint8_t*>(data), length))
		{

		}
		// ��ʼ��������
		do_read();

	}

	// �޳�ʼ���ݿ�ʼ�¼�
	void start()
	{
		// ��ʼ��������
		do_read();
	}

	const std::string& name() const
	{
		return m_name;
	}

	void replaced()
	{
		m_replaced = true;
		m_valid = false;
		do_error();
	}

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception) = 0;

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception) = 0;

	// ������
	virtual void error() throw()
	{

	}

	/// ��ʱ����:����ֵΪ�´�֪ͨ�����0-����֪ͨ
	int timeout(int interval)
	{
		LOG_DEBUG(m_name << " timeout count " << m_timeoutCount + 1);

		if (!m_valid)
		{
			interval = 0;
		}
		else if (++m_timeoutCount > m_timeoutMax)
		{
			LOG_DEBUG(m_name << " timeout");
			interval = 0;
			do_error();
		}
		
		return interval;
	}

protected:
	/// ��������
	void trySend(ProtocolStream& msg)
	{
		msg.serialize(m_serialNum++);
		{
			std::lock_guard<std::mutex> lock(m_sendLock);
			m_sendList.push_back(msg.toMemoryChunk());
		}
		trySend();
	}

	/// ���ø�������
	void setName(const std::string& name)
	{
		this->m_name = name;
	}

	/// �����Ƿ���Ч
	bool isValid()
	{
		return m_valid;
	}

	/// Ͷ�ݶ�����
	void do_read()
	{
		// wait response
		auto self(this->shared_from_this());
		m_socket.async_receive(asio::buffer(m_recvBuff + m_recvSize, sizeof(m_recvBuff)-m_recvSize)
			, [self](const std::error_code& ec, std::size_t length)
		{
			// no error and handle message
			if (!ec && self->handleData(length))
			{
				// continue to read
				self->do_read();
			}
			else
			{
				/*
				if (ec)
				{
					LOG_WARN(this->m_name << " read error: " << ec.message());
				}
				*/
				self->do_error();
			}
		});
	}

	/// Ͷ��д����
	void do_write()
	{
		trySend();
	}

	/// ����
	void do_error()
	{
//		LOG_DEBUG(m_name << " is closing...");
		m_valid = false;
		error();
		m_socket.close();
	}

	/// ʮ��������ʾ
	void toHexInfo(const char* info, const uint8_t* data, std::size_t length)
	{
#ifndef NDEBUG
		char buff[1024];
		for (size_t i = 0; i < length && i < sizeof(buff) / 3; i++)
		{
			std::sprintf(buff + i * 3, " %02x", data[i]);
		}
		LOG_DEBUG(info << " [" << m_name << "] " << length << " bytes...");
#endif
	}

	/// ���Է�������
	void trySend()
	{
		std::lock_guard<std::mutex> lock(m_sendLock);
		if (!m_isSending && !m_sendList.empty())
		{

			MemoryChunk& block = m_sendList.front();
			auto self(this->shared_from_this());

			m_socket.async_send(asio::buffer(block.data, block.length),
				[self](const std::error_code& ec, std::size_t length)
			{
				if (!ec)
				{
					MemoryChunk& chunk = self->m_sendList.front();
					self->toHexInfo("send to", chunk.data, chunk.length);
					g_memoryPool.put(chunk);
					self->m_sendList.pop_front();
					self->m_isSending = false;
					self->trySend();
				}
				else
				{
					LOG_DEBUG("send data error");
					self->do_error();
				}
			});

		}
	}

	/// ��������ݶ�
	bool handleData(std::size_t length)
	{
		/// show debug info
		this->toHexInfo("recv from", m_recvBuff + m_recvSize , length);

		try {
			if (m_msgHead)
			{
//				if (m_recvSize + length > m_needSize)
			}

			/// assemble a complete ICS message and handle it
			handleMessage(ProtocolStream(ProtocolStream::OptType::readType, m_recvBuff, m_recvSize));
		}
		catch (IcsException& ex)
		{
			LOG_ERROR(ex.message());
			return false;
		}

		return true;
	}

	/// �������������Ϣ
	void handleMessage(ProtocolStream&	request)
	{
		// ��0��ʱ����
		m_timeoutCount = 0;

		///*
		IcsMsgHead* head = request.getHead();

		try {
			/// verify ics protocol
			request.verify();

			/// ignore response message from terminal
			if (head->isResponse())
			{
				return;
			}

			MemoryChunk chunk = g_memoryPool.get();
			ProtocolStream response(std::move(chunk));

			/// handle message
			handle(request, response);

			/// send response message
			if (response.getHead()->getMsgID() != MessageId::MessageId_min)
			{	
				trySend(response);
			}
			else if (head->needResposne())
			{
				response.initHead(MessageId::MessageId_min, head->getSendNum());	// head->getMsgID()
				trySend(response);
			}

		}
		catch (IcsException& ex)
		{
			throw IcsException("%s handle message [0x%04x] IcsException: %s", m_name.c_str(), (uint16_t)head->getMsgID(), ex.message().c_str());
		}
		catch (otl_exception& ex)
		{
			throw IcsException("%s handle message [0x%04x] otl_exception: %s", m_name.c_str(), (uint16_t)head->getMsgID(), ex.msg);
		}
		catch (std::exception& ex)
		{
			throw IcsException("%s handle message [0x%04x] std::exception: %s", m_name.c_str(), (uint16_t)head->getMsgID(), ex.what());
		}
		catch (...)
		{
			throw IcsException("%s handle message [04%x] unknown error", m_name.c_str(), (uint16_t)head->getMsgID());
		}
		//*/
	}

protected:	
	/// �����׽���
	socket	m_socket;

	/// ��������Ĭ��Ϊ�Զ˵ĵ�ַ����ʽΪ"ip:port"
	std::string	m_name;

	/// �����Ƿ���Ч
	bool m_valid;

	/// �Ƿ���ͬ�����滻
	bool			m_replaced;
private:
	// recv area
//	ProtocolStream		m_request;
	uint8_t				m_recvBuff[1024];
	/// �ѽ������ݴ�С
	std::size_t			m_recvSize;
	IcsMsgHead*			m_msgHead;
	std::size_t			m_needSize;

	// send area
	uint16_t m_serialNum;
	std::list<MemoryChunk> m_sendList;
	std::mutex		m_sendLock;
	bool			m_isSending;


	// ��ʱ������: ÿ�ν��յ�һ������Ϣ��0����ʱһ�μ�1��������������������Ч
	int				m_timeoutCount;
	static const int m_timeoutMax = 2;
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

		// ���ۺ��ֽ��ɾ������
		delete this;
	}

	socket	m_socket;
	std::array<uint8_t, 64> m_recvBuff;
};


}
#endif // _ICS_CONNECTION_H
