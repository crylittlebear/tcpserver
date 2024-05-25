#ifdef _WIN32
// 用于解决windows.h和winsock2.h这两个头文件包含顺序的问题
#define WIN32_LEAN_AND_MEAN
// 用于解决inet_addr()这个函数报错问题
#define _WINSOCK_DEPRECATED_NO_WARNINGS
// 用于解决strcpy复制字符串安全性不高而导致的报错问题
#define _CRT_SECURE_NO_WARNINGS
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

#include <iostream>
#include <thread>

enum CMDType {
    CMD_LOGIN,
    CMD_LOGIN_RESULT,
    CMD_LOGOUT,
    CMD_LOGOUT_RESULT,
    CMD_NEW_USER_JOIN,
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

struct NewUserJoin : public DataHead {
    NewUserJoin() {
        dataLength = sizeof(NewUserJoin);
        cmd = CMD_NEW_USER_JOIN;
        sock = 0;
    }
    int sock;
};

bool g_tExit = true;

void processCMD(SOCKET sock) {
    char buf[1024];
    while (true) {
        memset(buf, 0, sizeof(buf));
        std::cin >> buf;
        if (strcmp(buf, "exit") == 0) {
            std::cout << "退出线程\n";
            g_tExit = false;
            break;
        }
        else if (strcmp(buf, "login") == 0) {
            //向服务器发送登录指令
            Login login;
            strcpy(login.userName, "Li Heng");
            strcpy(login.userPassWord, "1994218li");
            send(sock, (const char*)&login, sizeof(login), 0);
        }
        else if (strcmp(buf, "logout") == 0) {
            //向服务器发送登出指令
            Logout logout;
            strcpy(logout.userName, "Li Heng");
            send(sock, (const char*)&logout, sizeof(Logout), 0);
        }
        else {
            std::cout << "不支持的命令，请重新输入\n";
        }
        memset(buf, 0, sizeof(buf));
    }
}

int process(SOCKET sock) {
    DataHead head = { };
    int len = (int)recv(sock, (char*)&head, sizeof(DataHead), 0);
    if (len > 0) {
        switch (head.cmd) {
        case CMD_LOGIN_RESULT:
        {
            LoginResult result = { };
            recv(sock, (char*)&result + sizeof(DataHead), sizeof(LoginResult) - sizeof(DataHead), 0);
            std::cout << "收到来自服务器的登入结果信息: " << result.result << std::endl;
            break;
        }
        case CMD_LOGOUT_RESULT:
        {
            LogoutResult result = { };
            recv(sock, (char*)&result + sizeof(DataHead), sizeof(Logout) - sizeof(DataHead), 0);
            std::cout << "收到来自服务器的登出结果信息: " << result.result << std::endl;
            break;
        }
        case CMD_NEW_USER_JOIN:
        {
            NewUserJoin userJoin = { };
            recv(sock, (char*)&userJoin + sizeof(DataHead), sizeof(NewUserJoin) - sizeof(DataHead), 0);
            std::cout << "Sock: " << userJoin.sock << "成功登陆了\n";
            break;
        }
        default:
        {
            head.cmd = CMD_ERROR;
            head.dataLength = 0;
            std::cout << "无法解析传送来的消息" << std::endl;
            break;
        }
        }
    }
    else {
        std::cout << "与服务器断开连接,任务结束\n";
        return -1;
    }
    return 0;
}

int main() {
#ifdef _WIN32
    WORD ver = MAKEWORD(2, 2);
    WSADATA data;
    WSAStartup(ver, &data);
#endif
    //创建套接字
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "创建套接字失败\n";
        return -1;
    }
    else {
        std::cout << "创建套接字成功\n";
    }
    //连接服务器
    sockaddr_in addr;
    addr.sin_family = AF_INET;
#ifdef _WIN32
    addr.sin_addr.S_un.S_addr = inet_addr("192.168.0.138");
#else
    addr.sin_addr.s_addr = inet_addr("192.168.0.138");
#endif
    addr.sin_port = htons(9999);
    if (SOCKET_ERROR == connect(sock, (sockaddr*)&addr, sizeof(addr))) {
        std::cout << "连接服务器失败\n";
        return -1;
    }
    else {
        std::cout << "连接服务器成功\n";
    }

    //启动线程检测用户输入
    std::thread t1(processCMD, sock);
    t1.detach();

    while (g_tExit) {
        fd_set fdReads;
        FD_ZERO(&fdReads);
        FD_SET(sock, &fdReads);
        timeval t = { 1, 0 };
        int ret = select(sock + 1, &fdReads, NULL, NULL, &t);
        if (ret < 0) {
            std::cout << "select 任务结束\n";
            break;
        }
        if (FD_ISSET(sock, &fdReads)) {
            FD_CLR(sock, &fdReads);
            if (process(sock) < 0) {
                break;
            }
        }
    }
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    getchar();
    return 0;
}
