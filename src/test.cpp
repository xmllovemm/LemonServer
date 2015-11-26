
#include "log.hpp"
#include "tcpserver.hpp"
#include "clientmanager.hpp"
#include "database.hpp"
#include "mempool.hpp"
#include "icsconfig.hpp"
#include "icspushsystem.hpp"
#include "util.hpp"
#include "connection.hpp"
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

ics::MemoryPool g_memoryPool;

ics::IcsConfig config;

ics::ClientManager<ics::IcsConnection<asio::ip::tcp>> tcpClientManager(10);

ics::ClientManager<ics::IcsConnection<asio::ip::udp>> udpClientManager(10);

ics::PushSystem pushSystem;

void test_server(const char* configfile)
{
	asio::io_service io_service;

	ics::TcpServer clientServer(io_service);

	asio::signal_set signals(io_service);

	signals.add(SIGINT);
	signals.add(SIGTERM);

	signals.async_wait([&io_service](asio::error_code ec, int signo)
		{
			cout << "catch a signal " << signo << ",exit..." << endl;
			io_service.stop();
		});

	try {
		// load config file
		config.load(configfile);

		// init log file
		init_log(config.getAttributeString("log", "configfile").c_str());

		if (config.getAttributeInt("program", "daemon"))
		{
			ics::be_daemon(config.getAttributeString("program", "workdir").c_str());
		}

		ics::DataBase::initialize();

		pushSystem.init(config.getAttributeString("pushmsg", "serverip"), config.getAttributeInt("pushmsg", "serverport"));

		g_memoryPool.init(config.getAttributeInt("program", "chunksize"), config.getAttributeInt("program", "chunkcount"));

		db.init(config.getAttributeString("database", "username"), config.getAttributeString("database", "password"), config.getAttributeString("database", "dsn"));

		db.open();

		clientServer.init(config.getAttributeString("listen", "addr")
			,[](asio::ip::tcp::socket&& s){
				tcpClientManager.createConnection(std::move(s));
			});

		io_service.run();

	}
	catch (std::exception& ex)
	{
		cerr << "init failed std exception: " << ex.what() << endl;
	}
	catch (ics::IcsException& ex)
	{
		cerr << "init failed ics exception: " << ex.message() << endl;
	}
	catch (otl_exception& ex)
	{
		cerr << "init failed database exception: " << ex.msg << endl;
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

#include <regex>

class Base {
public:
	void start()
	{
		do_read();
	}

private:
	void do_read()
	{
		cout << "post do read" << endl;
		// handleData()
	}

	void handleData()
	{
		// is ics protocol
		// generate 
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
	
	test_server(argv[1]);

	cout << "stop..." << endl;
	return 0;
}
