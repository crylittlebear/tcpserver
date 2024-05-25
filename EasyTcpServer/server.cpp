#include <iostream>
#include <vector>
#include <algorithm>

#ifdef _WIN32
    // 用于解决windows.h和winsock2.h这两个头文件包含顺序的问题
    #define WIN32_LEAN_AND_MEAN 
    // 用于解决inet_addr()这个函数报错问题
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #define socklen_t int
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


enum CMDType {
    CMD_LOGIN,
    CMD_LOGIN_RESULT,
    CMD_LOGOUT,
    CMD_LOGOUT_RESULT,
    CMD_NEW_USER_JOIN,
    CMD_ERROR,
};

struct DataHead {
    short dataLength;
    short cmd;
};

struct Login : public DataHead {
    Login() {
        dataLength = sizeof(Login);
        cmd = CMD_LOGIN;
    }
    char userName[32];
    char userPassword[32];
};

struct LoginResult : public DataHead {
    LoginResult() {
        dataLength = sizeof(LoginResult);
        cmd = CMD_LOGIN_RESULT;
        result = 0;
    }
    int result;
};

struct Logout : public DataHead {
    Logout() {
        dataLength = sizeof(Logout);
        cmd = CMD_LOGOUT;
    }
    char userName[32];
};

struct LogoutResult : public DataHead {
    LogoutResult() {
        dataLength = sizeof(LogoutResult);
        cmd = CMD_LOGOUT_RESULT;
    }
    int result;
};

struct NewUserJoin : public DataHead {
    NewUserJoin() {
        dataLength = sizeof(NewUserJoin);
        cmd = CMD_NEW_USER_JOIN;
        sock = 0;
    }
    int sock;
};

std::vector<int> g_sockVec;

int process(int sock) {
    DataHead head = {};
    int recvLen = recv(sock, (char*)&head, sizeof(DataHead), 0);
    if (recvLen <= 0) {
        std::cout << "客户端退出, 任务结束\n";
        return -1;
    }
    else {
        switch (head.cmd) {
            case CMD_LOGIN:
            {
                Login login = { };
                recv(sock, (char*)&login + sizeof(head),
                    sizeof(Login) - sizeof(DataHead), 0);
                std::cout << "收到客户端<Socket=" << sock << ">请求:"
                    << "CMD_LOGIN, 数据长度: " << login.dataLength
                    << ", userName = " << login.userName << ", password = "
                    << login.userPassword << std::endl;
                LoginResult result;
                result.result = 0;
                send(sock, (const char*)&result, sizeof(result), 0);
            }
            break;
            case CMD_LOGOUT:
            {
                Logout logout = { };
                recv(sock, (char*)&logout + sizeof(DataHead),
                    sizeof(Logout) - sizeof(DataHead), 0);
                std::cout << "收到客户端<Socket=" << sock << ">请求: "
                    << "CMD_LOGOUT, 数据长度: " << logout.dataLength
                    << ", userName = " << logout.userName << std::endl;
                LogoutResult result;
                result.result = 0;
                send(sock, (const char*)&result, sizeof(result), 0);
            }
            break;
            default:
            {
                head.cmd = CMD_ERROR;
                head.dataLength = 0;
                std::cout << "无法解析的命令" << std::endl;
                send(sock, (const char*)&head, sizeof(DataHead), 0);
            }
            break;
        }
    }
    return 0;
}

int main() {
    //创建套接字
#ifdef _WIN32
    WORD ver = MAKEWORD(2, 2);
    WSADATA data;
    WSAStartup(ver, &data);
#endif
    //创建套接字
    SOCKET s_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_sock == INVALID_SOCKET) {
        std::cerr << "创建套接字失败!\n";
        return -1;
    }
    else {
        std::cout << "套接字创建成功\n";
    }
    //绑定本地IP和端口
    sockaddr_in addr = { };
    addr.sin_family = AF_INET;
#ifdef _WIN32
    addr.sin_addr.S_un.S_addr = INADDR_ANY;
#else
    addr.sin_addr.s_addr = INADDR_ANY;
