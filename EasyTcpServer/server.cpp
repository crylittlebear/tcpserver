// 用于解决windows.h和winsock2.h这两个头文件包含顺序的问题
#define WIN32_LEAN_AND_MEAN 
// 用于解决inet_addr()这个函数报错问题
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <Windows.h>
#include <WinSock2.h>
#include <vector>
#include <algorithm>

enum CMDType {
	CMD_LOGIN,
	CMD_LOGIN_RESULT,
	CMD_LOGOUT,
	CMD_LOGOUT_RESULT,
	CMD_ERROR
};

//数据包头
struct DataHead {
	//数据长度
	short dataLength;
	//命令
	short cmd;
};

struct Login : public DataHead {
	Login() {
		dataLength = sizeof(Login);
		cmd = CMD_LOGIN;
		memset(userName, 0, sizeof(userName));
		memset(userPassWord, 0, sizeof(userPassWord));
	}
	char userName[32];
	char userPassWord[32];
};

struct LoginResult : public DataHead {
	LoginResult() {
		dataLength = sizeof(LoginResult);
		cmd = CMD_LOGIN_RESULT;
		result = 0; // result为0表示操作成功
	}
	int result;
};

struct Logout : public DataHead {
	Logout() {
		dataLength = sizeof(Logout);
		cmd = CMD_LOGOUT;
		memset(userName, 0, sizeof(userName));
	}
	char userName[32];
};

struct LogoutResult : public DataHead {
	LogoutResult() {
		dataLength = sizeof(Logout);
		cmd = CMD_LOGOUT_RESULT;
		result = 0;
	}
	int result;
};

std::vector<SOCKET> g_clients;

int process(SOCKET sock) {
	DataHead head = { };
	int len = recv(sock, (char*)&head, sizeof(DataHead), 0);
	if (len > 0) {
		switch (head.cmd) {
			case CMD_LOGIN:
			{
				Login login = { };
				recv(sock, (char*)&login + sizeof(DataHead), sizeof(Login) - sizeof(DataHead), 0);
				std::cout << "接收到数据长度: " << head.dataLength << ", 命令："
					<< head.cmd << std::endl;
				std::cout << "账户名: " << login.userName << ", 密码: " <<
					login.userPassWord << std::endl;
				std::cout << login.userName << "登录成功\n";
				LoginResult res;
				send(sock, (const char*)&res, sizeof(LoginResult), 0);
				break;
			}
			case CMD_LOGOUT:
			{
				Logout logout = { };
				recv(sock, (char*)&logout + sizeof(DataHead), sizeof(Logout) - sizeof(DataHead), 0);
				std::cout << "接收到数据长度: " << head.dataLength << ", 命令："
					<< head.cmd << std::endl;
				std::cout << "账户名: " << logout.userName << "发来退出登录请求"
					<< std::endl;
				LogoutResult res;
				send(sock, (const char*)&res, sizeof(LogoutResult), 0);
				std::cout << logout.userName << "退出登录成功\n";
				break;
			}
			default:
			{
				head.cmd = CMD_ERROR;
				head.dataLength = 0;
				std::cout << "无法解析传送来的指令" << std::endl;
				send(sock, (const char*)&head, sizeof(DataHead), 0);
			}
		}
	}
	else {
		std::cout << "客户端退出，任务结束\n";
		return -1;
	}
}

int main() {
	WORD ver = MAKEWORD(2, 2);
	WSADATA data;
	WSAStartup(ver, &data);
	//创建套接字
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		std::cerr << "创建套接字失败!\n";
		return -1;
	}
	//绑定本地IP地址及端口号
	sockaddr_in addr = { };
	addr.sin_family = AF_INET;
	//addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	addr.sin_port = htons(9999);
	if (SOCKET_ERROR == bind(sock, (sockaddr*)&addr, sizeof(addr))) {
		std::cerr << "绑定本地IP和端口失败!\n";
		return -1;
	}
	else {
		std::cout << "绑定IP和端口成功!\n";
	}
	//监听客户端连接
	if (SOCKET_ERROR == (listen(sock, 5))) {
		std::cerr << "监听失败\n";
		return -1;
	}
	else {
		std::cout << "监听中....\n";
	}
	while (true) {
		// Socket集合
		fd_set fdRead;
		fd_set fdWrite;
		fd_set fdExp;
		//清空fd_set
		FD_ZERO(&fdRead);
		FD_ZERO(&fdWrite);
		FD_ZERO(&fdExp);
		//设置fd_set
		FD_SET(sock, &fdRead);
		FD_SET(sock, &fdWrite);
		FD_SET(sock, &fdExp);
		//把新加入的套接字放入fd_set中
		//for (std::size_t i = 0; i < g_clients.size(); ++i) {
		//	FD_SET(g_clients[i], &fdRead);
		//}
		/// 这里老师为什么选择递减而不是像我上面写的递增的原因是，如果是递增的话
		/// 每次都需要调用size函数，而递减只需要和0相比较，因而递减速度更快。
		for (int n = (int)g_clients.size() - 1; n >= 0; --n) {
			FD_SET(g_clients[n], &fdRead);
		}

		// select的第一个参数是所有文件描述符的范围而不是数量
		// 第一个参数是unix和linux中的伯克利socket，在windows下不需要设置
		timeval tm;
		tm.tv_sec = 0;
		tm.tv_usec = 200000;
		///select函数的最后一个参数如果被设置为NULL，则表示设置为阻塞状态
		///只有新的连接到来才会往下继续执行，而如果将其设置为非NULL值，则
		///变为非阻塞状态，在经过tm时间后便继续往下执行。
		int ret = select(sock + 1, &fdRead, &fdWrite, &fdExp, &tm);
		if (ret < 0) {
			std::cout << "select() error!, 任务结束\n";
			break;
		}
		if (FD_ISSET(sock, &fdRead)) {
			FD_CLR(sock, &fdRead);
			//接受来自客户端的连接
			sockaddr_in c_addr = {};
			int addr_len = sizeof(c_addr);
			SOCKET c_sock = accept(sock, (sockaddr*)&c_addr, &addr_len);
			if (c_sock == INVALID_SOCKET) {
				std::cerr << "接受客户端连接失败!\n";
				break;
			}
			else {
				std::cout << "成功接收到来自客户端的连接，IP: " << inet_ntoa(c_addr.sin_addr)
					<< ", port: " << ntohs(c_addr.sin_port) << std::endl;
			}
			g_clients.push_back(c_sock);
		}
		for (std::size_t i = 0; i < fdRead.fd_count; ++i) {
			if (process(fdRead.fd_array[i]) == -1) {
				auto iter = std::find(g_clients.begin(), g_clients.end(), fdRead.fd_array[i]);
				if (iter != g_clients.end()) {
					g_clients.erase(iter);
				}
			}
		}
	}
	// 关闭所有的套接字
	for (int i = 0; i < g_clients.size(); ++i) {
		closesocket(g_clients[i]);
	}
	closesocket(sock);
	WSACleanup();
	return 0;
}
