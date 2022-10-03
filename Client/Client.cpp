#include"Client.hpp"
#include <thread>

void cmdThread(Client* client) {
	while (true) {
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		//4. 处理请求数据
		if (0 == strcmp(cmdBuf, "exit")) {
			client->Close();
			printf("退出\n");
			return;
		}
		else if (0 == strcmp(cmdBuf, "login")) {
			Login login;
			strcpy(login.username, "xu");
			strcpy(login.password, "1234");

			client->SendData(&login);
		}
		else if (0 == strcmp(cmdBuf, "logout")) {

			Logout logout;
			strcpy(logout.username, "xu");
			strcpy(logout.password, "1234");

			client->SendData(&logout);
		}
		else
		{
			printf("不支持此命令!\n");
		}
	}
}

int main() {

	Client client;
	client.Connect("127.0.0.1", 4567);

	//启动线程
	std::thread t1(cmdThread, &client);
	t1.detach();

	while (client.IsRun()) {
		client.OnRun();
	}
	client.Close();

	return 0;
}