
#include "tcpserver.hpp"
#include "clientmanager.hpp"
#include "database.hpp"
#include "signalhandler.hpp"
#include "mempool.hpp"
#include "icsclient.hpp"
#include <iostream>
#include <string>
#include <tuple>


using namespace std;

#if 1
ics::DataBase db("commuser", "datang", "mysql");
#else
ics::DataBase db("sa", "123456", "sqlserver");
#endif

ics::MemoryPool mp(512 * 10, 10);

void test_server()
{
	
	try {
		asio::io_service io_service;

		ics::DataBase::initialize();

		db.open();

		ics::ClientManager cm(10);

		ics::TcpServer s(io_service, 9999, [&cm](asio::ip::tcp::socket s){
					cm.addClient(std::move(s));
				});
		
		s.run();

		ics::SignalHandler sh(io_service);

		sh.sync_wait();

		/*
		ics::SubCommServerClient subclient(io_service);

		subclient.connectTo("192.168.50.133", 99);
		//*/

		io_service.run();
	}
	catch (std::exception& ex)
	{
		cerr << ex.what() << endl;
	}
}

bool is_prime(int x)
{
	for (int i = 2; i < x; i++)
	{
		if (x%i == 0)
		{
			return false;
		}
	}
	return true;
}

#include <future>
#include <chrono>
void test_std()
{
	std::future<bool> fut = std::async(is_prime, 444444443);

	std::chrono::milliseconds span(100);

	while (fut.wait_for(span) == std::future_status::timeout)
	{
		std::cout << '.';
	}

	bool x = fut.get();

	std::cout << "44444444443 " << (x ? "is" : "not") << " prime." << std::endl;

	std::promise<int> prom;
	std::future<int> futu = prom.get_future();

	std::thread th1([](std::promise<int>& prom){
		int x;
		std::cout << "Please enter an integer value:";
		std::cin.exceptions(std::ios::failbit);
		try {
			std::cin >> x;
			prom.set_value(x);
		}
		catch (std::exception&){
			prom.set_exception(std::current_exception());
		}
		}, std::ref(prom));

	std::thread th2([](std::future<int>& fut){
		try {
			int x = fut.get();
			std::cout << "values: " << x << std::endl;
		}
		catch (std::exception& ex)
		{
			std::cout << "exception caught:" << ex.what() << std::endl;
		}
		}, std::ref(futu));

	th1.join();
	th2.join();
}



std::string to_json(int v)
{
	char buff[64];
	std::sprintf(buff, "%d", v);
	return buff;
}

std::string to_json(double v)
{
	char buff[64];
	std::sprintf(buff, "%f", v);
	return buff;
}

std::string to_json(const std::string& v)
{
	return v;
}

template<class T>
std::string to_json(const std::vector<T>& v)
{
	std::string s("[");
	bool first = true;
	for (auto&& e : v)
	{
		if (!first)
		{
			s += ", ";
		}
		else
		{
			first = false;
		}
		s += "]";
	}
	return s;
}

template<class K,class V>
std::string to_json(const std::map<K, V>& v)
{
	std::string s("{");
	bool first = false;
	for (auto&& e:v)
	{
		if (!first)
		{
			s += ", ";
		}
		else
		{
			first = false;
		}
		s += to_json(e.fisrt) + ":" + to_json(e.second);
	}
	return s;
}

template<std::size_t Index, std::size_t N>
struct tuple_to_json_impl 
{
	template<class T>
	std::string operator()(const T& t) const
	{
		std::string s(to_json(std::get<Index>(t)));
		if (Index < N -1)	// not the last element,then add a separator
		{
			s += ", ";
		}
		s += tuple_to_json_impl<Index + 1, N>()(t);
		return s;
	}
};

template<std::size_t N>
struct tuple_to_json_impl<N, N>
{
	template<class T>
	std::string operator()(const T& t) const
	{
		return "";
	}
};

template<class ...T>
std::string to_json(const std::tuple<T...>& t)
{
	std::string s("[");
//	s += tuple_to_json_impl<0, std::tuple_size<decltype(t)>()>(t);
	s += tuple_to_json_impl<0, sizeof...(T)>()(t);
	s += "]";
	return s;
}

int main()
{
	cout << "start..." << endl;

//	test_std();

	test_server();

	
	cout << "stop..." << endl;
	return 0;
}
