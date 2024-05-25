#include "EasyTcpServer.hpp"

int main() {
    EasyTcpServer server1;
    server1.initSocket();
    server1.Bind(nullptr, 9997);
    server1.Listen(5);

    EasyTcpServer server2;
    server2.initSocket();
    server2.Bind(nullptr, 9998);
    server2.Listen(5);

    EasyTcpServer server3;
    server3.initSocket();
    server3.Bind(nullptr, 9999);
    server3.Listen(5);

    while (server1.isRun() || server2.isRun() || server3.isRun()) {
        server1.onRun();
        server2.onRun();
        server3.onRun();
    }
    //关闭套接字
    server1.closeAllSocket();
    server2.closeAllSocket();
    server3.closeAllSocket();
    return 0;
}