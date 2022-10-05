#include"Server.hpp"
#include <thread>

void cmdThread(Server* server) {
	while (true) {
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		//4. 处理请求数据
		if (0 == strcmp(cmdBuf, "exit")) {
			printf("退出\n");
			server->Close();
			return;
		}
	}
}

int main()
{
	Server server;
	server.initSocket();
	server.Bind(nullptr, 4567);
	server.Listen(5);

	//启动线程
	std::thread t1(cmdThread, &server);
	t1.detach();

	while (server.IsRun())
	{
		server.OnRun();
	}
	server.Close();

	return 0;
}
