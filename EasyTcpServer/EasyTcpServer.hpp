#ifndef EASY_TCP_SERVER_H
#define EASY_TCP_SERVER_H

#ifdef _WIN32
#define FD_SETSIZE 12506
// 用于解决windows.h和winsock2.h这两个头文件包含顺序的问题
#define WIN32_LEAN_AND_MEAN 
// 用于解决inet_addr()这个函数报错问题
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <Windows.h>
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
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
#include <string.h>
#include "MessageHeader.hpp"

#ifndef RECV_BUF_SIZE
#define RECV_BUF_SIZE 102400
#endif

class ClientSocket {
public:
    ClientSocket(SOCKET sock = INVALID_SOCKET) :
        m_szMsgBuf(new char[RECV_BUF_SIZE * 10]) {
        m_sock = sock;
        memset(m_szMsgBuf, 0, RECV_BUF_SIZE * 10);
        m_lastPos = 0;
    }

    ~ClientSocket() {
        if (m_szMsgBuf) {
            delete[] m_szMsgBuf;
            m_szMsgBuf = nullptr;
        }
    }

    SOCKET sockfd() {
        return m_sock;
    }

    char* msgBuf() {
        return m_szMsgBuf;
    }

    int getLastPos() {
        return m_lastPos;
    }

    void setLastPos(int pos) {
        m_lastPos = pos;
    }
private:
    //文件描述符fd
    SOCKET m_sock;
    //第二缓冲区（消息缓冲区）
    char *m_szMsgBuf;
    //消息缓冲区尾部位置
    int m_lastPos;
};

class EasyTcpServer {
public:
    EasyTcpServer() {
        m_listenSock = INVALID_SOCKET;
        _recvCount = 0;
    }

