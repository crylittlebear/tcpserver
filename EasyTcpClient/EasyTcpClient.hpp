#ifndef EASY_TCP_CLIENT_H
#define EASY_TCP_CLIENT_H

//接收缓冲区单元大小
#ifndef RECV_BUF_SIZE
#define RECV_BUF_SIZE 102400
#endif
#include <iostream>
#include "MessageHeader.hpp"
// #include <vector>

#ifdef _WIN32
// 用于解决windows.h和winsock2.h这两个头文件包含顺序的问题
#define WIN32_LEAN_AND_MEAN
// 用于解决inet_addr()这个函数报错问题
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define FD_SETSIZE      12506
#pragma comment(lib, "ws2_32.lib")
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



class EasyTcpClient {
public:
	EasyTcpClient() {
		m_sock = INVALID_SOCKET;
	}

	//虚析构函数
	virtual ~EasyTcpClient() {
		if (szRecv) {
			delete[] szRecv;
			szRecv = nullptr;
		}
		if (szMsgBuf) {
			delete[] szMsgBuf;
			szMsgBuf = nullptr;
		}
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
			std::cout << "<m_socket=" << m_sock << ">关闭旧连接...\n";
			closeSock();
		}
		//创建套接字
		m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_sock == INVALID_SOCKET) {
			std::cerr << "创建套接字失败\n";
		}
		//else {
		//	std::cout << "创建套接字<Socket=" << m_sock << ">成功\n";
		//}
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
			std::cerr << "<Socket=" << m_sock << ">连接服务器<IP=" << ip << ", Port=" << port << ">失败\n";
		}
		//else {
		//	std::cerr << "<Socket=" << m_sock << ">连接服务器<IP=" << ip << ", Port=" << port << ">成功\n";
		//}
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

	//处理网络消息
	bool onRun() {
		if (isRun()) {
			fd_set fdReads;
			FD_ZERO(&fdReads);
			FD_SET(m_sock, &fdReads);
			timeval t = { 0, 0 };
			int ret = select(static_cast<int>(m_sock) + 1, &fdReads, NULL, NULL, &t);
			if (ret < 0) {
				std::cout << "<socket=" << m_sock << ">select任务结束\n";
				closeSock();
				return false;
			}
			if (FD_ISSET(m_sock, &fdReads)) {
				FD_CLR(m_sock, &fdReads);
				if (recvData(m_sock) == -1) {
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
	int recvData(SOCKET cSock) {
		//接收客户端数据
		int nLen = (int)recv(cSock, szRecv, RECV_BUF_SIZE, 0);
		if (nLen <= 0) {
			std::cout << "<Socket=" << cSock << ">与服务器断开连接,任务结束\n";
			return -1;
		}
		//将收到的数据拷贝到消息缓冲区
		memcpy(szMsgBuf + lastPos, szRecv, nLen);
		//消息缓冲区尾部后移
		lastPos += nLen;
		//如果消息缓冲区的大小大于消息头大小，循环处理消息缓冲区
		while (lastPos >= sizeof(DataHead)) {
			DataHead* head = (DataHead*)szMsgBuf;
			//如果消息缓冲区的大小大于头部包含的数据大小
			if (lastPos >= head->dataLength) {
				//保存未处理的消息长度
				int nSize = lastPos - head->dataLength;
				//处理网络消息
				onNetMsg(head);
				//将未处理的数据消息前移动
				memcpy(szMsgBuf, szMsgBuf + head->dataLength, nSize);
				// 因为memcpy拷贝的数据已经覆盖了前面的消息值，因此使用head->dataLength是不一定能得出
				// 正确的消息长度的，因此我们需要再处理消息之前就保存剩余未处理的消息长度。
				lastPos = nSize;
			}
			else {
				//剩余的数据不够一个完成的消息
				break;
			}
		}
		return 0;
	}

	//响应网络消息
	virtual void onNetMsg(DataHead* head) {
		switch (head->cmd) {
		case CMD_LOGIN_RESULT: {
			LoginResult* res = (LoginResult*)head;
			// std::cout << "<Socket=" <<  m_sock << ">收到服务器的消息: CMD_LOGIN_RESULT, 数据长度: " << res->dataLength << std::endl;
		}
							 break;
		case CMD_LOGOUT_RESULT: {
			LogoutResult* res = (LogoutResult*)head;
			// std::cout << "<Socket=" <<  m_sock << ">收到服务器的消息: CMD_LOGOUT_RESULT, 数据长度: " << res->dataLength << std::endl;
		}
							  break;
		case CMD_NEW_USER_JOIN: {
			NewUserJoin* userJoin = (NewUserJoin*)head;
			// std::cout << "<Socket=" <<  m_sock << ">收到服务器的消息: CMD_NEW_USER_JOIN, 数据长度: " << userJoin->dataLength << std::endl;
		}
							  break;
		case CMD_ERROR: {
			std::cout << "<socket = " << m_sock << ">收到服务端消息: CMD_ERROR, 数据长度: " << head->dataLength << std::endl;
		}
					  break;
		default: {
			std::cout << "<Socket=" << m_sock << ">收到未定义消息, 数据长度：" << head->dataLength << std::endl;
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
	//接受缓冲区
	char* szRecv = new char[RECV_BUF_SIZE];
	//第二缓冲区，消息缓冲区
	char* szMsgBuf = new char[RECV_BUF_SIZE * 10];
	//消息缓冲区数据尾部位置
	int lastPos = 0;
};

#endif // EASY_TCP_CLIENT_H