
#include "tcpserver.h"
#include "clientmanager.h"

#include <iostream>

using namespace std;


void test_server()
{
	
	try {
		ics::ClientManager cm(10);
		ics::TcpServer s(9999, [&cm](asio::ip::tcp::socket s){
					cm.addClient(std::move(s));
				});
		s.run();
	}
	catch (std::exception& ex)
	{
		cerr << ex.what() << endl;
	}
}


int main()
{
	cout << "start..." << endl;

	test_server();

	cout << "stop..." << endl;

	int n;
	cin >> n;

	return 0;
}