    virtual ~EasyTcpServer() {
        Close();
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
#ifdef _WIN32
            closesocket(m_listenSock);
#else
            close(m_listenSock);
#endif
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
        sockaddr_in addr = { };
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
        }
        else {
            std::cout << "绑定端口<Port = " << port << ">成功！\n";
        }
        return ret;
    }

    // 监听端口号
    int Listen(int n) {
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
        SOCKET c_sock = accept(m_listenSock, (sockaddr*)&c_addr, (socklen_t*)&addr_len);
#endif
        if (c_sock == INVALID_SOCKET) {
            std::cerr << "套接字<socket = " << m_listenSock << ">接收客户端连接失败\n";
        }
        else {
            //NewUserJoin userJoin = {};
            //userJoin.sock = c_sock;
            //sendDataToAll(&userJoin);
            m_clients.push_back(new ClientSocket(c_sock));
            //std::cout << "客户端Ip: " << inet_ntoa(c_addr.sin_addr) << ", Port: "
            //    << ntohs(c_addr.sin_port) << ", 套接字<socket = " << c_sock
            //    << ">, 成功与服务器建立连接" << std::endl;
        }
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
            for (int i = (int)m_clients.size() - 1; i >= 0; --i) {
                FD_SET(m_clients[i]->sockfd(), &fdRead);
                if (m_clients[i]->sockfd() > maxSock) {
                    maxSock = m_clients[i]->sockfd();
                }
            }
            ///select函数的最后一个参数如果被设置为NULL，则表示设置为阻塞状态
            ///只有新的连接到来才会往下继续执行，而如果将其设置为非NULL值，则
            ///变为非阻塞状态，在经过tm时间后便继续往下执行。
            timeval tm = {1 , 0};
            int ret = select(maxSock + 1, &fdRead, &fdWrite, &fdExp, &tm);
            if (ret < 0) {
                std::cout << "select任务结束。\n";
                Close();
                return false;
            }
            if (FD_ISSET(m_listenSock, &fdRead)) {
                FD_CLR(m_listenSock, &fdRead);
                Accept();
            }
            int index = 0;
            for (std::size_t i = 0; i < m_clients.size(); ++i) {
                if (FD_ISSET(m_clients[i]->sockfd(), &fdRead)) {
                    FD_CLR(m_clients[i]->sockfd(), &fdRead);
                    if (recvData(m_clients[i]) == -1) {
                        // std::cout << "<socket = " << m_clients[i] << ">出现通信错误"
                        //     << ", 关闭该套接字\n";
#ifdef _WIN32
                        closesocket(m_clients[i]->sockfd());
                        delete m_clients[i];
#else
                        close(m_clients[i]->sockfd());
                        delete m_clients[i];
#endif
                        continue;
                    }
                }
                m_clients[index++] = m_clients[i];
            }
            m_clients.resize(index);
            return true;
        }
        return false;
    }

    int recvData(ClientSocket* cSock) {
        int recvLen = recv(cSock->sockfd(), szRecv, RECV_BUF_SIZE, 0);
        if (recvLen <= 0) {
            std::cerr << "<socket = " << cSock->sockfd() << ">与服务器断开链接, 任务结束\n";
            return -1;
        }
        //将第一缓冲区中的数据拷贝到第二消息缓冲区
        memcpy(cSock->msgBuf() + cSock->getLastPos(), szRecv, recvLen);
        //更新消息缓冲区末尾位置
        cSock->setLastPos(cSock->getLastPos() + recvLen);
        // 当消息缓冲区的大小大于数据大小
        while (cSock->getLastPos() >= sizeof(DataHead)) {
            DataHead* head = (DataHead*)(cSock->msgBuf());
            //消息缓冲区中的数据大于一个完整消息的长度
            if (cSock->getLastPos() >= head->dataLength) {
                //由于后续的memcpy会覆盖掉前面消息的内容，因此需要记录
                //前面消息的长度以便计算取出消息后缓冲区中剩余数据大小
                int nSize = cSock->getLastPos() - head->dataLength;
                //处理取出的消息
                onNetMsg(head, cSock->sockfd());
                //将后续的消息移动到消息缓冲区的起始位置
                memcpy(cSock->msgBuf(), cSock->msgBuf() + head->dataLength, nSize);
                cSock->setLastPos(nSize);
            }
            else {
                break;
            }
        }
        return 0;
    }

    virtual void onNetMsg(DataHead* head, SOCKET sock) {
        switch (head->cmd) {
        case CMD_LOGIN: {
            Login* login = (Login*)head;
            // std::cout << "收到客户端<Socket=" << sock << ">请求:"
            //     << "CMD_LOGIN, 数据长度: " << login->dataLength
            //     << ", userName = " << login->userName << ", password = "
            //     << login->userPassword << std::endl;
            //LoginResult res;
            //sendData(sock, &res);
        }
                      break;
        case CMD_LOGOUT: {
            Logout* logout = (Logout*)head;
            // std::cout << "收到客户端<Socket=" << sock << ">请求: "
            //     << "CMD_LOGOUT, 数据长度: " << logout->dataLength
            //     << ", userName = " << logout->userName << std::endl;  
            LogoutResult res;
            sendData(sock, &res);
        }
                       break;
        default: {
            std::cout << "收到未定义的消息, 数据长度为: " << head->dataLength << "\n";
            // DataHead* res = (DataHead*)szSend;
        }
               break;
        }
    }

    int sendData(SOCKET sock, DataHead* head) {
        if (isRun() && head) {
            return send(sock, (const char*)head, head->dataLength, 0);
        }
        return SOCKET_ERROR;
    }

    void sendDataToAll(DataHead* head) {
        for (int i = (int)m_clients.size() - 1; i >= 0; --i) {
            sendData(m_clients[i]->sockfd(), head);
        }
    }

    bool isRun() {
        return m_listenSock != INVALID_SOCKET;
    }

    void Close() {
        if (m_listenSock != INVALID_SOCKET) {
#ifdef _WIN32
            for (int i = (int)m_clients.size() - 1; i >= 0; --i) {
                closesocket(m_clients[i]->sockfd());
                delete m_clients[i];
            }
            closesocket(m_listenSock);
            WSACleanup();
#else
            for (int i = (int)m_clients.size() - 1; i >= 0; --i) {
                close(m_clients[i]->sockfd());
                delete m_clients[i];
            }
            close(m_listenSock);
#endif
        }
        m_clients.clear();
    }
private:
    SOCKET m_listenSock;
    std::vector<ClientSocket*> m_clients;
    //接收缓冲区
    char* szRecv = new char[RECV_BUF_SIZE];
    int _recvCount;
};


#endif // EASY_TCP_SERVER_H