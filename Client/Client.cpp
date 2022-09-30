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
#include <thread>




enum CMD
{
	CMD_LOGIN,
	CMD_LOGIN_RESULT,
	CMD_LOGOUT,
	CMD_LOGOUT_RESULT,
	CMD_NEW_USER_JOIN,
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
struct NewUserJoin : public DataHeader
{
	NewUserJoin()
	{
		cmd = CMD_NEW_USER_JOIN;
		dataLength = sizeof(NewUserJoin);
	}
	int sock;
};
bool g_bRun = true;
int processor(SOCKET _cSock)
{

	//缓冲区
	char szRecv[1024] = {};
	// 5. 接收客户端数据
	int nlen = recv(_cSock, szRecv, sizeof(DataHeader), 0);
	DataHeader* header = (DataHeader*)szRecv;
	if (nlen <= 0)
	{
		printf("与服务器断开连接,任务结束!\n");
		return -1;
	}
	// 6. 处理客户端请求
	switch (header->cmd)
	{
	case CMD_LOGIN_RESULT:
	{
		recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		LoginResult* login = (LoginResult*)szRecv;
		printf("收到服务器消息:CMD_LOGIN_RESULT,数据长度:%d\n", login->dataLength);
	}
	break;
	case CMD_LOGOUT_RESULT:
	{
		recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		LogoutResult* logout = (LogoutResult*)szRecv;
		printf("收到服务器消息:CMD_LOGOUT_RESULT,数据长度:%d\n", logout->dataLength);
	}
	break;
	case CMD_NEW_USER_JOIN:
	{
		recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		NewUserJoin* userJoin = (NewUserJoin*)szRecv;
		printf("收到服务器消息:CMD_NEW_USER_JOIN,数据长度:%d\n", userJoin->dataLength);
	}
	break;
	}
	return 0;
}

void cmdThread(SOCKET _sock) {
	while (true) {
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		//4. 处理请求数据
		if (0 == strcmp(cmdBuf, "exit")) {
			g_bRun = false;
			printf("退出\n");
			return;
		}
		else if (0 == strcmp(cmdBuf, "login")) {
			Login login;
			strcpy(login.username, "xu");
			strcpy(login.password, "1234");

			send(_sock, (const char*)&login, sizeof(login), 0);

			//LoginResult loginRet = {};
			//recv(_sock, (char*)&loginRet, sizeof(loginRet), 0);
			//printf("请求结果:%d\n", loginRet.result);
		}
		else if (0 == strcmp(cmdBuf, "logout")) {

			Logout logout;
			strcpy(logout.username, "xu");
			strcpy(logout.password, "1234");

			send(_sock, (const char*)&logout, sizeof(logout), 0);

			//LogoutResult logoutRet = {};
			//recv(_sock, (char*)&logoutRet, sizeof(logoutRet), 0);
			//printf("请求结果:%d\n", logoutRet.result);
		}
		else
		{
			printf("不支持此命令!\n");
			//send(_sock, sendBuf, strlen(sendBuf) + 1, 0);
		}
	}

}


int main() {

#ifdef _WIN32
	//启动Windows socket 2.x环境
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);
#endif
	//1. 建立一个socket套接字
	SOCKET _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sock) {
		printf("建立SOCKET失败\n");
	}
	else {
		printf("建立SOCKET成功\n");
	}

	//2. 连接服务器
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(4567);

#ifdef _WIN32
	_sin.sin_addr.S_un.S_addr = inet_addr("172.26.238.123");
#else
	_sin.sin_addr.s_addr = inet_addr("172.26.224.1");
#endif
	if (SOCKET_ERROR == connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in))) {
		printf("连接服务器失败\n");
	}
	else {
		printf("连接服务器成功\n");
	}

	//启动线程
	std::thread t1(cmdThread, _sock);
	t1.detach();

	while (g_bRun) {

		fd_set fdReads;
		FD_ZERO(&fdReads);
		FD_SET(_sock, &fdReads);

		timeval t = { 1,0 };//设置最大停留时间,不阻塞
		int ret = select((int)_sock + 1, &fdReads, 0, 0, &t);
		if (ret < 0) {
			printf("select任务结束\n");
			break;
		}

		//这里可以处理空闲时的其他业务...
		//printf("空闲时间,处理其他业务...\n");


		if (FD_ISSET(_sock, &fdReads)) {
			FD_CLR(_sock, &fdReads);

			if (-1 == processor(_sock)) {
				printf("select任务结束\n");
				break;
			}
		}





	}
#ifdef _WIN32
	//4. 关闭Socket
	closesocket(_sock);
	WSACleanup();
#else
	close(_sock);
#endif
	return 0;
}