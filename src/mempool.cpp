

#include "mempool.hpp"
#include "icsexception.hpp"
#include <cstring>

namespace ics {

MemoryChunk::MemoryChunk()
	: data(nullptr), length(0)
{

}

MemoryChunk::MemoryChunk(void* buf, std::size_t length)
: data((uint8_t*)buf), length(length)
{

}


MemoryChunk::MemoryChunk(const MemoryChunk& rhs)
	: data(rhs.data), length(rhs.length)
{
}


MemoryChunk::MemoryChunk(MemoryChunk&& rhs)
: data(rhs.data), length(rhs.length)
{
	rhs.data = nullptr;
	rhs.length = 0;
}


MemoryChunk::~MemoryChunk()
{

}

MemoryChunk MemoryChunk::clone(MemoryPool& mp)
{
	auto mc = mp.get();
	mc.length = length;
	std::memcpy(mc.data, data, mc.length);
	return mc;
}

void MemoryChunk::operator = (MemoryChunk&& rhs)
{
	data = rhs.data;
	length = rhs.length;

	rhs.data = nullptr;
	rhs.length = 0;
}

MemoryChunk& MemoryChunk::operator = (const MemoryChunk& rhs)
{
	data = rhs.data;
	length = rhs.length;
	return *this;
}


bool MemoryChunk::valid() const
{
	return data && length > 0;
}



MemoryPool::MemoryPool(std::size_t chunkSize, std::size_t countOfChunk, bool zeroData)
{
	init(chunkSize, countOfChunk, zeroData);
}

MemoryPool::MemoryPool()
{

}

MemoryPool::~MemoryPool()
{
	delete[] m_buff;
}

void MemoryPool::init(std::size_t chunkSize, std::size_t countOfChunk, bool zeroData)
{
	m_chunkSize = chunkSize;
	m_chunkCount = countOfChunk;
	m_buff = new uint8_t[m_chunkSize * m_chunkCount];

	if (!m_buff)
	{
		throw IcsException("can't allocal %d bytes buffer", m_chunkSize * m_chunkCount);
	}

	if (zeroData)
	{
		std::memset(m_buff, 0, m_chunkSize * m_chunkCount);
	}

	for (size_t i = 0; i < m_chunkCount; i++)
	{
		m_chunkList.push_back(m_buff);
	}
}

MemoryChunk MemoryPool::get()
{
	std::lock_guard<std::mutex> lock(m_chunkLock);
	MemoryChunk chunk;
	if (!m_chunkList.empty())
	{
		chunk.data = m_chunkList.front();
		chunk.length = m_chunkSize;
		m_chunkList.pop_front();
	}
	return chunk;
}

void MemoryPool::put(const MemoryChunk& chunk)
{
	if (chunk.data)
	{
		std::lock_guard<std::mutex> lock(m_chunkLock);
		m_chunkList.push_back(chunk.data);
	}
}

std::size_t MemoryPool::chunkSize() const
{
	return m_chunkSize;
}

}