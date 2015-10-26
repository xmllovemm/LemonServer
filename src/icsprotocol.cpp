

#include "icsprotocol.hpp"
#include <string.h>


namespace ics{


static const uint32_t Crc32Table[] =
{
	0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC,
	0x17C56B6B, 0x1A864DB2, 0x1E475005, 0x2608EDB8, 0x22C9F00F,
	0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A,
	0x384FBDBD, 0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9,
	0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75, 0x6A1936C8,
	0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3,
	0x709F7B7A, 0x745E66CD, 0x9823B6E0, 0x9CE2AB57, 0x91A18D8E,
	0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
	0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84,
	0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D, 0xD4326D90, 0xD0F37027,
	0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022,
	0xCA753D95, 0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1,
	0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D, 0x34867077,
	0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C,
	0x2E003DC5, 0x2AC12072, 0x128E9DCF, 0x164F8078, 0x1B0CA6A1,
	0x1FCDBB16, 0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
	0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB,
	0x6F52C06C, 0x6211E6B5, 0x66D0FB02, 0x5E9F46BF, 0x5A5E5B08,
	0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D,
	0x40D816BA, 0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E,
	0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692, 0x8AAD2B2F,
	0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044,
	0x902B669D, 0x94EA7B2A, 0xE0B41DE7, 0xE4750050, 0xE9362689,
	0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2,
	0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683,
	0xD1799B34, 0xDC3ABDED, 0xD8FBA05A, 0x690CE0EE, 0x6DCDFD59,
	0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C,
	0x774BB0EB, 0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F,
	0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53, 0x251D3B9E,
	0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5,
	0x3F9B762C, 0x3B5A6B9B, 0x0315D626, 0x07D4CB91, 0x0A97ED48,
	0x0E56F0FF, 0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
	0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2,
	0xE6EA3D65, 0xEBA91BBC, 0xEF68060B, 0xD727BBB6, 0xD3E6A601,
	0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604,
	0xC960EBB3, 0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7,
	0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B, 0x9B3660C6,
	0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD,
	0x81B02D74, 0x857130C3, 0x5D8A9099, 0x594B8D2E, 0x5408ABF7,
	0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
	0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD,
	0x6C47164A, 0x61043093, 0x65C52D24, 0x119B4BE9, 0x155A565E,
	0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B,
	0x0FDC1BEC, 0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088,
	0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654, 0xC5A92679,
	0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12,
	0xDF2F6BCB, 0xDBEE767C, 0xE3A1CBC1, 0xE760D676, 0xEA23F0AF,
	0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
	0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5,
	0x9E7D9662, 0x933EB0BB, 0x97FFAD0C, 0xAFB010B1, 0xAB710D06,
	0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03,
	0xB1F740B4
};

uint32_t CRC32S_SChack8(const uint8_t* pBuf, const uint32_t nSize)
{
	uint32_t nTemp = 0;
	uint32_t i = 0;
	uint32_t n = 0;

	uint32_t nReg = 0xFFFFFFFF;

	for (n = 0; n < nSize; n++)
	{
		nReg ^= (uint32_t)pBuf[n];

		for (i = 0; i < 4; i++)
		{
			nTemp = Crc32Table[(uint8_t)((nReg >> 24) & 0xff)];
			nReg <<= 8;
			nReg ^= nTemp;
		}
	}

	return nReg;
}



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
/*-----------------------------IcsMsgHead-------------------------------------*/

void IcsProtocol::IcsMsgHead::verify() throw(std::logic_error)
{
	if (std::memcmp(name, ICS_HEAD_PROTOCOL_NAME, ICS_HEAD_PROTOCOL_NAME_LEN) != 0)
	{
		throw std::logic_error("error protocol name");
	}
	if (version != ICS_HEAD_PROTOCOL_VERSION)
	{
		throw std::logic_error("error protocol version");
	}
}

// set 0
void IcsProtocol::IcsMsgHead::clean()
{
	memset(this, 0, sizeof(IcsMsgHead));
}

void IcsProtocol::IcsMsgHead::setMsgID(uint16_t id)
{
	this->id = ics_hton(id);
}

void IcsProtocol::IcsMsgHead::setLength(uint16_t len)
{
	this->length = ics_hton(len);
}

void IcsProtocol::IcsMsgHead::setSendNum(uint16_t num)
{
	this->send_num = ics_hton(num);
}

void IcsProtocol::IcsMsgHead::setAckNum(uint16_t num)
{
	this->ack_num = ics_hton(num);
}

void IcsProtocol::IcsMsgHead::setFlag(uint8_t encrypt, bool ack, bool response)
{
	this->id = ics_hton(encrypt);
}

void IcsProtocol::IcsMsgHead::setCrcCode(IcsProtocol::ics_crccode_t code)
{
	*(ics_crccode_t*)((char*)this + getLength() - sizeof(ics_crccode_t)) = ics_hton(code);
}

uint16_t IcsProtocol::IcsMsgHead::getMsgID()
{
	return ics_ntoh(this->id);
}

uint16_t IcsProtocol::IcsMsgHead::getLength()
{
	return ics_ntoh(this->length);
}

uint16_t IcsProtocol::IcsMsgHead::getSendNum()
{
	return ics_ntoh(this->send_num);
}

uint16_t IcsProtocol::IcsMsgHead::getAckNum()
{
	return ics_ntoh(this->ack_num);
}

uint16_t IcsProtocol::IcsMsgHead::getFlag()
{
	return ics_ntoh(this->id);
}

IcsProtocol::ics_crccode_t IcsProtocol::IcsMsgHead::getCrcCode()
{
	return ics_ntoh(*(ics_crccode_t*)((char*)this + getLength() - sizeof(ics_crccode_t)));
}
/*-----------------------------IcsProtocol-------------------------------------*/


IcsProtocol::IcsProtocol(void* buf, size_t length)
{
	m_head = (IcsMsgHead*)buf;
	m_start = m_pos = (uint8_t*)buf + sizeof(IcsMsgHead);
	m_end = (uint8_t*)buf + length;
}

		
// set data
void IcsProtocol::serailzeToData()
{
	/*
	// set length
	((IcsMsgHead*)m_start)->length = ics_hton((uint16_t)(m_pos - m_start + sizeof(uint16_t)));

	// set crc32 code
	*this << getCrc32Code(m_start, m_pos - m_start);
	m_pos += sizeof(uint16_t);
	*/
}

void IcsProtocol::initHead(uint16_t id, uint16_t send_num, uint16_t ack_num, bool request, bool response, EncryptionType et)
{
	IcsMsgHead* mh = (IcsMsgHead*)m_start;
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

template<typename T>
void IcsProtocol::setString(const string& data) throw(std::overflow_error)
{
	T len = (T)data.length();
	*this << len;

	if (len > (T)leftSize())
	{
		throw underflow_error("OOM to set string data");
	}
	memcpy(m_pos, data.data(), data.length());
	m_pos += data.length();
}


// get data
void IcsProtocol::parseFormData(void* buf, int length)  throw(std::underflow_error)
{
	m_start = m_pos = (uint8_t*)buf;
	m_end = m_start + length;
}

void IcsProtocol::verify() throw(std::logic_error)
{
	// verify head
	m_head->verify();

	// verify crc code
	if (m_head->getCrcCode() != getCrc32Code((uint8_t*)m_head, m_head->getLength() - sizeof(ics_crccode_t)))
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

template<typename T>
string&& IcsProtocol::getString() throw(std::underflow_error)
{
	string str;
	T len;
	*this >> len;
	if (len > leftSize())
	{
		throw underflow_error("OOM to get string data");
	}

	str.assign((char*)m_pos, len);
	m_pos += len;
	return std::move(str);
}

void IcsProtocol::assertEmpty() throw(std::logic_error)
{
	if (leftSize() != 0)
	{
		throw std::logic_error("superfluous data");
	}
}

// calcutlate the crc code
IcsProtocol::ics_crccode_t IcsProtocol::getCrc32Code(uint8_t* buf, int length)
{
	uint32_t nReg = 0xFFFFFFFF;

	for (int i = 0; i < length; i++)
	{
		nReg ^= (uint32_t)buf[i];

		for (int j = 0; j < 4; j++)
		{
			uint32_t tmp = Crc32Table[(nReg >> 24) & 0xff];
			nReg <<= 8;
			nReg ^= tmp;
		}
	}

	return nReg;
}

}	// end namespace ics
