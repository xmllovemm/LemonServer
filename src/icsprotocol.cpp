

#include "icsprotocol.h"
#include <string.h>


namespace ics{

template<typename T>
inline T IcsProtocol::ics_ntoh(T n)
{
#ifdef ICS_USE_LITTLE_ENDIAN
	return n;
#else
	return ntohs(n);
#endif
}

template<typename T>
inline T IcsProtocol::ics_hton(T n)
{
#ifdef ICS_USE_LITTLE_ENDIAN
	return n;
#else
	return ntohs(n);
#endif
}

IcsProtocol::IcsProtocol(void* buf, size_t length)
{
	m_start = m_pos = (uint8_t*)buf;
	m_end = m_start + length;
}

void IcsProtocol::reset()
{
	m_start = m_pos = m_end;
}

		
// set data
void IcsProtocol::serailzeToData()
{
	// set length
	((IcsMsgHead*)m_start)->length = ics_hton((uint16_t)(m_pos - m_start + sizeof(uint16_t)));

	// set crc32 code
	*this << getCrc32Code(m_start, m_pos - m_start);
	m_pos += sizeof(uint16_t);
}

void IcsProtocol::initHead(uint16_t id, uint16_t send_num, uint16_t ack_num, bool request, bool response, EncryptionType et)
{
	IcsMsgHead* mh = (IcsMsgHead*)m_start;
	memset(mh, 0, sizeof(IcsMsgHead));

	memcpy(mh->name, ICS_HEAD_PROTOCOL_NAME, ICS_HEAD_PROTOCOL_NAME_LEN);
	mh->version = ics_hton((uint16_t)ICS_HEAD_PROTOCOL_VERSION);
	mh->id = ics_hton(id);
	mh->send_num = ics_hton(send_num);
	mh->ack_num = ics_hton(ack_num);
	mh->encrypt = et;
	mh->ack = (request ? 0 : 1);
	mh->response = (response ? 1 : 0);

	m_pos = m_start + sizeof(IcsMsgHead);
}

void IcsProtocol::initHead(uint16_t id, uint16_t send_num)
{
	initHead(id, send_num, 0, true, true, IcsProtocol::EncryptionType_none);
}

void IcsProtocol::initHead(uint16_t ack_num)
{
	initHead(0, 0, ack_num, false, false, IcsProtocol::EncryptionType_none);
}

IcsProtocol& IcsProtocol::operator << (const uint8_t& data)
{
	if (sizeof(data) > leftLength())
	{
		throw overflow_error("OOM to set uint8_t data");
	}

	*m_pos++ = data;
	return *this;
}

IcsProtocol& IcsProtocol::operator << (const uint16_t& data)
{
	if (sizeof(data) > leftLength())
	{
		throw overflow_error("OOM to set uint16_t data");
	}

	*(uint16_t*)m_pos = ics_hton(data);
	m_pos += sizeof(data);
	return *this;
}

IcsProtocol& IcsProtocol::operator << (const IcsDataTime& data)
{
	if (sizeof(data) > leftLength())
	{
		throw overflow_error("OOM to set IcsDataTime data");
	}

	IcsDataTime* dt = (IcsDataTime*)m_pos;
	*dt = data;
	dt->year = ics_hton(data.year);

	m_pos += sizeof(IcsDataTime);
	return *this;
}

IcsProtocol& IcsProtocol::operator << (const string& data)
{
	*this << (uint8_t)data.length();

	if (data.length() > leftLength())
	{
		throw overflow_error("OOM to put string data");
	}
	memcpy(m_pos, data.data(), data.length());
	m_pos += data.length();
	return *this;
}

template<typename T>
void IcsProtocol::setString(const string& data)
{
	T len = (T)data.length();
	*this << len;

	if (len > (T)leftLength())
	{
		throw underflow_error("OOM to set string data");
	}
	memcpy(m_pos, data.data(), data.length());
	m_pos += data.length();
}


// get data
void IcsProtocol::parseFormData(void* buf, int length)
{
	m_start = m_pos = (uint8_t*)buf;
	m_end = m_start + length;
}

IcsProtocol::IcsMsgHead* IcsProtocol::getHead()
{
	IcsMsgHead* mh = (IcsMsgHead*)m_start;
	mh->version = ics_ntoh(mh->version);
	mh->id = ics_ntoh(mh->id);
	mh->send_num = ics_ntoh(mh->send_num);
	mh->ack_num = ics_ntoh(mh->ack_num);

	m_pos = m_start + sizeof(IcsMsgHead);
	return mh;
}

IcsProtocol& IcsProtocol::operator >> (uint8_t& data)
{
	if (sizeof(data) > leftLength())
	{
		throw underflow_error("OOM to get uint8_t data");
	}
	data = *m_pos++;
	return *this;
}

IcsProtocol& IcsProtocol::operator >> (uint16_t& data)
{
	if (sizeof(data) > leftLength())
	{
		throw underflow_error("OOM to get uint16_t data");
	}
	data = *(uint16_t*)m_pos;
	m_pos += sizeof(data);
	return *this;
}

IcsProtocol& IcsProtocol::operator >> (IcsDataTime& data)
{
	if (sizeof(data) > leftLength())
	{
		throw underflow_error("OOM to get IcsDataTime data");
	}
	IcsDataTime* dt = (IcsDataTime*)m_pos;
	data = *dt;
	data.year = ics_ntoh(dt->year);

	m_pos += sizeof(IcsDataTime);
	return *this;
}

IcsProtocol& IcsProtocol::operator >> (string& data)
{
	uint8_t len = 0;
	*this >> len;

	if (len > leftLength())
	{
		throw underflow_error("OOM to get string data");
	}
	data.assign((char*)m_pos, len);
	m_pos += len;
	return *this;
}

template<typename T>
string IcsProtocol::getString()
{
	string str;
	T len;
	*this >> len;
	if (len > leftLength())
	{
		throw underflow_error("OOM to get string data");
	}

	str.assign((char*)m_pos, len);
	m_pos += len;
	return std::move(str);
}

// calcutlate the crc code
uint16_t IcsProtocol::getCrc32Code(uint8_t* buf, int length)
{

	return 0;
}

}	// end namespace ics
