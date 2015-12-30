

#include "icsprotocol.hpp"
#include "util.hpp"

extern ics::MemoryPool g_memoryPool;

namespace ics{

//------------------------------ICS byteorder function------------------------------//
uint8_t ics_byteorder(uint8_t n)
{
	return n;
}

uint16_t ics_byteorder(uint16_t n)
{
#if PROTOCOL_BYTE_ORDER == SYSTEM_BYTE_ORDER
	return n;
#else
	return ((n & 0xff) << 8) | ((n >> 8) & 0xff);
#endif
}

uint32_t ics_byteorder(uint32_t n)
{
#if PROTOCOL_BYTE_ORDER == SYSTEM_BYTE_ORDER
	return n;
#else
	return ((n & 0x000000ff) << 24) | ((n & 0x0000ff00) << 8) | ((n & 0x00ff0000) >> 8) | ((n & 0xff000000) >> 24);
#endif
}

uint64_t ics_byteorder(uint64_t n)
{
#if PROTOCOL_BYTE_ORDER == SYSTEM_BYTE_ORDER
	return n;
#else
	return ((n & 0x000000ff) << 24) | ((n & 0x0000ff00) << 8) | ((n & 0x00ff0000) >> 8) | ((n & 0xff000000) >> 24);
#endif
}

float ics_byteorder(float n)
{
#if PROTOCOL_BYTE_ORDER == SYSTEM_BYTE_ORDER
	return n;
#else
	auto tmp = ics_byteorder(*(uint32_t)&n);
	return *(float)&tmp;
#endif
}

double ics_byteorder(double n)
{
#if PROTOCOL_BYTE_ORDER == SYSTEM_BYTE_ORDER
	return n;
#else
	double ret;
	if (sizeof(double) == sizeof(uint32_t))
	{
		auto tmp = ics_byteorder(*(uint32_t)&n);
		ret = *(double)&tmp;
	}
	else
	{
		auto tmp = ics_byteorder(*(uint64_t)&n);
		ret = *(double)&tmp;
	}
	return ret;
#endif
}

std::time_t ics_byteorder(std::time_t n)
{
#if PROTOCOL_BYTE_ORDER == SYSTEM_BYTE_ORDER
	return n;
#else
	return ((n & 0x000000ff) << 24) | ((n & 0x0000ff00) << 8) | ((n & 0x00ff0000) >> 8) | ((n & 0xff000000) >> 24);
#endif
}




//------------------------------ICS useful function------------------------------//
otl_stream& operator<<(otl_stream& s, const IcsDataTime& dt)
{
	otl_datetime od;
	od.year = dt.year;
	od.month = dt.month;
	od.day = dt.day;
	od.hour = dt.hour;
	od.minute = dt.miniute;
	od.second = dt.second;

	return s << od;
}

otl_stream& operator<<(otl_stream& s, const LongString& str)
{
	return s << str.c_str();
}

// get current time
void getIcsNowTime(IcsDataTime& dt)
{
	std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm* tm = std::localtime(&t);
	dt.year = tm->tm_year + 1900;
	dt.month = tm->tm_mon + 1;
	dt.day = tm->tm_mday;
	dt.hour = tm->tm_hour;
	dt.miniute = tm->tm_min;
	dt.second = tm->tm_sec;
	dt.milesecond = 0;
}


//------------------------------ICS message head------------------------------//
void IcsMsgHead::verify(const void* buf, std::size_t len) const throw(IcsException)
{
	if (std::memcmp(name, ICS_HEAD_PROTOCOL_NAME, ICS_HEAD_PROTOCOL_NAME_LEN) != 0)
	{
		throw IcsException("protocol name = %c%c%c%c", name[0], name[1], name[2], name[3]);
	}
	if (getVersion() != ICS_HEAD_PROTOCOL_VERSION)
	{
		throw IcsException("protocol version = %x", getVersion());
	}
	if (getLength() != len)
	{
		throw IcsException("protocol length = %d,but recv length = %d", getLength(), len);
	}
	if (getCrcCode() != crc32_code(buf, len - CrcCodeSize))
	{
		throw IcsException("protocol crc code = %x", getCrcCode());
	}
}


// set 0
void IcsMsgHead::clean()
{
	memset(this, 0, sizeof(IcsMsgHead));
}

void IcsMsgHead::setMsgID(uint16_t id)
{
	this->id = ics_byteorder(id);
}

void IcsMsgHead::setLength(uint16_t len)
{
	this->length = ics_byteorder(len);
}

void IcsMsgHead::setSendNum(uint16_t num)
{
	this->send_num = ics_byteorder(num);
}

void IcsMsgHead::setAckNum(uint16_t num)
{
	this->ack_num = ics_byteorder(num);
}

void IcsMsgHead::setFlag(int encrypt, int ack, int needResponse)
{
	this->encrypt = encrypt;
	this->ack = ack;
	this->response = needResponse;
	this->flag_data = ics_byteorder(this->flag_data);
}

void IcsMsgHead::setCrcCode()
{
	ics_crccode_t code = 0;
	*(ics_crccode_t*)((char*)this + getLength() - sizeof(ics_crccode_t)) = ics_byteorder(code);
}

MessageId IcsMsgHead::getMsgID() const
{
	return (MessageId)ics_byteorder(this->id);
}

uint16_t IcsMsgHead::getLength() const
{
	return ics_byteorder(this->length);
}

bool IcsMsgHead::isResponse() const
{
	uint16_t flag = ics_byteorder(this->flag_data);
	return (flag >> 8 & 0x1) == 1;
}

bool IcsMsgHead::needResposne() const
{
	uint16_t flag = ics_byteorder(this->flag_data);
	return (flag >> 8 & 0x03) == 0x02;
}

uint16_t IcsMsgHead::getSendNum() const
{
	return ics_byteorder(this->send_num);
}

uint16_t IcsMsgHead::getAckNum() const
{
	return ics_byteorder(this->ack_num);
}

uint16_t IcsMsgHead::getFlag() const
{
	return ics_byteorder(this->id);
}

uint16_t IcsMsgHead::getVersion() const
{
	return ics_byteorder(this->version);
}

uint32_t IcsMsgHead::getCrcCode() const
{
	return ics_byteorder(*(uint32_t*)(this->name + getLength() - CrcCodeSize));
}


//------------------------------ICS message stream------------------------------//

ProtocolStream::ProtocolStream(OptType type, void* buf, std::size_t length)
	: m_optType(type)
	, m_start((uint8_t*)buf)
	, m_pos(m_start + sizeof(IcsMsgHead))
	, m_end(m_start + length)
{
	if (!m_start || length < sizeof(IcsMsgHead)+IcsMsgHead::CrcCodeSize)
	{
		throw IcsException("init ProtocolStream data error,buf=%x,length=%d", buf, length);
	}
	
	if (type == OptType::writeType)/// 若作为写操作，初始化消息头
	{
		new (m_start)IcsMsgHead();
	}
	else if (type == OptType::readType)/// 若作为读操作，校验消息头，长度减去校验码长度
	{
		((IcsMsgHead*)m_start)->verify(m_start, length);
		m_end -= IcsMsgHead::CrcCodeSize;
	}
}

ProtocolStream::ProtocolStream(OptType type, const MemoryChunk& chunk)
{
	::new (this) ProtocolStream(type, chunk.data, chunk.length);
}

ProtocolStream::ProtocolStream(const ProtocolStream& rhs, const MemoryChunk& chunk)
{
	::new (this) ProtocolStream(rhs.m_optType, chunk);
	if (size() < rhs.length())
	{
		throw IcsException("not enough memory [%d] to store [%d] bytes data", size(), rhs.length());
	}
	std::memcpy(m_start, rhs.m_start, rhs.length());
	m_pos = m_start + rhs.length();
}

ProtocolStream::~ProtocolStream()
{
	if (m_start && m_optType == OptType::writeType)
	{
		g_memoryPool.put(toMemoryChunk());
	}
}


/// 调用该接口以后不可读写操作
MemoryChunk ProtocolStream::toMemoryChunk()
{
	if (!m_start)
	{
		throw IcsException("MemoryChunk has been moved");
	}

	MemoryChunk mc(m_start, length());
	m_start = nullptr;
	return mc;
}





// -----------------------set data----------------------- 
// 设置消息体长度和校验码
void ProtocolStream::serialize(uint16_t sendNum)
{
	IcsMsgHead* head = (IcsMsgHead*)m_start;
	head->setSendNum(sendNum);
	head->setLength(length() + IcsMsgHead::CrcCodeSize);
	*this << crc32_code(m_start, length());
}


void ProtocolStream::initHead(MessageId id, bool needResponse)
{
	((IcsMsgHead*)m_start)->setMsgID(id);
	((IcsMsgHead*)m_start)->setFlag(0, 0, needResponse?1:0);
}

void ProtocolStream::initHead(MessageId id, uint16_t ackNum)
{
	((IcsMsgHead*)m_start)->setMsgID(id);
	((IcsMsgHead*)m_start)->setFlag(0, 0, 0);
	((IcsMsgHead*)m_start)->setAckNum(ackNum);
}


ProtocolStream& ProtocolStream::operator << (const IcsDataTime& data) throw(IcsException)
{
	if (sizeof(data) > leftLength())
	{
		throw IcsException("OOM to set IcsDataTime data");
	}

	*this << data.year << data.month << data.day << data.hour << data.miniute << data.sec_data;
	return *this;
}


ProtocolStream& ProtocolStream::operator << (const ShortString& data) throw(IcsException)
{
	return this->operator<<<uint8_t>(data.c_str());
}

ProtocolStream& ProtocolStream::operator << (const LongString& data) throw(IcsException)
{
	return this->operator<<<uint16_t>(data.c_str());
}

ProtocolStream& ProtocolStream::operator << (const ProtocolStream& data) throw(IcsException)
{
	if (data.leftLength() > leftLength())
	{
		throw IcsException("OOM to set %d bytes ProtocolStream data", data.leftLength());
	}
	std::memcpy(m_pos, data.m_pos, data.leftLength());
	m_pos += data.leftLength();
	return *this;
}

void ProtocolStream::moveBack(std::size_t offset) throw(IcsException)
{
	if (m_pos - offset < m_start)
	{
		throw IcsException("can't move back %s bytes", offset);
	}
	m_pos -= offset;
}

void ProtocolStream::append(const void* data, std::size_t len)
{
	if (len > leftLength())
	{
		throw IcsException("OOM to set %d bytes ProtocolStream data", len);
	}
	std::memcpy(m_pos, data, len);
	m_pos += len;
}

ProtocolStream& ProtocolStream::operator >> (IcsDataTime& data) throw(IcsException)
{
	if (sizeof(data) > leftLength())
	{
		throw IcsException("OOM to get IcsDataTime data");
	}
	*this >> data.year >> data.month >> data.day >> data.hour >> data.miniute >> data.sec_data;
	return *this;
}

ProtocolStream& ProtocolStream::operator >> (ShortString& data) throw(IcsException)
{
	uint8_t len = 0;
	*this >> len;

	if (len > leftLength())
	{
		throw IcsException("OOM to get %d bytes short string data", len);
	}
	data.assign((char*)m_pos, len);
	m_pos += len;
	return *this;
}

ProtocolStream& ProtocolStream::operator >> (LongString& data) throw(IcsException)
{
	uint16_t len;
	*this >> len;
	if (len > leftLength())
	{
		throw IcsException("OOM to get %d bytes long string data", len);
	}

	data.assign((char*)m_pos, len);
	m_pos += len;
	return *this;
}

void ProtocolStream::assertEmpty() const throw(IcsException)
{
	if (leftLength() != 0)
	{
		throw IcsException("superfluous data:%d bytes", leftLength());
	}
}

#ifndef WIN32
/// 跳过ShortString类型的数据
template<>
ProtocolStream& ProtocolStream::moveForward<ShortString>() throw(IcsException)
{
	uint8_t len;
	*this >> len;
	if (len > leftLength())
	{
		throw IcsException("can't move forward %d bytes", len);
	}
	m_pos += len;
	return *this;
}

/// 跳过LongString类型的数据
template<>
ProtocolStream& ProtocolStream::moveForward<LongString>() throw(IcsException)
{
	uint16_t len;
	*this >> len;
	if (len > leftLength())
	{
		throw IcsException("can't move forward %d bytes", len);
	}
	m_pos += len;
	return *this;
}
#endif

}
