
#include "tcpserver.h"
#include "icsclient.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>

using namespace std;


void test_client()
{
	
	ics::IcsClientManager m("192.168.50.133", "2100");

	m.createIcsClient({"test1", "test1", 2, "ext_info"});

	m.run();

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

int main()
{
	cout << "start..." << endl;

	test_client();

	cout << "stop..." << endl;

	return 0;
}