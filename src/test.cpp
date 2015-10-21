
#include "tcpserver.h"
#include "clientmanager.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>

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


///*
namespace ics{


template<typename ...args>	class MyTuple;


template<> class MyTuple<>{};


template<typename _This, typename... _Rest>
class MyTuple<_This, _Rest...> : private MyTuple<_Rest...>
{
public:
	typename _This _this_type;
//	typename MyTuple<_This, _Rest...> _my_type;
	typename MyTuple<_Rest...> _base_type;

	/*
	my_tuple(_This&& this_arg, _Rest&&... rest_args) : _base_type(std::forward<_Rest>(rest_args...)), m_this(std::forward<_This>(this_arg))
	{
		cout << "this:" << m_head << endl;
	}
	*/

	MyTuple(_This this_arg, _Rest... rest_args) : _base_type(rest_args...), m_this(this_arg)
	{
		cout << "this:" << m_this << ",count: " << sizeof...(rest_args) << endl;
	}

	MyTuple()
	{
		cout << "over" << endl;
	}

private:
	_This	m_this;
};

template<size_t _index, typename... _tuple>
class MyTupleElement;

template<typename _This,typename _Rest>
class MyTupleElement<0, MyTuple<_This,_Rest> >
{
	
};

template<size_t _index, typename... _types>
void MyGet(MyTuple<_types...>& _tuple)
{

};



}
//*/

template<typename Func, typename... Args>
inline auto FuncWraper(Func&& f, Args&&... args) ->decltype(f(std::forward<Args>(args)...))
{
	return f(std::forward<Args>(args)...);
}

int main()
{
	cout << "start..." << endl;

	test_server();

	/*
//	ics::MyTuple<int, char, double> m(1, 'x', 1.23);

	FuncWraper([](int a, int b)
		{
			cout << "a:" << a << ",b:" << b << endl;
		},
		1, 2);
	*/
	cout << "stop..." << endl;

	int n;
	cin >> n;

	return 0;
}