
#ifndef _ICS_PUSH_SYSTEM_H
#define _ICS_PUSH_SYSTEM_H

#include "icsconnection.hpp"
#include <unordered_map>

namespace ics {

/// ������Ϣ������
class PushMsgConnection : public IcsConnection<icsudp>
{
public:
	typedef IcsConnection<icsudp>	_baseType;

	typedef _baseType::socket		socket;

	PushMsgConnection(socket&& s);

	// ����ײ���Ϣ
	virtual void handle(ProtocolStream& request, ProtocolStream& response) throw(IcsException, otl_exception);

	// ����ƽ����Ϣ
	virtual void dispatch(ProtocolStream& request) throw(IcsException, otl_exception);
};

/// ����ϵͳ
class PushSystem {
public:
	PushSystem(asio::io_service& ioService, const std::string& addr);

	~PushSystem();

	void send(ProtocolStream& request);
private:

	std::unique_ptr<PushMsgConnection>	m_connection;
	std::unordered_map<uint16_t, MemoryChunk> m_msgList;

	asio::ip::udp::endpoint		m_serverEndpoint;
};


}

#endif // _ICS_PUSH_SYSTEM_H
