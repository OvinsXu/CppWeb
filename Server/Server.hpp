#ifndef _Server_hpp_
#define _Server_hpp_

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN // Windows.h和WinSock2.h中函数有重叠
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#define FD_SETSIZE 4096 // WinSock2.h里默认64,可以设置,Linux只能到1024

#include <Windows.h>
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

#else

#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#define SOCKET int
#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)

#endif

#include <stdio.h>
#include <vector>
#include <mutex>
#include "MessageHeader.hpp"
#include <thread>
#include "CELLTimestamp.hpp"

#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 40960
#endif // !RECV_BUFF_SIZE



//客户端对象:为解决粘包,给每一个客户端socket建立接收缓冲区,并记录游标
class ClientSocket
{
private:
	SOCKET _sockfd;
	//第二缓冲区
	char _szMsgBuf[RECV_BUFF_SIZE * 10];
	//第二缓冲区,末标
	int _lastPos;
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET)
	{
		_sockfd = sockfd;
		memset(_szMsgBuf, 0, sizeof(_szMsgBuf));
		_lastPos = 0;
	}
	SOCKET sockfd()
	{
		return _sockfd;
	}
	char* msgBuf()
	{
		return _szMsgBuf;
	}
	int getLastPos()
	{
		return _lastPos;
	}
	void setLastPos(int pos)
	{
		_lastPos = pos;
	}
	//发送指定SOCKET数据
	int SendData(DataHeader* header)
	{
		if (header)
		{
			return send(_sockfd, (const char*)&header, header->dataLength, 0);
		}
		return INVALID_SOCKET;
	}


};

//事件通知机制
class INetEvent
{
public:
	//纯虚函数
	virtual void OnNetJoin(ClientSocket* pClient) = 0;				//客户端加入事件
	virtual void OnNetLeave(ClientSocket* pClient) = 0;			//客户端离开:消费者 通知-->生产者 ,有四个cellserver线程可能会调用,通知一个server线程执行业务,线程安全
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header) = 0;//客户端消息
};

//多线程处理客户端任务,使用生产者-消费者模式,一个Server生产者监听,分配给多个CellServer消费者处理
class CellServer
{
private:
	SOCKET _sock;
	std::vector<ClientSocket*> _clients;	  //正式客户队列
	std::vector<ClientSocket*> _clientsBuff; //缓存客户队列
	std::mutex _mutex;						//用于加锁
	std::thread _thread;					//用于创建消费者线程
	INetEvent* _pNetEvent;					//用于事件通知


public:
	CellServer(SOCKET sock = INVALID_SOCKET)
	{
		_sock = sock;
		_pNetEvent = nullptr;
	}

	virtual ~CellServer()
	{
		Close();
		_sock = INVALID_SOCKET;
	}

	//记录一下对象
	void setEventObj(INetEvent* pNetEvent)
	{
		_pNetEvent = pNetEvent;
	}

	//处理网络消息
	bool OnRun()
	{

		while (IsRun())
		{
			if (_clientsBuff.size() > 0)
			{
				//缓存队列-->正式客户队列
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff)
				{
					_clients.push_back(pClient);
				}
				_clientsBuff.clear();
			}
			if (_clients.empty())
			{
				std::chrono::microseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}
			//定义可读,可写.异常 三个队列
			fd_set fdRead;
			fd_set fdWrite;
			fd_set fdExp;

			//将队列清零
			FD_ZERO(&fdRead);
			FD_ZERO(&fdWrite);
			FD_ZERO(&fdExp);

			SOCKET maxSock = _clients[0]->sockfd();

			//取得最大的socket
			// for (size_t n = 0; n < g_clients.size(); n++) { 每次比较都要调用size()方法,改写成-->
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				FD_SET(_clients[n]->sockfd(), &fdRead);
				if (maxSock < _clients[n]->sockfd())
				{
					maxSock = _clients[n]->sockfd();
				}
			}
			timeval t = { 1, 0 }; //设置最大停留时间,不阻塞

			//开启select,开启后,系统内核会轮询队列
			int ret = select((int)maxSock + 1, &fdRead, &fdWrite, &fdExp, &t); //第一个参数,在Windows下无意义,最后一个参数设置为NULL时,则会一直阻塞在这里
			if (ret < 0)
			{
				printf("select任务结束.\n");
				Close();
				return false;
			}
			//这里可以处理空闲时的其他业务...
			// printf("空闲时间,处理其他业务...\n");

