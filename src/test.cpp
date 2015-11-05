
#include "tcpserver.hpp"
#include "clientmanager.hpp"
#include "database.hpp"
#include "signalhandler.hpp"
#include "mempool.hpp"
#include "icsprotocol.hpp"
#include "icsclient.hpp"
#include <iostream>


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

class MyNull {
public:
	template<class T>
	operator T*() const
	{
		return (T*)0;
	}
};


class A {
public:
	virtual void f()
	{
		cout << "call A::f()" << endl;
	}

	void g()
	{
		f();
	}
};

class B :public A
{
public:
	virtual void f()
	{
		cout << "call B::f()" << endl;
	}
};

int main()
{
	cout << "start..." << endl;

//	test_std();
	
	test_server();

	cout << "stop..." << endl;

	

	int n;
	cin >> n;

	return 0;
}
