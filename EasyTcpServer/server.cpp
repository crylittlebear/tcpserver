#include "EasyTcpServer.hpp"

int main() {
    EasyTcpServer server1;
    // EasyTcpServer server2;
    server1.initSocket();
    // server2.initSocket();
    server1.Bind(nullptr, 9999);
    // server2.Bind(nullptr, 9999);
    server1.Listen(100);
    // server2.Listen(5);
    while (server1.isRun()) {
        server1.onRun();
        // server2.onRun();
    }
    //关闭套接字
    server1.Close();
    // server2.closeAllSocket();
    return 0;
}