			//判断描述符是否在集合中
			if (FD_ISSET(_sock, &fdRead))
			{
				//先把状态清除
				FD_CLR(_sock, &fdRead);
			}
			//遍历可读队列
			for (int i = (int)_clients.size() - 1; i >= 0; i--)
			{
				if (FD_ISSET(_clients[i]->sockfd(), &fdRead))
				{

					if (-1 == RecvData(_clients[i])) //如果任务出错
					{
						auto iter = _clients.begin() + i;
						if (iter != _clients.end())
						{
							if (_pNetEvent)
							{
								_pNetEvent->OnNetLeave(_clients[i]);
							}
							delete _clients[i];
							_clients.erase(iter);
						}
					}
				}
			}
		}
	}
	//接收缓冲区
	char _szRecv[RECV_BUFF_SIZE] = {};

	//接收数据
	int RecvData(ClientSocket* cSock)
	{

		// 5. 接收客户端数据
		int nlen = recv(cSock->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);

		if (nlen <= 0)
		{
			//printf("SOCKET=<%d>客户端已退出,任务结束!\n", (int)cSock->sockfd());

			return -1;
		}

		memcpy(cSock->msgBuf() + cSock->getLastPos(), _szRecv, nlen); //放到第二缓存区
		cSock->setLastPos(cSock->getLastPos() + nlen);

		while (cSock->getLastPos() >= sizeof(DataHeader))
		{ //如果接收数据含有完整消息头
			DataHeader* header = (DataHeader*)cSock->msgBuf();
			if (cSock->getLastPos() >= header->dataLength)
			{ //如果接收数据含有完整消息报

				int nSize = cSock->getLastPos() - header->dataLength;
				OnNetMsg(cSock, header);

				memcpy(cSock->msgBuf(), cSock->msgBuf() + header->dataLength, nSize); //放到第二缓存区
				cSock->setLastPos(nSize);
			}
			else
			{
				break;
			}
		}
		return 0;
	}

	bool IsRun()
	{
		return _sock != INVALID_SOCKET;
	}
	//响应消息
	void OnNetMsg(ClientSocket* pClient, DataHeader* header)
	{
		if (_pNetEvent)
		{
			_pNetEvent->OnNetMsg(pClient, header);
		}
		// }
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			Login* login = (Login*)header;
			//printf("收到的命令:CMD_LOGIN,数据长度:%d,用户名:%s,密码:%s\n", login->dataLength, login->username, login->password);
			//判断逻辑
			LoginResult ret;
			ret.result = 1;
			send(pClient->sockfd(), (const char*)&ret, sizeof(LoginResult), 0);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			printf("收到的命令:CMD_LOGOUT,数据长度:%d,用户名:%s,密码:%s\n", logout->dataLength, logout->username, logout->password);
			LogoutResult ret;
			ret.result = 1;
			send(pClient->sockfd(), (const char*)&ret, sizeof(LogoutResult), 0);
		}
		break;
		}
	}
	void Start()
	{
		_thread = std::thread(&CellServer::OnRun, this);
	}
	int getClientCount()
	{
		return _clients.size() + _clientsBuff.size();
	}
	//关闭socket
	void Close()
	{
		//结束时,关闭所有客户端连接
		for (int n = (int)_clients.size() - 1; n >= 0; n--)
		{

#ifdef _WIN32
			closesocket(_clients[n]->sockfd());
#else
			close(_clients[n]->sockfd());
#endif
			delete _clients[n];
		}

#ifdef _WIN32
		// 0. 关闭Socket
		closesocket(_sock);
		WSACleanup();
		_sock = INVALID_SOCKET;
#else
		close(_sock);
#endif
		_clients.clear();
	}
	void addClient(ClientSocket* pClient)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		//_mutex.lock();
		_clientsBuff.push_back(pClient);
		//_mutex.unlock();
	}
};

class Server : public INetEvent
{
private:
	SOCKET _sock;							//服务器监听socket
	//std::vector<ClientSocket*> _clients;	//接到的连接socket,会分配给具体的处理线程
	std::vector<CellServer*> _cellServers; //消费者线程队列
	CELLTimestamp _tTime;					//数据包计时
	std::atomic_int _recvCnt;				//数据包计数
	std::atomic_int _clientCnt;				//客户端计数
public:
	Server()
	{
		_sock = INVALID_SOCKET;
		_recvCnt = 0;
	}

	virtual ~Server()
	{
		Close();
	}
	//初始化socket
	void initSocket()
	{
#ifdef _WIN32
		//启动Windows socket 2.x环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (_sock != INVALID_SOCKET)
		{
			printf("SOCKET=<%d>关闭旧连接\n", (int)_sock);
			Close();
		}
		//建立socket
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("建立SOCKET=<%d>失败\n", (int)_sock);
		}
		else
		{
			printf("建立SOCKET=<%d>成功\n", (int)_sock);
		}
	}
	//绑定IP和端口号
	int Bind(const char* ip, unsigned short port)
	{
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);
#ifdef _WIN32
		if (ip != nullptr)
		{
			_sin.sin_addr.S_un.S_addr = *ip;
		}
		else
		{
			_sin.sin_addr.S_un.S_addr = INADDR_ANY;
		}

#else
		if (ip != nullptr)
		{
			_sin.sin_addr.s_addr = *ip;
		}
		else
		{
			_sin.sin_addr.s_addr = INADDR_ANY;
		}

