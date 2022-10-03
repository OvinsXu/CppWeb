//#pragma once	//vc++可用

#ifndef _Client_hpp_
#define _Client_hpp_

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN		//Windows.h和WinSock2.h中函数有重叠
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")

#else

#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#define SOCKET int
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)

#endif
#include <stdio.h>
#include"MessageHeader.hpp"

class Client
{
	SOCKET _sock;
public:
	Client() {
		_sock = INVALID_SOCKET;
	}
	virtual ~Client() {
		Close();
	}
	//初始化socket
	void initSocket() {
#ifdef _WIN32
		//启动Windows socket 2.x环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (_sock != INVALID_SOCKET) {
			printf("SOCKET=<%d>关闭旧的连接\n", (int)_sock);
			Close();
		}
		//建立一个socket
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		if (INVALID_SOCKET == _sock) {
			printf("建立SOCKET失败\n");
		}
		else {
			printf("建立SOCKET=<%d>成功\n", (int)_sock);
		}
	}
	//连接服务器
	int Connect(const char* ip, unsigned short port) {

		if (_sock == INVALID_SOCKET) {
			initSocket();
		}

		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);

#ifdef _WIN32
		_sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		_sin.sin_addr.s_addr = inet_addr(ip);
#endif
		int ret = connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret) {
			printf("连接服务器失败\n");
		}
		else {
			printf("连接服务器成功\n");
		}
		return ret;
	}
	//关闭socket
	void Close() {
		if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
			//4. 关闭Socket
			closesocket(_sock);
			WSACleanup();
#else
			close(_sock);
#endif
			_sock = INVALID_SOCKET;
		}
	}
	//查询网络消息
	bool OnRun() {
		if (IsRun()) {
			fd_set fdReads;
			FD_ZERO(&fdReads);
			FD_SET(_sock, &fdReads);

			timeval t = { 1,0 };//设置最大停留时间,不阻塞
			int ret = select((int)_sock + 1, &fdReads, 0, 0, &t);
			if (ret < 0) {
				printf("select任务结束\n");
				return false;
			}
			//这里可以处理空闲时的其他业务...
			//printf("空闲时间,处理其他业务...\n");


			if (FD_ISSET(_sock, &fdReads)) {
				FD_CLR(_sock, &fdReads);

				if (-1 == RecvData(_sock)) {
					printf("select任务结束\n");
					return false;
				}
			}
			return true;
		}
		return false;
	}
	//是否工作中
	bool IsRun() {
		return _sock != INVALID_SOCKET;
	}
	//接收数据
	int RecvData(SOCKET _sock)
	{
		//缓冲区
		char szRecv[4096] = {};
		// 5. 接收客户端数据
		int nlen = recv(_sock, szRecv, sizeof(DataHeader), 0);
		DataHeader* header = (DataHeader*)szRecv;
		if (nlen <= 0)
		{
			printf("与服务器断开连接,任务结束!\n");
			return -1;
		}
		recv(_sock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		OnNetMsg(header);
		return 0;
	}
	//发送数据
	int SendData(DataHeader* header) {
		if (IsRun() && header) {
			return send(_sock, (const char*)header, header->dataLength, 0);
		}
		return INVALID_SOCKET;

	}
	//响应消息
	void OnNetMsg(DataHeader* header) {
		// 6. 处理客户端请求
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT:
		{

			LoginResult* login = (LoginResult*)header;
			printf("收到服务器消息:CMD_LOGIN_RESULT,数据长度:%d\n", login->dataLength);
		}
		break;
		case CMD_LOGOUT_RESULT:
		{

			LogoutResult* logout = (LogoutResult*)header;
			printf("收到服务器消息:CMD_LOGOUT_RESULT,数据长度:%d\n", logout->dataLength);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{

			NewUserJoin* userJoin = (NewUserJoin*)header;
			printf("收到服务器消息:CMD_NEW_USER_JOIN,数据长度:%d\n", userJoin->dataLength);
		}
		break;
		}
	}


private:

};


#endif // !_Client_hpp_

