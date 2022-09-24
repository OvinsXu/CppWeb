#define WIN32_LEAN_AND_MEAN // Windows.h和WinSock2.h中函数有重叠
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <Windows.h>
#include <WinSock2.h>
#include <stdio.h>

#include <vector>
using namespace std;
#pragma comment(lib, "ws2_32.lib")

enum CMD
{
	CMD_LOGIN,
	CMD_LOGIN_RESULT,
	CMD_LOGOUT,
	CMD_LOGOUT_RESULT,
	CMD_ERROR
};

struct DataHeader
{ //消息头
	short cmd;
	short dataLength;
};
struct Login : public DataHeader
{
	Login()
	{
		cmd = CMD_LOGIN;
		dataLength = sizeof(Login);
	}
	char username[32];
	char password[32];
};
struct Logout : public DataHeader
{
	Logout()
	{
		cmd = CMD_LOGOUT;
		dataLength = sizeof(Logout);
	}
	char username[32];
	char password[32];
};
struct LoginResult : public DataHeader
{
	LoginResult()
	{
		cmd = CMD_LOGIN_RESULT;
		dataLength = sizeof(LoginResult);
	}
	int result;
};
struct LogoutResult : public DataHeader
{
	LogoutResult()
	{
		cmd = CMD_LOGOUT_RESULT;
		dataLength = sizeof(LogoutResult);
	}
	int result;
};

vector<SOCKET> g_clients;

int processor(SOCKET _cSock)
{
	//缓冲区
	char szRecv[1024] = {};
	// 5. 接收客户端数据
	int nlen = recv(_cSock, szRecv, sizeof(DataHeader), 0);
	DataHeader* header = (DataHeader*)szRecv;
	if (nlen <= 0)
	{
		printf("客户端已退出,任务结束!\n");
		return -1;
	}
	// 6. 处理客户端请求
	switch (header->cmd)
	{
	case CMD_LOGIN:
	{
		recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		Login* login = (Login*)szRecv;
		printf("收到的命令:CMD_LOGIN,数据长度:%d,用户名:%s,密码:%s\n", login->dataLength, login->username, login->password);
		//判断逻辑
		LoginResult ret;
		ret.result = 1;
		send(_cSock, (const char*)&ret, sizeof(ret), 0);
	}
	break;
	case CMD_LOGOUT:
	{
		recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		Logout* logout = (Logout*)szRecv;
		printf("收到的命令:CMD_LOGOUT,数据长度:%d,用户名:%s,密码:%s\n", logout->dataLength, logout->username, logout->password);
		LogoutResult ret;
		ret.result = 1;
		send(_cSock, (const char*)&ret, sizeof(ret), 0);
	}
	break;
	}
}

int main()
{
	//启动Windows socket 2.x环境
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);

	// 1. 建立一个socket套接字
	SOCKET _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sock)
	{
		printf("建立SOCKET失败\n");
	}
	else
	{
		printf("建立SOCKET成功\n");
	}

	// 2. 绑定端口
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(4567);
	_sin.sin_addr.S_un.S_addr = INADDR_ANY;
	if (SOCKET_ERROR == bind(_sock, (sockaddr*)&_sin, sizeof(_sin)))
	{
		printf("绑定端口错误\n");
	}
	else
	{
		printf("绑定端口成功\n");
	}

	// 3. 监听端口
	if (SOCKET_ERROR == listen(_sock, 5))
	{
		printf("监听端口错误\n");
	}
	else
	{
		printf("监听端口成功\n");
	}

	while (true)
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

		//将客户端socket放入可读队列
		// for (size_t n = 0; n < g_clients.size(); n++) { 每次比较都要调用size()方法,改写成-->
		for (int n = (int)g_clients.size() - 1; n >= 0; n--)
		{
			FD_SET(g_clients[n], &fdRead);
		}

		timeval t = { 0,0 };//设置不停留,不阻塞

		//开启select,开启后,系统内核会轮询队列
		int ret = select(_sock + 1, &fdRead, &fdWrite, &fdExp, &t); //第一个参数,在Windows下无意义,最后一个参数设置为NULL时,则会一直阻塞在这里
		if (ret < 0)
		{
			printf("select任务结束.\n");
			break;
		}
		//如果可读队列中的服务器socket就绪,即有新客户端连接
		if (FD_ISSET(_sock, &fdRead))
		{
			//先把状态清除
			FD_CLR(_sock, &fdRead);

			// 4. 等待客户连接
			sockaddr_in clientAddr = {};
			int nAddrLen = sizeof(sockaddr_in);
			SOCKET _cSock = INVALID_SOCKET;

			_cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
			if (INVALID_SOCKET == _cSock)
			{
				printf("错误,接收到无效客户端SOCKET\n");
			}
			//将客户端socket添加进队列
			g_clients.push_back(_cSock);
			printf("新客户端加入,Socket=%d,IP=%s\n", (int)_cSock, inet_ntoa(clientAddr.sin_addr));
		}

		//遍历可读队列
		for (size_t n = 0; n < fdRead.fd_count; n++)
		{
			//如果客户端socket发生错误
			if (-1 == processor(fdRead.fd_array[n]))
			{
				auto iter = find(g_clients.begin(), g_clients.end(), fdRead.fd_array[n]);
				if (iter != g_clients.end())
				{
					//移除出错的客户端socket
					g_clients.erase(iter);
				}
			}
		}
	}

	//结束时,关闭所有客户端连接
	for (int n = (int)g_clients.size() - 1; n >= 0; n--)
	{
		closesocket(g_clients[n]);
	}

	// 0. 关闭Socket
	closesocket(_sock);

	WSACleanup();
	return 0;
}
