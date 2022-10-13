#include"Client.hpp"
#include <thread>

void cmdThread() {
	while (true) {
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		//4. 处理请求数据
		if (0 == strcmp(cmdBuf, "exit")) {
			printf("退出\n");
			break;
		}
		else
		{
			printf("不支持此命令!\n");
		}
	}
}

//一个线程的客户端数量
const int cCnt = 1000;
//线程数量
const int tCnt = 4;

//运行标志
bool isRun = true;




void sendThread(int id) {
	Client* clients[cCnt];
	for (int n = 0; n < cCnt; n++) {
		clients[n] = new Client();
		clients[n]->Connect("127.0.0.1", 4567);// 172.18.128.1  172.18.143.19      127.0.0.1  192.168.137.10
	}

	std::chrono::microseconds t(3000);
	std::this_thread::sleep_for(t);

	//定义测试数据
	Login test[10];

	const int nLen = sizeof(test);
	//发送测试数据
	while (isRun) {
		for (int n = 0; n < cCnt; n++) {
			//printf("线程id=<%d>", id);
			clients[n]->SendData(test, nLen);
		}
	}
	for (int i = 0; i < cCnt; i++)
	{
		clients[i]->Close();
	}
}

int main() {

	//启动UI线程
	std::thread t1(cmdThread);
	t1.detach();

	//启动发送线程
	for (int i = 0; i < tCnt; i++)
	{
		std::thread t(sendThread, i + 1);
		t.detach();
	}

	while (isRun) {
		//Sleep(100);
	}





	return 0;
}