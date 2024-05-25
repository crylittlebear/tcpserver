#ifdef _WIN32
// 用于解决strcpy复制字符串安全性不高而导致的报错问题
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "EasyTcpClient.hpp"
// #include <thread>

bool g_bRun = true;

//发送指令消息
void processCMD(EasyTcpClient* client) {
	char buf[1024];
	while (true) {
		std::cin >> buf;
		if (strcmp(buf, "exit") == 0) {
			g_bRun = false;
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
	}
}

int main() {
	const int clientCount = 10000;
	EasyTcpClient client[clientCount];
	for (int i = 0; i < clientCount; ++i) {
		if (!g_bRun) {
			return 0;
		}
		client[i].connectToServer("127.0.0.1", 9999);
		std::cout << "Connect = " << i << std::endl;
	}

	Login login = {};
	strcpy(login.userName, "Li heng");
	strcpy(login.userPassword, "1994218li");
	while (g_bRun) {
		for (int i = 0; i < clientCount; ++i) {
			client[i].sendData(&login);
			//client[i].onRun();
		}
	}
	for (int i = 0; i < clientCount; ++i) {
		client[i].closeSock();
	}
	// client2.closeSock();
	getchar();
	return 0;
}
