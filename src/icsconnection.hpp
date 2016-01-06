
#ifndef _ICS_CONNECTION_H
#define _ICS_CONNECTION_H

#include "icsprotocol.hpp"
#include "mempool.hpp"
#include "log.hpp"
#include "config.hpp"
#include "otlv4.h"
#include "timer.hpp"
#include "util.hpp"
#include <asio.hpp>
#include <cstdio>


extern ics::MemoryPool g_memoryPool;

namespace ics {

typedef asio::ip::tcp icstcp;

typedef asio::ip::udp icsudp;

/// ICSЭ�鴦�����
template<class Protocol>
class IcsConnection : public std::enable_shared_from_this<IcsConnection<Protocol>>, NonCopyable {
public:

	typedef typename Protocol::socket socket;

	typedef typename socket::shutdown_type shutdown_type;

	/// s:�׽��֣�name:������
	IcsConnection(socket&& s, const char* name )
		: m_socket(std::move(s))
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
		do_error();
	}

	/// �����Ƿ���Ч
	bool isValid()
	{
		return m_valid;
	}

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception) = 0;

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception) = 0;

	// ������
	virtual void error() throw()
	{

	}

	/// ��ʱ����:true-��Ч��false-��Ч
	bool timeout() throw()
	{
		if (m_valid && ++m_timeoutCount > m_timeoutMax)
		{
			do_error();
		}	
		return m_valid;
	}

	/// ����
	void do_error()
	{
		if (m_valid)
		{
			m_valid = false;
			error();	/// ֪ͨ�ϲ�Ӧ�ó���	
			asio::error_code ec;
			m_socket.close(ec);		/// �ر�����
		}
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


	/// �Ƿ���ͬ�����滻
	bool m_replaced = false;

private:

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
			m_isSending = true;
			MemoryChunk& block = m_sendList.front();
			auto self(this->shared_from_this());
			m_socket.async_send(asio::buffer(block.data, block.length),
				[self](const std::error_code& ec, std::size_t length)
			{
				if (!ec)
				{
					{
						std::lock_guard<std::mutex> lock(self->m_sendLock);
						MemoryChunk& chunk = self->m_sendList.front();
						self->toHexInfo("send to", chunk.data, chunk.length);
						g_memoryPool.put(chunk);
						self->m_sendList.pop_front();
						self->m_isSending = false;
					}
					self->trySend();
				}
				else
				{
					LOG_DEBUG(self->m_name << " send data error");
					self->do_error();
				}
			});

		}
	}

	/// ��������յ������ݶ�
	bool handleData(std::size_t length)
	{
		bool ret = true;
		IcsMsgHead*	head = (IcsMsgHead*)m_recvBuff;

		/// show debug info
		this->toHexInfo("recv from", m_recvBuff + m_recvSize , length);

		m_recvSize += length;

		while (ret && m_recvSize >= sizeof(IcsMsgHead)+IcsMsgHead::CrcCodeSize)
		{
			uint16_t msgLen = head->getLength();
			/// ��Ϣ���ȼ��
			if (msgLen > sizeof(m_recvBuff) || msgLen < sizeof(IcsMsgHead)+IcsMsgHead::CrcCodeSize)
			{
				LOG_ERROR("length of message error:" << msgLen << " ,max length of buffer is:" << sizeof(m_recvBuff));
				ret = false;
				break;
			}
			
			if (m_recvSize >= msgLen)	// ��һ��������Ϣ
			{
				ret = handleMessage(m_recvBuff, msgLen);
				m_recvSize -= msgLen;
				head = (IcsMsgHead*)((uint8_t*)head + msgLen);
			}
			else // ����һ��������Ϣ
			{
				if (m_recvBuff != (uint8_t*)head)
				{				
					LOG_DEBUG(m_name << " move " << m_recvSize << " bytes");
					std::memmove(m_recvBuff, head, m_recvSize);
				}
				break;
			}
		}

		return ret;
	}

	/// �������������Ϣ
	bool handleMessage(void* data, std::size_t len)
	{
		bool ret = false;
		IcsMsgHead* head = (IcsMsgHead*)data;
		try {
			ProtocolStream request(ProtocolStream::OptType::readType, data, len);

			// ��0��ʱ����
			m_timeoutCount = 0;

			/// �����ն˵���Ӧ��Ϣ,FIXME
			if (head->isResponse())
			{
				LOG_DEBUG(m_name << " ignore response message");
				return true;
			}

			ProtocolStream response(ProtocolStream::OptType::writeType, g_memoryPool.get());

			/// ͨ���麯���������Ϣ
			handle(request, response);

			/// send response message
			if (response.getHead()->getMsgID() != MessageId::MessageId_min_0x0000)
			{	
				trySend(response);
			}
			else if (head->needResposne())
			{
				response.initHead(MessageId::MessageId_min_0x0000, head->getSendNum());	// head->getMsgID()
				trySend(response);
			}
			ret = true;
		}
		catch (IcsException& ex)
		{
			LOG_ERROR(m_name << " occurs IcsException: id=" << (uint16_t)head->getMsgID() << ",error=" << ex.message());
		}
		catch (otl_exception& ex)
		{
			LOG_ERROR(m_name << " occurs otl_exception: id=" << (uint16_t)head->getMsgID() << ",error=" << ex.msg);
		}
		catch (std::exception& ex)
		{
			LOG_ERROR(m_name << " occurs std::exception: id=" << (uint16_t)head->getMsgID() << ",error=" << ex.what());
		}
		catch (...)
		{
			LOG_ERROR(m_name << " occurs unkonw exception: id=" << (uint16_t)head->getMsgID());
		}

		return ret;
	}

	/// �����׽���
	socket	m_socket;

private:
	/// �����Ƿ���Ч
//	std::atomic<bool> m_valid = true;
//	std::atomic<bool>			m_isSending = false;
	bool m_valid = true;
	bool m_isSending = false;
	// recv area
	uint8_t				m_recvBuff[1024];
	/// �ѽ������ݴ�С
	std::size_t			m_recvSize = 0;

	// send area
	uint16_t m_serialNum = 0;
	std::list<MemoryChunk> m_sendList;
	std::mutex		m_sendLock;

	/// ��������Ĭ��Ϊ�Զ˵ĵ�ַ����ʽΪ"ip:port"
	std::string	m_name;

	// ��ʱ������: ÿ�ν��յ�һ������Ϣ��0����ʱһ�μ�1��������������������Ч
	int				m_timeoutCount = 0;
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
			if (msgid > MessageId::T2C_min_0x0100 && msgid < MessageId::T2C_max)
			{
//				ic = new IcsTerminal<Protocol>(std::move(m_socket));
			}
			else if (msgid > MessageId::W2C_min_0x2000 && msgid < MessageId::W2C_max)
			{
//				ic = new IcsWeb<Protocol>(std::move(m_socket));
			}
			else if (msgid == MessageId::C2C_forward_to_terminal_0x4004)
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
