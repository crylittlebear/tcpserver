#ifdef _WIN32
// 用于解决strcpy复制字符串安全性不高而导致的报错问题
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "EasyTcpClient.hpp"
#include <thread>

//发送指令消息
void processCMD(EasyTcpClient* client) {
	char buf[1024];
	while (true) {
		memset(buf, 0, sizeof(buf));
		std::cin >> buf;
		if (strcmp(buf, "exit") == 0) {
			client->closeSock();
			std::cout << "退出cmdThread线程\n";
			break;
		}
		else if (strcmp(buf, "login") == 0) {
			//向服务器发送登录指令
			Login login;
			strcpy(login.userName, "Li Heng");
			strcpy(login.userPassword, "1994218li");
			client->sendData(&login);
		}
		else if (strcmp(buf, "logout") == 0) {
			//向服务器发送登出指令
			Logout logout;
			strcpy(logout.userName, "Li Heng");
			client->sendData(&logout);
		}
		else {
			std::cout << "不支持的命令，请重新输入\n";
		}
		memset(buf, 0, sizeof(buf));
	}
}

int main() {
    EasyTcpClient client1;
	EasyTcpClient client2;
	EasyTcpClient client3;
	//client.initSocket();
	client1.connectToServer("192.168.0.102", 9999);
	client2.connectToServer("192.168.126.145", 9999);
	client3.connectToServer("192.168.0.138", 9999);
    //启动线程检测用户输入
    std::thread t1(processCMD, &client1);
    t1.detach();
	std::thread t2(processCMD, &client2);
	t2.detach();
	std::thread t3(processCMD, &client3);
	t3.detach();
    while (client1.isRun() || client2.isRun() || client3.isRun()) {
        client1.onRun();
		client2.onRun();
		client3.onRun();
    }
	client1.closeSock();
	client2.closeSock();
	client3.closeSock();
    getchar();
    return 0;
}
