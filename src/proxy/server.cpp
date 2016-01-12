
#include "config.hpp"
#include "mempool.hpp"
#include "log.hpp"
#include "util.hpp"
#include "icsconnection.hpp"
#include "icsconfig.hpp"
#include "icsproxyserver.hpp"
#include "database.hpp"

#include <asio.hpp>
#include <iostream>
#include <string>


using namespace std;

ics::IcsConfig g_configFile;
ics::MemoryPool g_memoryPool;
ics::DataBase g_database;

void usage(const char* prog)
{
	cerr << "useage:" << prog << " configfile" << endl;
}



int main(int argc, char** argv)
{
	const char* configFile = "/etc/icsconf.xml";
	if (argc >= 2)
	{
		configFile = argv[1];
	}

	asio::io_service io_service;

	// 信号处理
	asio::signal_set signals(io_service);
	signals.add(SIGINT);
	signals.add(SIGTERM);
	signals.async_wait([&io_service](asio::error_code ec, int signo)
	{
		cout << "catch a signal " << signo << ",exit..." << endl;
		io_service.stop();
	});
	
	try {
		// 加载配置文件
		g_configFile.load(configFile);

		// 初始进程模式
		if (g_configFile.getAttributeInt("program", "daemon"))
		{
			ics::be_daemon(g_configFile.getAttributeString("program", "workdir").c_str());
		}

		// 初始日志模块
		ics::init_log(g_configFile.getAttributeString("log", "configfile").c_str());

		// 初始内存池模块
		g_memoryPool.init(g_configFile.getAttributeInt("program", "chunksize"), g_configFile.getAttributeInt("program", "chunkcount"));

		// ICS代理模式
		auto p = std::make_unique<ics::IcsPorxyServer>(io_service
			, g_configFile.getAttributeString("proxyraddr", "terminal"), 100
			, g_configFile.getAttributeString("proxyraddr", "center"), 100);		

		// 主线程开始IO事件,忽略错误
		asio::error_code ec;
		io_service.run(ec);
	}
	catch (ics::IcsException& ex)
	{
		cerr << "init failed ics exception: " << ex.message() << endl;
		return 1;
	}
	catch (std::exception& ex)
	{
		cerr << "init failed std exception: " << ex.what() << endl;
		return 2;
	}
	catch (otl_exception& ex)
	{
		cerr << "init failed database exception: " << ex.msg << endl;
		return 3;
	}
	catch (...)
	{
		cerr << "unknown error" << endl;
		return 4;
	}


	cout << "The process stoped";

	return 0;
}