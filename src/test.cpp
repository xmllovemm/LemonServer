

#include "database.hpp"
#include "mempool.hpp"
#include "icsconfig.hpp"
#include "log.hpp"
#include "util.hpp"
#include "icspushsystem.hpp"
#include "tcpserver.hpp"
#include "clientmanager.hpp"
#include "icsconnection.hpp"

#include "config.hpp"
#include <asio.hpp>

#include <iostream>
#include <string>


using namespace std;



#if defined(WIN32)
#define CONFIG_FILE "E:\\workspace\\project_on_github\\LemonServer\\bin\\config.xml"
#else
#define CONFIG_FILE "../src/config.xml"
#endif


ics::IcsConfig config;
ics::PushSystem pushSystem;
ics::MemoryPool g_memoryPool;
ics::ClientManager<ics::IcsConnectionImpl<asio::ip::tcp>> tcpClientManager(10);
ics::ClientManager<ics::IcsConnectionImpl<asio::ip::udp>> udpClientManager(10);
#if 1
ics::DataBase db;
#else
ics::DataBase db("sa", "123456", "sqlserver");
#endif



void usage(const char* prog)
{
	cerr << "useage:" << prog << " configfile" << endl;
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		usage(argv[0]);
		return 0;
	}

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
		config.load(argv[1]);

		// init log file
		ics::init_log(config.getAttributeString("log", "configfile").c_str());

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
			, [](asio::ip::tcp::socket&& s)
		{
//				tcpClientManager.createConnection(std::move(s));
				auto uc = new ics::UnknownConnection<asio::ip::tcp>(std::move(s));
				uc->start();
		});
		

		LOG_DEBUG("The server start...");

		io_service.run();

		LOG_DEBUG("The server stop...");
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

	return 0;
}