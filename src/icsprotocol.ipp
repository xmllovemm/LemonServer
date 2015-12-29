
#include "icsprotocol.hpp"

namespace ics {


/// 跳过该类型的数据
template<class T>
ProtocolStream& ProtocolStream::moveForward() throw(IcsException)
{
	if (sizeof(T) > leftLength())
	{
		throw IcsException("can't move forward %d bytes", sizeof(T));
	}
	m_pos += sizeof(T);
	return *this;
}


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

}