#endif
		int ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
		if (SOCKET_ERROR == ret)
		{
			printf("SOCKET=<%d>绑定端口<%d>错误\n", (int)_sock, port);
		}
		else
		{
			printf("SOCKET=<%d>绑定端口<%d>成功\n", (int)_sock, port);
		}
		return ret;
	}
	//监听端口号
	int Listen(int n)
	{
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)
		{
			printf("SOCKET=<%d>监听IP端口错误\n", (int)_sock);
		}
		else
		{
			printf("SOCKET=<%d>监听IP端口成功\n", (int)_sock);
		}
		return ret;
	}

	//接受客户端连接
	SOCKET Accept()
	{
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET _cSock = INVALID_SOCKET;
#ifdef _WIN32
		_cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		_cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#endif
		if (INVALID_SOCKET == _cSock)
		{
			printf("错误,接收到无效客户端SOCKET\n");
		}
		else
		{
			/*NewUserJoin userJoin;
		SendData2All(&userJoin);*/

		//将客户端socket添加进队列
			addClientToCellServer(new ClientSocket(_cSock));
			// printf("新客户端加入,Socket=<%d>,IP=%s\n", (int)_cSock, inet_ntoa(clientAddr.sin_addr));
		}

		return _cSock;
	}
	void addClientToCellServer(ClientSocket* pClient)
	{
		//_clients.push_back(pClient);
		auto minCellSvr = _cellServers[0];
		for (auto cSvr : _cellServers)
		{
			if (minCellSvr->getClientCount() > cSvr->getClientCount())
			{
				minCellSvr = cSvr;
			}
		}
		minCellSvr->addClient(pClient);
		OnNetJoin(pClient);
	}
	//接收缓冲区
	char _szRecv[RECV_BUFF_SIZE] = {};



	//判断是否工作中(监听连接)
	bool IsRun()
	{
		return _sock != INVALID_SOCKET;
	}
	//处理网络消息
	bool OnRun()
	{
		if (IsRun())
		{
			//定义可读,可写.异常 三个队列
			fd_set fdRead;
			fd_set fdWrite;
			fd_set fdExp;

			//将队列清零
			FD_ZERO(&fdRead);
			FD_ZERO(&fdWrite);
			FD_ZERO(&fdExp);

			//将服务器socket添加进三个队列
			FD_SET(_sock, &fdRead);
			FD_SET(_sock, &fdWrite);
			FD_SET(_sock, &fdExp);

			Time4Msg();

			timeval t = { 0, 10 }; //设置最大停留时间,不阻塞 (秒,毫秒)

			//开启select,开启后,系统内核会轮询队列
			int ret = select((int)_sock + 1, &fdRead, &fdWrite, &fdExp, &t); //第一个参数,在Windows下无意义,最后一个参数设置为NULL时,则会一直阻塞在这里
			if (ret < 0)
			{
				printf("select任务结束.\n");
				Close();
				return false;
			}
			//这里可以处理空闲时的其他业务...
			// printf("空闲时间,处理其他业务...\n");

			//判断描述符是否在集合中
			if (FD_ISSET(_sock, &fdRead))
			{
				//先把状态清除
				FD_CLR(_sock, &fdRead);
				Accept();
				return true;
			}
			return true;
		}
		return false;
	}

	//响应消息
	void Time4Msg()
	{
		auto t1 = _tTime.getElapsedTimeInSec();
		if (t1 >= 1.0)
		{
			printf("线程数量=<%d>,客户端数量=<%d>,每秒收到数据包=<%d>\n", (int)_cellServers.size(), (int)_clientCnt, (int)(_recvCnt / t1));
			_tTime.update();
			_recvCnt = 0;
		}
	}

	void Start(unsigned int tCnt)
	{
		for (int i = 0; i < tCnt; i++)
		{
			auto ser = new CellServer(_sock);
			_cellServers.push_back(ser);
			ser->setEventObj(this);
			ser->Start();
		}
	}
	//关闭socket
	void Close()
	{
		//结束时,关闭所有客户端连接
		/*for (int n = (int)_clients.size() - 1; n >= 0; n--)
		{

#ifdef _WIN32
			closesocket(_clients[n]->sockfd());
#else
			close(_clients[n]->sockfd());
#endif
			delete _clients[n];
		}*/

#ifdef _WIN32
		// 0. 关闭Socket
		closesocket(_sock);
		WSACleanup();
		_sock = INVALID_SOCKET;
#else
		close(_sock);
#endif
		//_clients.clear();
	}
	virtual void OnNetJoin(ClientSocket* pClient) {
		_clientCnt++;
	}
	//消费者通知生产者客户端退出
	virtual void OnNetLeave(ClientSocket* pClient)
	{
		_clientCnt--;
		/*for (int i = (int)_clients.size() - 1; i >= 0; i--)
		{
			if (_clients[i] == pClient)
			{
				auto iter = _clients.begin() + i;
				if (iter != _clients.end())
				{
					_clients.erase(iter);
				}
			}
		}*/
	}
	//消费者通知生产者接收到数据包
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header) {
		_recvCnt++;
	}
};

#endif // !_Server_hpp_