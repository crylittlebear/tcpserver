#ifndef EASY_TCP_CLIENT_H
#define EASY_TCP_CLIENT_H

#include <iostream>
#include <vector>

#ifdef _WIN32
// 用于解决windows.h和winsock2.h这两个头文件包含顺序的问题
#define WIN32_LEAN_AND_MEAN
// 用于解决inet_addr()这个函数报错问题
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <Windows.h>
#include <WinSock2.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#define SOCKET                  int
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)
#endif

#include "MessageHeader.hpp"

class EasyTcpClient {
public:
	EasyTcpClient() {
		m_sock = INVALID_SOCKET;
	}

	//虚析构函数
	virtual ~EasyTcpClient() {
		closeSock();
	}

	//初始化socket
	void initSocket() {
		//启动win sock 2.x环境
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);
		WSADATA data;
		WSAStartup(ver, &data);
#endif
		//防止多次调用，判断套接字是否已经有效
		if (m_sock != INVALID_SOCKET) {
			std::cout << "<m_socket="<< m_sock << ">关闭旧连接...\n";
			closeSock();
		}
		//创建套接字
		m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_sock == INVALID_SOCKET) {
			std::cerr << "创建套接字失败\n";
		}
		else {
			std::cout << "创建套接字<socket=" << m_sock << "成功\n";
		}
	}

	//连接服务器
	int connectToServer(const char* ip, unsigned short port) {
		//判断套接字是否有效
		if (m_sock == INVALID_SOCKET) {
			initSocket();
		}
		sockaddr_in addr;
		addr.sin_family = AF_INET;
#ifdef _WIN32
		addr.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		addr.sin_addr.s_addr = inet_addr(ip);
#endif
		addr.sin_port = htons(port);
		int ret = connect(m_sock, (sockaddr*)&addr, sizeof(sockaddr_in));
		if (ret == SOCKET_ERROR) {
			std::cerr << "连接服务器<ip = " << ip << ":" << port << ">失败\n";
		}
		else {
			std::cout << "连接服务器<ip = " << ip << ":" << port << ">成功\n";
		}
		return ret;
	}

	//关闭socket
	void closeSock() {
		if (m_sock != INVALID_SOCKET) {
#ifdef _WIN32
			closesocket(m_sock);
			WSACleanup();
#else
			close(m_sock);
#endif
			m_sock = INVALID_SOCKET;
		}
	}

	//获取socket
	SOCKET getSocket() const {
		return m_sock;
	}

	//处理网络消息
	bool onRun() {
		if (isRun()) {
			fd_set fdReads;
			FD_ZERO(&fdReads);
			FD_SET(m_sock, &fdReads);
			timeval t = { 0, 0 };
			int ret = select(m_sock + 1, &fdReads, NULL, NULL, &t);
			if (ret < 0) {
				std::cout << "<socket=" << m_sock << ">select任务结束\n";
				closeSock();
				return false;
			}
			if (FD_ISSET(m_sock, &fdReads)) {
				FD_CLR(m_sock, &fdReads);
				if (recvData() < 0) {
					std::cout << "<socket=" << m_sock << ">select任务结束\n";
					closeSock();
					return false;
				}
			}
			return true;
		}
		return false;
	}

	//接收数据(后续需要处理粘包、拆分包问题)
	int recvData() {
		//缓冲区
		char szRecv[4096];
		//接收客户端数据
		int nLen = (int)recv(m_sock, szRecv, sizeof(DataHead), 0);
		DataHead* head = (DataHead*)szRecv;
		if (nLen <= 0) {
			std::cout << "与服务器断开连接,任务结束\n";
			return -1;
		}
		recv(m_sock, szRecv + sizeof(DataHead), head->dataLength - sizeof(DataHead), 0);
		onNetMsg(head);
		return 0;
	}

	//响应网络消息
	void onNetMsg(DataHead* head) {
		switch (head->cmd) {
			case CMD_LOGIN_RESULT: {
				LoginResult* res = (LoginResult*)head;
				std::cout << "收到服务器的消息: CMD_LOGIN_RESULT, 数据长度: "
					<< res->dataLength << std::endl;
			}
			break;
			case CMD_LOGOUT_RESULT: {
				LogoutResult* res = (LogoutResult*)head;
				std::cout << "收到服务器的消息: CMD_LOGOUT_RESULT, 数据长度: "
					<< res->dataLength << std::endl;
			}
			break;
			case CMD_NEW_USER_JOIN: {
				NewUserJoin* userJoin = (NewUserJoin*)head;
				std::cout << "收到服务器的消息: CMD_NEW_USER_JOIN, 数据长度: "
					<< userJoin->dataLength << std::endl;
			}
			break;
			default: {
				head->cmd = CMD_ERROR;
				head->dataLength = 0;
				std::cout << "无法解析传送来的消息" << std::endl;
			}
			break;
		}
	}

	bool isRun() {
		return m_sock != INVALID_SOCKET;
	}

	//发送数据
	int sendData(DataHead* head) {
		if (isRun() && head) {
			return send(m_sock, (const char*)head, head->dataLength, 0);
		}
		return SOCKET_ERROR;
	}
private:
	SOCKET m_sock;
};

#endif // EASY_TCP_CLIENT_H