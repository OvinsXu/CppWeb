#define WIN32_LEAN_AND_MEAN		//Windows.h��WinSock2.h�к������ص�
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

struct DataHeader {	//��Ϣͷ
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
	//����Windows socket 2.x����
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);

	//1. ����һ��socket�׽���
	SOCKET _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sock) {
		printf("����SOCKETʧ��\n");
	}
	else {
		printf("����SOCKET�ɹ�\n");
	}

	//2. ���ӷ�����
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(4567);
	_sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	if (SOCKET_ERROR == connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in))) {
		printf("���ӷ�����ʧ��\n");
	}
	else {
		printf("���ӷ������ɹ�\n");
	}



	while (true) {
		//3. ������������
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		//4. ������������
		if (0 == strcmp(cmdBuf, "exit")) {
			printf("�յ�exit,�˳�����\n");
			break;
		}
		else if (0 == strcmp(cmdBuf, "login")) {
			Login login;
			strcpy(login.username, "xu");
			strcpy(login.password, "1234");

			send(_sock, (const char*)&login, sizeof(login), 0);

			LoginResult loginRet = {};
			recv(_sock, (char*)&loginRet, sizeof(loginRet), 0);
			printf("������:%d\n", loginRet.result);
		}
		else if (0 == strcmp(cmdBuf, "logout")) {

			Logout logout;
			strcpy(logout.username, "xu");
			strcpy(logout.password, "1234");

			send(_sock, (const char*)&logout, sizeof(logout), 0);

			LogoutResult logoutRet = {};
			recv(_sock, (char*)&logoutRet, sizeof(logoutRet), 0);
			printf("������:%d\n", logoutRet.result);
		}
		else
		{
			printf("��֧�ִ�����!\n");
			//send(_sock, sendBuf, strlen(sendBuf) + 1, 0);
		}


	}

	//4. �ر�Socket
	closesocket(_sock);

	WSACleanup();
	return 0;
}