#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

void send_receive(int sock, string message) {
    // 将消息转换为字节数组并发送到服务器
    send(sock, message.c_str(), message.size(), 0);
    // 接收服务器的响应
    char buffer[1024] = {0};
    recv(sock, buffer, 1024, 0);

    // 打印响应结果
    cout << buffer << endl;
}

int main() {
    // 获取目标 IP 地址和端口号
    string host;
    int port;
    cout << "请输入目标IP地址：";
    cin >> host;
    cout << "请输入目标端口号：";
    cin >> port;

    // 创建套接字并连接到服务器
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
   server_address.sin_addr.s_addr = inet_addr(host.c_str());

    if (connect(sock, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cout << "连接服务器失败" << endl;
        return -1;
    }

    // 循环处理用户输入
    while (true) {
        // 获取用户输入
        string command;
        cout << "请输入命令（set/del/get）：";
        cin >> command;

        if (command == "set") {
            // 构造 SET 命令消息
            string message = "*4\r\n$3\r\nSET\r\n$7\r\nCS06142\r\n$5\r\nCloud\r\n$9\r\nComputing\r\n";
            send_receive(sock, message);
        } else if (command == "del") {
            // 构造 DEL 命令消息
            string message = "*3\r\n$3\r\nDEL\r\n$7\r\nCS06142\r\n$5\r\nCS162\r\n";
            send_receive(sock, message);
        } else if (command == "get") {
            // 构造 GET 命令消息
            string message = "*2\r\n$3\r\nGET\r\n$7\r\nCS06142\r\n";
            send_receive(sock, message);
        } else {
            // 处理无效命令
            cout << "无效命令，请重新输入" << endl;
        }

            sleep(1);

    }

    // 关闭套接字连接
    close(sock);
    return 0;
}
