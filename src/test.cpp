
#include "tcpserver.hpp"
#include "clientmanager.hpp"
#include "database.hpp"
#include "signalhandler.hpp"
#include "mempool.hpp"
#include "icsclient.hpp"
#include "icsconfig.hpp"
#include <iostream>
#include <string>
#include <tuple>


using namespace std;

#if defined(WIN32)
#define CONFIG_FILE "E:\\workspace\\project_on_github\\LemonServer\\bin\\config.xml"
#else
#define CONFIG_FILE "../src/config.xml"
#endif

#if 1
ics::DataBase db;
#else
ics::DataBase db("sa", "123456", "sqlserver");
#endif

ics::MemoryPool mp;

ics::IcsConfig config;

ics::ClientManager<ics::IcsConnection<ics::icstcp>> tcpClientManager(10);

ics::ClientManager<ics::IcsConnection<ics::icsudp>> udpClientManager(10);


void test_server(const char* configfile)
{
	asio::io_service io_service;

	ics::SignalHandler sh(io_service);

	// load config file
	config.load(configfile);

	// get listen address
	const std::string& ipaddr = config.getAttributeString("listen", "addr");

	int separator = ipaddr.find(':');
	if (separator == std::string::npos)
	{
		cerr << "ip addr is not correct:" << ipaddr << endl;
		return;
	}

	std::string port = ipaddr.substr(separator + 1);
	std::string ip = ipaddr.substr(0, separator);

	ics::TcpServer clientServer(io_service
		, ip
		, std::strtol(port.c_str(), NULL, 10)
		, [](asio::ip::tcp::socket&& s){
			tcpClientManager.createConnection(std::move(s));
		});

	try {
		sh.sync_wait();

		ics::DataBase::initialize();

		mp.init(config.getAttributeInt("program", "chunksize"), config.getAttributeInt("program", "chunkcount"));

		db.init(config.getAttributeString("database", "username"), config.getAttributeString("database", "password"), config.getAttributeString("database", "dsn"));

		db.open();

		clientServer.run();

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
	catch (ics::IcsException& ex)
	{
		cerr << ex.message() << endl;
	}
	catch (...)
	{
		cerr << "unknown error" << endl;
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


class Test : public std::enable_shared_from_this<Test> {
public:
	Test()
	{
		cout << "construct..." << endl;
	}

	~Test()
	{
		cout << "destruct..." << endl;
	}
};

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		cerr << "useage:" << argv[0] << " configfile" << endl;
		return 0;
	}

	cout << "start..." << endl;

	std::unique_ptr<Test> it= std::make_unique<Test>();

	std::shared_ptr<Test> ano = it->shared_from_this();

//	test_server(argv[1]);

	cout << "stop..." << endl;
	return 0;
}
