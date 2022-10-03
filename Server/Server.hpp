#ifndef _Server_hpp_
#define _Server_hpp_

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN // Windows.h和WinSock2.h中函数有重叠
#define _WINSOCK_DEPRECATED_NO_WARNINGS
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
#include "MessageHeader.hpp"

class Server
{
private:
	SOCKET _sock;
	std::vector<SOCKET> g_clients;

public:
	Server()
	{
		_sock = INVALID_SOCKET;
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
		//建立一个socket
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
		if (ip != nullptr) {
			_sin.sin_addr.S_un.S_addr = *ip;
		}
		else {
			_sin.sin_addr.S_un.S_addr = INADDR_ANY;
		}

#else
		if (ip != nullptr) {
			_sin.sin_addr.s_addr = *ip;
		}
		else {
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
	int Listen(int n) {
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

		NewUserJoin userJoin;
		SendData2All(&userJoin);

		//将客户端socket添加进队列
		g_clients.push_back(_cSock);
		printf("新客户端加入,Socket=<%d>,IP=%s\n", (int)_cSock, inet_ntoa(clientAddr.sin_addr));
		return _cSock;
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

			SOCKET maxSock = _sock;

			//将客户端socket放入可读队列
			// for (size_t n = 0; n < g_clients.size(); n++) { 每次比较都要调用size()方法,改写成-->
			for (int n = (int)g_clients.size() - 1; n >= 0; n--)
			{
				FD_SET(g_clients[n], &fdRead);
				if (maxSock < g_clients[n])
				{
					maxSock = g_clients[n];
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
				Accept();

			}
			//遍历可读队列
			for (int i = (int)g_clients.size() - 1; i >= 0; i--)
			{
				if (FD_ISSET(g_clients[i], &fdRead))
				{

					if (-1 == RecvData(g_clients[i]))//如果任务出错
					{
						auto iter = g_clients.begin() + i;
						if (iter != g_clients.end())
						{
							g_clients.erase(iter);
						}
					}
				}
			}
		}
		return true;
	}
	//处理网络消息
	bool IsRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//接收数据
	int RecvData(SOCKET _cSock)
	{
		//缓冲区
		char szRecv[4096] = {};
		// 5. 接收客户端数据
		int nlen = recv(_cSock, szRecv, sizeof(DataHeader), 0);
		DataHeader* header = (DataHeader*)szRecv;
		if (nlen <= 0)
		{
			printf("客户端已退出,任务结束!\n");
			return -1;
		}
		// 6. 处理客户端请求
		recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		OnNetMsg(_cSock, header);
		return 0;
	}
	//发送指定SOCKET数据
	int SendData(SOCKET _cSock, DataHeader* header) {
		if (IsRun() && header) {
			return send(_cSock, (const char*)&header, header->dataLength, 0);
		}
		return INVALID_SOCKET;

	}
	//发送指定SOCKET数据
	void SendData2All(DataHeader* header) {
		if (IsRun() && header) {
			for (int i = (int)g_clients.size() - 1; i >= 0; i--)
			{
				SendData(g_clients[i], header);
			}
		}
	}

	//响应消息
	void OnNetMsg(SOCKET _cSock, DataHeader* header) {
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			Login* login = (Login*)header;
			printf("收到的命令:CMD_LOGIN,数据长度:%d,用户名:%s,密码:%s\n", login->dataLength, login->username, login->password);
			//判断逻辑
			LoginResult ret;
			ret.result = 1;
			send(_cSock, (const char*)&ret, sizeof(LoginResult), 0);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			printf("收到的命令:CMD_LOGOUT,数据长度:%d,用户名:%s,密码:%s\n", logout->dataLength, logout->username, logout->password);
			LogoutResult ret;
			ret.result = 1;
			send(_cSock, (const char*)&ret, sizeof(LogoutResult), 0);
		}
		break;
		}
	}
	//关闭socket
	void Close()
	{
		//结束时,关闭所有客户端连接
		for (int n = (int)g_clients.size() - 1; n >= 0; n--)
		{

#ifdef _WIN32
			closesocket(g_clients[n]);
#else
			close(g_clients[n]);
#endif
		}

#ifdef _WIN32
		// 0. 关闭Socket
		closesocket(_sock);
		WSACleanup();
#else
		close(_sock);
#endif
	}
};

#endif // !_Server_hpp_