

#include "icsprotocol.hpp"
#include "util.hpp"
#include <string.h>


namespace ics{
	namespace protocol{

/*-----------------------------IcsMsgHead-------------------------------------*/

void IcsMsgHead::verify() throw(std::logic_error)
{
	if (std::memcmp(name, ICS_HEAD_PROTOCOL_NAME, ICS_HEAD_PROTOCOL_NAME_LEN) != 0)
	{
		throw std::logic_error("error protocol name");
	}
	if (getVersion() != ICS_HEAD_PROTOCOL_VERSION)
	{
		throw std::logic_error("error protocol version");
	}

}

// set 0
void IcsMsgHead::clean()
{
	memset(this, 0, sizeof(IcsMsgHead));
}

void IcsMsgHead::setMsgID(uint16_t id)
{
	this->id = ics_hton(id);
}

void IcsMsgHead::setLength(uint16_t len)
{
	this->length = ics_hton(len);
}

void IcsMsgHead::setSendNum(uint16_t num)
{
	this->send_num = ics_hton(num);
}

void IcsMsgHead::setAckNum(uint16_t num)
{
	this->ack_num = ics_hton(num);
}

void IcsMsgHead::setFlag(uint8_t encrypt, bool ack, bool response)
{
//	this->id = ics_hton(encrypt);
}

void IcsMsgHead::setCrcCode()
{
	ics_crccode_t code = 0;
	*(ics_crccode_t*)((char*)this + getLength() - sizeof(ics_crccode_t)) = ics_hton(code);
}

uint16_t IcsMsgHead::getMsgID()
{
	return ics_ntoh(this->id);
}
uint16_t IcsMsgHead::getLength()
{
	return ics_ntoh(this->length);
}

uint16_t IcsMsgHead::getSendNum()
{
	return ics_ntoh(this->send_num);
}

uint16_t IcsMsgHead::getAckNum()
{
	return ics_ntoh(this->ack_num);
}

uint16_t IcsMsgHead::getFlag()
{
	return ics_ntoh(this->id);
}

uint16_t IcsMsgHead::getVersion()
{
	return ics_ntoh(this->version);
}

ics_crccode_t IcsMsgHead::getCrcCode()
{
	return ics_ntoh(*(ics_crccode_t*)((char*)this + getLength() - sizeof(ics_crccode_t)));
}
/*-----------------------------IcsProtocol-------------------------------------*/


IcsProtocol::IcsProtocol(void* buf, size_t length)
{
	m_head = (IcsMsgHead*)buf;
	m_pos = (uint8_t*)(m_head + 1);
	m_end = (uint8_t*)buf + length;
}


// set data
void IcsProtocol::serailzeToData()
{
	m_head->setLength(m_pos - (uint8_t*)m_head + sizeof(ics_crccode_t));
	*this << crc32_code(m_head, m_pos - (uint8_t*)m_head);
}

void IcsProtocol::initHead(uint16_t id, uint16_t send_num, uint16_t ack_num, bool request, bool response, EncryptionType et)
{
	IcsMsgHead* mh = (IcsMsgHead*)m_head;
	memset(mh, 0, sizeof(IcsMsgHead));
	/*
	memcpy(mh->name, ICS_HEAD_PROTOCOL_NAME, ICS_HEAD_PROTOCOL_NAME_LEN);
	mh->version = ics_hton((uint16_t)ICS_HEAD_PROTOCOL_VERSION);
	mh->id = ics_hton(id);
	mh->send_num = ics_hton(send_num);
	mh->ack_num = ics_hton(ack_num);
	mh->encrypt = et;
	mh->ack = (request ? 0 : 1);
	mh->response = (response ? 1 : 0);
	*/
	//	m_pos = m_start + sizeof(IcsMsgHead);
}

void IcsProtocol::initHead(uint16_t id, uint16_t send_num)
{
	initHead(id, send_num, 0, true, true, IcsProtocol::EncryptionType_none);
}

void IcsProtocol::initHead(uint16_t ack_num)
{
	initHead(0, 0, ack_num, false, false, IcsProtocol::EncryptionType_none);
}

template<class T, class Cond = std::enable_if<sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8>::type>
IcsProtocol& IcsProtocol::operator << (T&& data) throw(std::overflow_error)
{
	if (sizeof(data) > leftSize())
	{
		throw overflow_error("OOM to set data");
	}

	*(T*)m_pos = ics_hton(std::forward<T>(data));
	m_pos += sizeof(data);
	return *this;
}

/*
IcsProtocol& IcsProtocol::operator << (uint8_t data) throw(std::overflow_error)
{
	if (sizeof(data) > leftSize())
	{
		throw overflow_error("OOM to set uint8_t data");
	}

	*m_pos++ = data;
	return *this;
}

IcsProtocol& IcsProtocol::operator << (uint16_t data) throw(std::overflow_error)
{
	if (sizeof(data) > leftSize())
	{
		throw overflow_error("OOM to set uint16_t data");
	}

	*(uint16_t*)m_pos = ics_hton(data);
	m_pos += sizeof(data);
	return *this;
}


IcsProtocol& IcsProtocol::operator << (uint32_t data) throw(std::overflow_error)
{
	if (sizeof(data) > leftSize())
	{
		throw overflow_error("OOM to set uint16_t data");
	}

	*(uint32_t*)m_pos = ics_hton(data);
	m_pos += sizeof(data);
	return *this;
}
*/

IcsProtocol& IcsProtocol::operator << (const IcsDataTime& data) throw(std::overflow_error)
{
	if (sizeof(data) > leftSize())
	{
		throw overflow_error("OOM to set IcsDataTime data");
	}

	IcsDataTime* dt = (IcsDataTime*)m_pos;
	*dt = data;
	dt->year = ics_hton(data.year);

	m_pos += sizeof(IcsDataTime);
	return *this;
}

IcsProtocol& IcsProtocol::operator << (const string& data) throw(std::overflow_error)
{
	*this << (uint8_t)data.length();

	if (data.length() > leftSize())
	{
		throw overflow_error("OOM to put string data");
	}
	memcpy(m_pos, data.data(), data.length());
	m_pos += data.length();
	return *this;
}

IcsProtocol& IcsProtocol::operator << (const LongString& data) throw(std::overflow_error)
{
	uint16_t len = data.length();
	*this << len;

	if (len > leftSize())
	{
		throw underflow_error("OOM to set long string data");
	}
	memcpy(m_pos, data.data(), data.length());
	m_pos += data.length();
}

IcsProtocol& IcsProtocol::operator << (const IcsProtocol& data) throw(std::overflow_error)
{
	if (data.leftSize() > leftSize())
	{
		throw overflow_error("OOM to put IcsProtocol data");
	}
	memcpy(m_pos, data.m_pos, data.leftSize());
	m_pos += data.leftSize();
	return *this;
}


void IcsProtocol::verify() throw(std::logic_error)
{
	// verify head
	m_head->verify();

	if (m_head->getLength() != bufferSize())
	{
		throw std::logic_error("message length error");
	}

	// verify crc code
	if (m_head->getCrcCode() != crc32_code(m_head, m_head->getLength() - sizeof(ics_crccode_t)))
	{
		throw std::logic_error("crc code verify error");
	}
}

IcsProtocol& IcsProtocol::operator >> (uint8_t& data) throw(std::underflow_error)
{
	if (sizeof(data) > leftSize())
	{
		throw underflow_error("OOM to get uint8_t data");
	}
	data = *m_pos++;
	return *this;
}

IcsProtocol& IcsProtocol::operator >> (uint16_t& data) throw(std::underflow_error)
{
	if (sizeof(data) > leftSize())
	{
		throw underflow_error("OOM to get uint16_t data");
	}
	data = *(uint16_t*)m_pos;
	m_pos += sizeof(data);
	return *this;
}

IcsProtocol& IcsProtocol::operator >> (uint32_t& data) throw(std::underflow_error)
{
	if (sizeof(data) > leftSize())
	{
		throw underflow_error("OOM to get uint32_t data");
	}
	data = *(uint16_t*)m_pos;
	m_pos += sizeof(data);
	return *this;
}

IcsProtocol& IcsProtocol::operator >> (IcsDataTime& data) throw(std::underflow_error)
{
	if (sizeof(data) > leftSize())
	{
		throw underflow_error("OOM to get IcsDataTime data");
	}
	IcsDataTime* dt = (IcsDataTime*)m_pos;
	data = *dt;
	data.year = ics_ntoh(dt->year);

	m_pos += sizeof(IcsDataTime);
	return *this;
}

IcsProtocol& IcsProtocol::operator >> (string& data) throw(std::underflow_error)
{
	uint8_t len = 0;
	*this >> len;

	if (len > leftSize())
	{
		throw underflow_error("OOM to get string data");
	}
	data.assign((char*)m_pos, len);
	m_pos += len;
	return *this;
}

IcsProtocol& IcsProtocol::operator >> (LongString& data) throw(std::underflow_error)
{
	uint16_t len;
	*this >> len;
	if (len > leftSize())
	{
		throw underflow_error("OOM to get long string data");
	}

	data.assign((char*)m_pos, len);
	m_pos += len;
	return *this;
}

void IcsProtocol::assertEmpty() throw(std::logic_error)
{
	if (leftSize() != 0)
	{
		char buff[64];
		std::sprintf(buff, "superfluous data:%u bytes", leftSize());
		throw std::logic_error(buff);
	}
}

}	// end protocol
}	// end namespace ics
