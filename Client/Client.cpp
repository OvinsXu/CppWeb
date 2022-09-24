#define WIN32_LEAN_AND_MEAN		//Windows.h和WinSock2.h中函数有重叠
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include<WinSock2.h>
#include<stdio.h>
#pragma comment(lib,"ws2_32.lib")

enum CMD {
	CMD_LOGIN,
	CMD_LOGIN_RESULT,
	CMD_LOGOUT,
	CMD_LOGOUT_RESULT,
	CMD_ERROR
};

struct DataHeader {	//消息头
	short cmd;
	short dataLength;
};
struct Login :public DataHeader
{
	Login() {
		cmd = CMD_LOGIN;
		dataLength = sizeof(Login);
	}
	char username[32];
	char password[32];
};
struct Logout :public DataHeader
{
	Logout() {
		cmd = CMD_LOGOUT;
		dataLength = sizeof(Logout);
	}
	char username[32];
	char password[32];
};
struct LoginResult :public DataHeader
{
	LoginResult() {
		cmd = CMD_LOGIN_RESULT;
		dataLength = sizeof(LoginResult);
	}
	int result;
};
struct LogoutResult :public DataHeader
{
	LogoutResult() {
		cmd = CMD_LOGOUT_RESULT;
		dataLength = sizeof(LogoutResult);
	}
	int result;
};


int main() {
	//启动Windows socket 2.x环境
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);

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
	_sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	if (SOCKET_ERROR == connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in))) {
		printf("连接服务器失败\n");
	}
	else {
		printf("连接服务器成功\n");
	}



	while (true) {
		//3. 输入请求命令
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		//4. 处理请求数据
		if (0 == strcmp(cmdBuf, "exit")) {
			break;
		}
		else if (0 == strcmp(cmdBuf, "login")) {
			Login login;
			strcpy(login.username, "xu");
			strcpy(login.password, "1234");

			send(_sock, (const char*)&login, sizeof(login), 0);

			LoginResult loginRet = {};
			recv(_sock, (char*)&loginRet, sizeof(loginRet), 0);
			printf("请求结果:%d\n", loginRet.result);
		}
		else if (0 == strcmp(cmdBuf, "logout")) {

			Logout logout;
			strcpy(logout.username, "xu");
			strcpy(logout.password, "1234");

			send(_sock, (const char*)&logout, sizeof(logout), 0);

			LogoutResult logoutRet = {};
			recv(_sock, (char*)&logoutRet, sizeof(logoutRet), 0);
			printf("请求结果:%d\n", logoutRet.result);
		}
		else
		{
			printf("不支持此命令!\n");
			//send(_sock, sendBuf, strlen(sendBuf) + 1, 0);
		}


	}

	//4. 关闭Socket
	closesocket(_sock);

	WSACleanup();
	return 0;
}