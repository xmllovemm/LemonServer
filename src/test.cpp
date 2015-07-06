
#include "tcpserver.h"
#include "icsclient.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <thread>

using namespace std;
using asio::ip::tcp;


class chat_message
{
public:
	enum { header_length = 4 };
	enum { max_body_length = 512 };

	chat_message()
		: body_length_(0)
	{
	}

	const char* data() const
	{
		return data_;
	}

	char* data()
	{
		return data_;
	}

	std::size_t length() const
	{
		return header_length + body_length_;
	}

	const char* body() const
	{
		return data_ + header_length;
	}

	char* body()
	{
		return data_ + header_length;
	}

	std::size_t body_length() const
	{
		return body_length_;
	}

	void body_length(std::size_t new_length)
	{
		body_length_ = new_length;
		if (body_length_ > max_body_length)
			body_length_ = max_body_length;
	}

	bool decode_header()
	{
		char header[header_length + 1] = "";
		std::memcpy(header, data_, header_length);
		body_length_ = std::atoi(header);
		if (body_length_ > max_body_length)
		{
			body_length_ = 0;
			return false;
		}
		return true;
	}

	void encode_header()
	{
		char header[header_length + 1] = "";
		std::sprintf(header, "%4d", static_cast<int>(body_length_));
		std::memcpy(data_, header, header_length);
	}

private:
	char data_[header_length + max_body_length];
	std::size_t body_length_;
};

typedef std::deque<chat_message> chat_message_queue;

class chat_client
{
public:
	chat_client(asio::io_service& io_service,
		tcp::resolver::iterator endpoint_iterator)
		: io_service_(io_service),
		socket_(io_service)
	{
		do_connect(endpoint_iterator);
	}

	void write(const chat_message& msg)
	{
		io_service_.post(
			[this, msg]()
		{
			bool write_in_progress = !write_msgs_.empty();
			write_msgs_.push_back(msg);
			if (!write_in_progress)
			{
				do_write();
			}
		});
	}

	void close()
	{
		io_service_.post([this]() { socket_.close(); });
	}

private:
	void do_connect(tcp::resolver::iterator endpoint_iterator)
	{
		asio::async_connect(socket_, endpoint_iterator,
			[this](std::error_code ec, tcp::resolver::iterator)
		{
			if (!ec)
			{
				do_read_header();
			}
		});
	}

	void do_read_header()
	{
		asio::async_read(socket_,
			asio::buffer(read_msg_.data(), chat_message::header_length),
			[this](std::error_code ec, std::size_t /*length*/)
		{
			if (!ec && read_msg_.decode_header())
			{
				do_read_body();
			}
			else
			{
				socket_.close();
			}
		});
	}

	void do_read_body()
	{
		asio::async_read(socket_,
			asio::buffer(read_msg_.body(), read_msg_.body_length()),
			[this](std::error_code ec, std::size_t /*length*/)
		{
			if (!ec)
			{
				std::cout.write(read_msg_.body(), read_msg_.body_length());
				std::cout << "\n";
				do_read_header();
			}
			else
			{
				socket_.close();
			}
		});
	}

	void do_write()
	{
		asio::async_write(socket_,
			asio::buffer(write_msgs_.front().data(),
			write_msgs_.front().length()),
			[this](std::error_code ec, std::size_t /*length*/)
		{
			if (!ec)
			{
				write_msgs_.pop_front();
				if (!write_msgs_.empty())
				{
					do_write();
				}
			}
			else
			{
				socket_.close();
			}
		});
	}

private:
	asio::io_service& io_service_;
	tcp::socket socket_;
	chat_message read_msg_;
	chat_message_queue write_msgs_;
};

int test_asio(char* ip, char* port)
{
	try
	{
		asio::io_service io_service;

		tcp::resolver resolver(io_service);
		auto endpoint_iterator = resolver.resolve({ ip, port });
		chat_client c(io_service, endpoint_iterator);

		std::thread t([&io_service](){ io_service.run(); });

		char line[chat_message::max_body_length + 1];
		while (std::cin.getline(line, chat_message::max_body_length + 1))
		{
			if (std::strcmp(line,"quit") == 0)
			{
				break;
			}
			
			chat_message msg;
			msg.body_length(std::strlen(line));
			std::memcpy(msg.body(), line, msg.body_length());
			msg.encode_header();
			c.write(msg);
		}

		c.close();
		t.join();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}


void test_client()
{
	
	ics::IcsClientManager m("192.168.50.133", "2100");

	ics::AuthrizeInfo ai("test1", "test1", 2, "ext_info");

	m.createIcsClient({"test1", "test1", 2, "ext_info"});
//	m.createIcsClient(ai);

//	m.run();

//	ics::AuthrizeInfo&& rl = static_cast<ics::AuthrizeInfo&&>(ai);


	int n;
	cin >> n;
}


/*
namespace ics{


template<typename ...args>	class my_tuple;


template<typename head, typename... tail>
class my_tuple<head, tail...> : private my_tuple<tail...>
{
public:
	my_tuple(head h, tail... t) : m_head(h)
	{
		cout << "head:" << h << endl;
	}
private:
	head	m_head;
};



template<>
class my_tuple<>
{

};

}
//*/


void test_c11()
{
	vector<int> v(10);
	int n = 1;
	for_each(v.begin(), v.end(), [&](int& v){ v = n++; });

	for (auto it=v.begin(); it!=v.end(); it++)
	{
		cout << *it << " ";
	}
	cout << endl;

	for_each(v.begin(), v.end(), [=](int v){ cout << v << " "; });

	cout << endl;
}


void my_printf(const char* s)
{
	cout << s;
}


template<typename T, typename... Args>
void my_printf(const char* s, T value, Args... args)
{
	cout << "sizeof args: " << sizeof...(args) << endl;
	while (*s != 0)
	{
		if (*s == '%')
		{
			cout << value;

			// call even when *s == 0 to detect extra arguments 
			my_printf(s + 1, args...);
			return;			
		}
		
		cout << *s++;
	}
}

class X
{

};

void inner(const X& x){ cout << "const x&" << endl; }

void inner(X&& x){ cout << "x&&" << endl; }

template<typename T> 
void outer(T&& t)
{ 
	inner(std::forward<X>(t)); 
}

void test_forward()
{
	X a;
	outer(a);	// there is wrong with T&& under forward
	outer(X());
	inner(std::forward<X>(X()));
}

int main()
{
	cout << "start..." << endl;

	

//	test_client();

//	test_asio("192.168.50.133", "2100");

//	ics::my_tuple<int,char,float,string> m(1,'2',3.0,"fuck");
	
	my_printf("Today is %,and % I'm very happy, so say: %\n", "Monday", "your leaving", "bye bye");

//	test_forward();

	cout << "stop..." << endl;

	return 0;
}