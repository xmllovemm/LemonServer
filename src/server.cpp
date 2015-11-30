

#include "database.hpp"
#include "mempool.hpp"
#include "icsconfig.hpp"
#include "log.hpp"
#include "util.hpp"
#include "icspushsystem.hpp"
#include "tcpserver.hpp"
#include "icsconnection.hpp"
//#include "clientmanager.hpp"
#include "icsconfig.hpp"
#include <asio.hpp>

#include <iostream>
#include <string>


using namespace std;

ics::IcsConfig g_configFile;
ics::PushSystem g_pushSystem;
ics::MemoryPool g_memoryPool;
ics::DataBase g_database;
//ics::ClientManager<asio::ip::tcp, asio::ip::tcp> g_clientManager;


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

		// load g_configFile file
		g_configFile.load(argv[1]);

		// init log file
		ics::init_log(g_configFile.getAttributeString("log", "configfile").c_str());

		if (g_configFile.getAttributeInt("program", "daemon"))
		{
			ics::be_daemon(g_configFile.getAttributeString("program", "workdir").c_str());
		}


		ics::DataBase::initialize();

		g_pushSystem.init(g_configFile.getAttributeString("pushmsg", "serverip"), g_configFile.getAttributeInt("pushmsg", "serverport"));

		g_memoryPool.init(g_configFile.getAttributeInt("program", "chunksize"), g_configFile.getAttributeInt("program", "chunkcount"));

		g_database.init(g_configFile.getAttributeString("database", "username"), g_configFile.getAttributeString("database", "password"), g_configFile.getAttributeString("database", "dsn"));

		g_database.open();
		
		clientServer.init(g_configFile.getAttributeString("listen", "addr")
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