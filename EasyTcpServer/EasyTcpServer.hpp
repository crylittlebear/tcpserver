#ifndef EASY_TCP_SERVER_H
#define EASY_TCP_SERVER_H

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

#include <vector>
#include <iostream>
#include "MessageHeader.hpp"

class EasyTcpServer {
public:
    EasyTcpServer() {
        m_listenSock = INVALID_SOCKET;
    }

    virtual ~EasyTcpServer() {
        closeAllSocket();
    }

    //初始化监听套接字
    void initSocket() {
#ifdef _WIN32
        WORD ver = MAKEWORD(2, 2);
        WSADATA data;
        WSAStartup(ver, &data);
#endif
        //防止多次调用初始化套接字
        if (m_listenSock != INVALID_SOCKET) {
            std::cout << "<m_socket=" << m_listenSock << ">关闭旧连接...\n";
            closeSock(m_listenSock);
        }
        m_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSock == INVALID_SOCKET) {
            std::cerr << "创建套接字失败\n";
        }
        else {
            std::cout << "创建套接字<socket=" << m_listenSock << "成功\n";
        }
    }

    //绑定IP地址和端口号
    int Bind(const char* ip, unsigned short port) {
        if (m_listenSock == INVALID_SOCKET) {
            initSocket();
        }
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
#ifdef _WIN32
        if (ip) {
            addr.sin_addr.S_un.S_addr = inet_addr(ip);
        }
        else {
            addr.sin_addr.S_un.S_addr = INADDR_ANY;
        }
#else
        if (ip) {
            addr.sin_addr.s_addr = inet_addr(ip);
        }
        else {
            addr.sin_addr.s_addr = INADDR_ANY;
        }
#endif        
        int ret = bind(m_listenSock, (sockaddr*)&addr, sizeof(addr));
        if (ret == SOCKET_ERROR) {
            std::cerr << "绑定端口<Port = " << port << ">失败!\n";
            return -1;
        }
        else {
            std::cout << "绑定端口<Port = " << port << ">成功！\n";
        }
        return 0;
    }

    // 监听端口号
    int Listen(int n) {
        if (m_listenSock == INVALID_SOCKET) {
            initSocket();
        }
        int ret = listen(m_listenSock, n);
        if (ret == SOCKET_ERROR) {
            std::cerr << "<socket = " << m_listenSock << ">监听网络端口失败\n";
        }
        else {
            std::cout << "服务器通过套接字<sock =" << m_listenSock << ">监听中...\n";
        }
        return ret;
    }

    // 接受客户端连接
    SOCKET Accept() {
        sockaddr_in c_addr = {};
        int addr_len = sizeof(c_addr);
#ifdef _WIN32
        SOCKET c_sock = accept(m_listenSock, (sockaddr*)&c_addr, &addr_len);
#else
        SOCKET c_sock = accept(m_listenSock, (sockaddr*)&c_addr, (socklen_t*) & addr_len);
#endif
        if (c_sock == INVALID_SOCKET) {
            std::cerr << "套接字<socket = " << m_listenSock << ">接收客户端连接失败\n";
            return INVALID_SOCKET;
        }
        std::cout << "客户端Ip: " << inet_ntoa(c_addr.sin_addr) << ", Port: "
            << ntohs(c_addr.sin_port) << ", 套接字<socket = " << c_sock
            << ">, 成功与服务器建立连接" << std::endl;
        NewUserJoin userJoin = {};
        userJoin.sock = c_sock;
        sendDataToAll(&userJoin);
        return c_sock;
    }

    bool onRun() {
        if (isRun()) {
            //新建读、写、异常描述符集合
            fd_set fdRead;
            fd_set fdWrite;
            fd_set fdExp;
            //清空读、写、异常描述符集合
            FD_ZERO(&fdRead);
            FD_ZERO(&fdWrite);
            FD_ZERO(&fdExp);
            //将监听套接字放入集合中
            FD_SET(m_listenSock, &fdRead);
            FD_SET(m_listenSock, &fdWrite);
            FD_SET(m_listenSock, &fdExp);
            //从所有需要处理的套接字从找出最大描述符
            SOCKET maxSock = m_listenSock;
            for (int i = (int)m_comSockVec.size() - 1; i >= 0; --i) {
                FD_SET(m_comSockVec[i], &fdRead);
                if (m_comSockVec[i] > maxSock) {
                    maxSock = m_comSockVec[i];
                }
            }
            ///select函数的最后一个参数如果被设置为NULL，则表示设置为阻塞状态
            ///只有新的连接到来才会往下继续执行，而如果将其设置为非NULL值，则
            ///变为非阻塞状态，在经过tm时间后便继续往下执行。
            timeval tm = { 1 , 0 };
            int ret = select(maxSock + 1, &fdRead, &fdWrite, &fdExp, &tm);
            if (ret < 0) {
                std::cout << "select任务结束。\n";
                closeAllSocket();
                return false;
            }
            if (FD_ISSET(m_listenSock, &fdRead)) {
                FD_CLR(m_listenSock, &fdRead);
                SOCKET sock = Accept();
                m_comSockVec.push_back(sock);
            }
            int index = 0;
            for (std::size_t i = 0; i < m_comSockVec.size(); ++i) {
                if (FD_ISSET(m_comSockVec[i], &fdRead)) {
                    FD_CLR(m_comSockVec[i], &fdRead);
                    if (recvData(m_comSockVec[i]) == -1) {
                        // std::cout << "<socket = " << m_comSockVec[i] << ">出现通信错误"
                        //     << ", 关闭该套接字\n";
#ifdef _WIN32
                        closesocket(m_comSockVec[i]);
#else
                        close(m_comSockVec[i]);
#endif
                        continue;
                    }
                }
                m_comSockVec[index++] = m_comSockVec[i];
            }
            m_comSockVec.resize(index);
            return true;
        }
        return false;
    }

    int recvData(SOCKET sock) {
        char szRecv[4096];
        int recvLen = recv(sock, szRecv, sizeof(DataHead), 0);
        DataHead* head = (DataHead*)szRecv;
        if (recvLen <= 0) {
            std::cerr << "<socket = " << sock << ">与服务器断开链接, 任务结束\n";
            closeSock(sock);
            return -1;
        }
        recv(sock, szRecv + sizeof(DataHead), head->dataLength - sizeof(DataHead), 0);
        onNetMsg(head, sock);
        return 0;
    }

    virtual void onNetMsg(DataHead* head, SOCKET sock) {
        char szSend[4069];
        switch (head->cmd) {
        case CMD_LOGIN: {
            Login* login = (Login*)head;
            std::cout << "收到客户端<Socket=" << sock << ">请求:"
                << "CMD_LOGIN, 数据长度: " << login->dataLength
                << ", userName = " << login->userName << ", password = "
                << login->userPassword << std::endl;
            LoginResult* res = (LoginResult*)szSend;
            res->cmd = CMD_LOGIN_RESULT;
            res->dataLength = sizeof(LoginResult);
            res->result = 0;
        }
                      break;
        case CMD_LOGOUT: {
            Logout* logout = (Logout*)head;
            std::cout << "收到客户端<Socket=" << sock << ">请求: "
                << "CMD_LOGOUT, 数据长度: " << logout->dataLength
                << ", userName = " << logout->userName << std::endl;
            LogoutResult* res = (LogoutResult*)szSend;
            res->cmd = CMD_LOGOUT_RESULT;
            res->dataLength = sizeof(LogoutResult);
            res->result = 0;
        }
                       break;
        default: {
            std::cout << "无法解析传输来的数据\n";
            DataHead* res = (DataHead*)szSend;
            res->cmd = CMD_ERROR;
            res->dataLength = sizeof(DataHead);
        }
               break;
        }
        sendData(sock, (DataHead*)szSend);
    }

    int sendData(SOCKET sock, DataHead* head) {
        if (isRun() && head) {
            return send(sock, (const char*)head, head->dataLength, 0);
        }
        return SOCKET_ERROR;
    }

    void sendDataToAll(DataHead* head) {
        for (int i = (int)m_comSockVec.size() - 1; i >= 0; --i) {
            sendData(m_comSockVec[i], head);
        }
    }

    bool isRun() {
        if (m_listenSock != INVALID_SOCKET) {
            return true;
        }
        for (auto i : m_comSockVec) {
            if (i != INVALID_SOCKET) {
                return true;
            }
        }
        return false;
    }

    void closeSock(SOCKET sock) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }

    void closeAllSocket() {
        if (m_listenSock != INVALID_SOCKET) {
            closeSock(m_listenSock);
        }
        for (auto i : m_comSockVec) {
            if (i != INVALID_SOCKET) {
                closeSock(i);
            }
        }
    }
private:
    SOCKET m_listenSock;
    std::vector<SOCKET> m_comSockVec;
};


#endif // EASY_TCP_SERVER_H