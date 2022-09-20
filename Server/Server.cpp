#define WIN32_LEAN_AND_MEAN		//Windows.h��WinSock2.h�к������ص�
#define _WINSOCK_DEPRECATED_NO_WARNINGS

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

	//2. �󶨶˿�
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(4567);
	_sin.sin_addr.S_un.S_addr = INADDR_ANY;
	if (SOCKET_ERROR == bind(_sock, (sockaddr*)&_sin, sizeof(_sin))) {
		printf("�󶨶˿ڴ���\n");
	}
	else {
		printf("�󶨶˿ڳɹ�\n");
	}

	//3. �����˿�
	if (SOCKET_ERROR == listen(_sock, 5)) {
		printf("�����˿ڴ���\n");
	}
	else {
		printf("�����˿ڳɹ�\n");
	}

	//4. �ȴ��ͻ�����
	sockaddr_in clientAddr = {};
	int nAddrLen = sizeof(sockaddr_in);
	SOCKET _cSock = INVALID_SOCKET;

	//while (true) {

	_cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
	if (INVALID_SOCKET == _cSock) {
		printf("����,���յ���Ч�ͻ���SOCKET\n");
	}
	printf("�¿ͻ��˼���,Socket=%d,IP=%s\n", (int)_cSock, inet_ntoa(clientAddr.sin_addr));





	while (true) {
		//������
		char szRecv[1024] = {};
		//5. ���տͻ�������
		int nlen = recv(_cSock, szRecv, sizeof(DataHeader), 0);
		DataHeader* header = (DataHeader*)szRecv;
		if (nlen <= 0) {
			printf("�ͻ������˳�,�������!\n");
			break;
		}
		//6. ����ͻ�������
		switch (header->cmd) {
		case CMD_LOGIN:
		{
			recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
			Login* login = (Login*)szRecv;
			printf("�յ�������:CMD_LOGIN,���ݳ���:%d,�û���:%s,����:%s\n", login->dataLength, login->username, login->password);
			//�ж��߼�
			LoginResult ret;
			ret.result = 1;
			send(_cSock, (const char*)&ret, sizeof(ret), 0);
		}
		break;
		case CMD_LOGOUT:
		{
			recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
			Logout * logout = (Logout*)szRecv;
			printf("�յ�������:CMD_LOGOUT,���ݳ���:%d,�û���:%s,����:%s\n", logout->dataLength, logout->username, logout->password);
			LogoutResult ret;
			ret.result = 1;
			send(_cSock, (const char*)&ret, sizeof(ret), 0);
		}
		break;

		}

	}

	//}

	//0. �ر�Socket
	closesocket(_sock);

	WSACleanup();
	return 0;
}
