#ifndef MESSAGE_HEADER_H
#define MESSAGE_HEADER_H

#include <string.h>

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
        memset(userPassword, 0, sizeof(userPassword));
    }
    char userName[32];
    char userPassword[32];
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

#endif // MESSAGE_HEADER_H