#endif
    addr.sin_port = htons(9999);
    if (bind(s_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "绑定本地IP和端口失败!\n";
        return -1;
    }
    else {
        std::cout << "绑定IP和端口成功!\n";
    }
    //监听客户端连接
    if (listen(s_sock, 5) == SOCKET_ERROR) {
        std::cerr << "监听失败\n";
        return -1;
    }
    else {
        std::cout << "监听中....\n";
    }
    while (true) {
        fd_set fdReads;
        fd_set fdWrites;
        fd_set fdExps;
        FD_ZERO(&fdReads);
        FD_ZERO(&fdWrites);
        FD_ZERO(&fdExps);
        FD_SET(s_sock, &fdReads);
        FD_SET(s_sock, &fdWrites);
        FD_SET(s_sock, &fdExps);
        SOCKET maxSock = s_sock;
        /// 这里老师为什么选择递减而不是像我上面写的递增的原因是，如果是递增的话
        /// 每次都需要调用size函数，而递减只需要和0相比较，因而递减速度更快。
        for (int i = (int)g_sockVec.size() - 1; i >= 0; --i) {
            FD_SET(g_sockVec[i], &fdReads);
            if (g_sockVec[i] > maxSock) {
                maxSock = g_sockVec[i];
            }
        }
        timeval t = { 0, 0 };
        /// select的第一个参数是所有文件描述符的范围而不是数量
        /// 第一个参数是unix和linux中的伯克利socket，在windows下不需要设置
        ///select函数的最后一个参数如果被设置为NULL，则表示设置为阻塞状态
        ///只有新的连接到来才会往下继续执行，而如果将其设置为非NULL值，则
        ///变为非阻塞状态，在经过tm时间后便继续往下执行。
        select(maxSock + 1, &fdReads, &fdWrites, &fdExps, &t);
        if (FD_ISSET(s_sock, &fdReads)) {
            FD_CLR(s_sock, &fdReads);
            sockaddr_in c_addr;
            socklen_t addr_len = sizeof(sockaddr_in);
            int c_sock = accept(s_sock, (sockaddr*)&c_addr, &addr_len);
            if (c_sock == INVALID_SOCKET) {
                std::cerr << "接受客户端连接失败\n";
                break;
            }
            else {
                std::cout << "客户端IP: " << inet_ntoa(c_addr.sin_addr)
                    << ", PORT: " << ntohs(c_addr.sin_port)
                    << ", 套接字: " << c_sock
                    << ", 成功与服务器连接" << std::endl;
                //在将自己加入vector中之前将加入的信息传送给所有客户端
                for (int i = g_sockVec.size() - 1; i >= 0; --i) {
                    NewUserJoin userJoin;
                    userJoin.sock = c_sock;
                    send(g_sockVec[i], (const char*)&userJoin, sizeof(NewUserJoin), 0);
                }
                g_sockVec.push_back(c_sock);
            }
        }
        int index = 0;
         //std::cout << "查询是否有命令之前：\n";
         //for (auto c : g_sockVec) {
         //    std::cout << c << " ";
         //}
         //std::cout << std::endl;
        for (size_t i = 0; i < g_sockVec.size(); ++i) {
            if (FD_ISSET(g_sockVec[i], &fdReads)) {
                FD_CLR(g_sockVec[i], &fdReads);
                if (process(g_sockVec[i]) == -1) {
                    std::cout << "套接字" << g_sockVec[i] << "出现通信错误"
                        << ", 关闭该套接字\n";
                #ifdef _WIN32
                    closesocket(g_sockVec[i]);
                #else
                    close(g_sockVec[i]);
                #endif
                    continue;
                }
            }
            g_sockVec[index++] = g_sockVec[i];
        }
        g_sockVec.resize(index);
         //std::cout << "查询是否有命令之后：\n";
         //for (auto c : g_sockVec) {
         //    std::cout << c << " ";
         //}
         //std::cout << std::endl;
    }
    //关闭套接字
    for (int i = (int)g_sockVec.size() - 1; i >= 0; --i) {
#ifdef _WIN32
        closesocket(g_sockVec[i]);
#else
        close(g_sockVec[i]);
#endif
    }
#ifdef _WIN32
    closesocket(s_sock);
#else
    close(s_sock);
#endif
    return 0;
}