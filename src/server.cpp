
#include "config.hpp"
#include "mempool.hpp"
#include "log.hpp"
#include "util.hpp"
#include "icslocalserver.hpp"
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

int itostr(char* buf, int value)
{
	// numerator = denomerator * quotient + remainder
	// n = d * q + r
	static const char* digitalsBuff = "9876543210123456789";
	const char* digital = digitalsBuff + 9;
	char* outbuf = buf;
	
	if (value < 0)
	{
		*outbuf++ = '-';
	}

	int q = value;
	do
	{	
		*outbuf++ = digital[q % 10];
		q /= 10;
	} while (q != 0);

	*outbuf = 0;

	std::reverse(buf + (value < 0 ? 1 : 0), outbuf);

	return outbuf - buf;
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

		// 初始日志模块
		ics::init_log(g_configFile.getAttributeString("log", "configfile").c_str());

		// 初始内存池模块
		g_memoryPool.init(g_configFile.getAttributeInt("program", "chunksize"), g_configFile.getAttributeInt("program", "chunkcount"));

		// 初始主服务
#ifdef ICS_CENTER_MODE	// ICS中心模式
		ics::DataBase::initialize();
		g_database.init(g_configFile.getAttributeString("database", "username"), g_configFile.getAttributeString("database", "password"), g_configFile.getAttributeString("database", "dsn"));
		g_database.open();
		
		auto p = std::make_unique<ics::IcsLocalServer>(io_service
			, g_configFile.getAttributeString("centeraddr", "terminal"), 100
			, g_configFile.getAttributeString("centeraddr", "web"), 100
			, g_configFile.getAttributeString("centeraddr", "msgpush"));	
#else
		// ICS代理模式
		auto p = std::make_unique<ics::IcsPorxyServer>(io_service
			, g_configFile.getAttributeString("proxyraddr", "terminal"), 100
//			, g_configFile.getAttributeString("proxyraddr", "web"), 100
			, g_configFile.getAttributeString("proxyraddr", "center"), 100);
#endif
		// 初始进程模式
		if (g_configFile.getAttributeInt("program", "daemon"))
		{
			ics::be_daemon(g_configFile.getAttributeString("program", "workdir").c_str());
		}

		// 主线程开始IO事件
		io_service.run();

	}
	catch (ics::IcsException& ex)
	{
		cerr << "init failed ics exception: " << ex.message() << endl;
	}
	catch (std::exception& ex)
	{
		cerr << "init failed std exception: " << ex.what() << endl;
	}
	catch (otl_exception& ex)
	{
		cerr << "init failed database exception: " << ex.msg << endl;
	}
	catch (...)
	{
		cerr << "unknown error" << endl;
	}

	cout << "The process stoped";

	return 0;
}