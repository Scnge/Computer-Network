#include <iostream>
#include <string>
#include <vector>
// #include <ws2tcpip.h>
#include <ctime>
#include <thread>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define _WINSOCK_DEPRECATED_NO_WARNINGS // 禁止编译器显示与 Winsock / WIN API 相关的特定警告信息
#define _CRT_SECURE_NO_WARNINGS // 禁止编译器在编译过程中发出关于可能存在安全风险的函数警告

#define MAX_CLIENTS 3
#define BUFFER_SIZE 1024

// 客户端结构体
struct Client {
    SOCKET sock = INVALID_SOCKET;
    string username;

    Client(SOCKET sock = INVALID_SOCKET, string username = "NULL") : sock(sock), username(username) {}
};
vector<Client> clients(MAX_CLIENTS);
vector<thread> clientThreads(MAX_CLIENTS);

// 获取当前时间
string GetNowTime() {
    time_t now = time(nullptr);
    struct tm* ltm = localtime(&now);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);
    return string(buffer);
}

// 时间 + 消息
void PrintInfo(const string& info) {
    cout << GetNowTime() << " " << info << endl;
}

// 寻找空闲线程
int FindFreeThread() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sock == INVALID_SOCKET)
            return i;
    }
    return -1;
}

// 同步发送消息
void ShareMessage(const string& msg) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sock != INVALID_SOCKET) {
            send(clients[i].sock, msg.c_str(), msg.length(), 0);
        }
    }
}

// 线程
void thread_func(int index) {
    Client& client = clients[index];
    char buffer[BUFFER_SIZE];
    int bytes;

    bytes = recv(client.sock, buffer, BUFFER_SIZE, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        client.username = buffer;

        string welcomeMsg = "Welcome " + client.username + " to the chat room!";
        PrintInfo(client.username + " joined the chat room.");
        ShareMessage("System message: " + welcomeMsg);

        while (true) {
            memset(buffer, 0, BUFFER_SIZE);
            bytes = recv(client.sock, buffer, BUFFER_SIZE, 0);
            if (bytes <= 0) break;
            buffer[bytes] = '\0';
            string message = client.username + ": " + buffer;
            PrintInfo("Message: " + message);
            ShareMessage(message);
        }

        client.sock = INVALID_SOCKET;
        PrintInfo(client.username + " has left the chat room.");
    }

    closesocket(client.sock);
}

int main() {
    UINT port;
    cout << "Please enter the port number:";
    cin >> port;

    // socket初始化
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == listen_socket){
        cout << "Create listen socket failed. Error Code:" << GetLastError() << endl;
        return -1;
    }
    else
        cout << "Create listen socket successed." << endl;

    // 绑定端口号
    struct sockaddr_in local = { 0 };
    local.sin_family = AF_INET;
    local.sin_port = htons(port); // 大小端转换
    local.sin_addr.s_addr = inet_addr("0.0.0.0"); // 定义全0地址，全部接受

    if (-1 == bind(listen_socket, (struct sockaddr*)&local, sizeof(local))){
        cout << "Bind listen socket failed. Error Code:" << GetLastError() << endl;
        return -1;
    }
    else
        cout << "Bind listen socket successed." << endl;

    // 开启监听
    if (-1 == listen(listen_socket, MAX_CLIENTS)){
        cout << "Start listen failed. Error Code:" << GetLastError() << endl;
        return -1;
    }
    else
        cout << "Start listen successed." << endl;

    PrintInfo("Server started on port " + to_string(port));

    // 等待连接客户端
    try {
        while (1) {
            SOCKET client_socket = accept(listen_socket, NULL, NULL);

            if (client_socket != INVALID_SOCKET) {
                int pos = FindFreeThread();
                if (pos == -1) {
                    PrintInfo("The server has reached its maximum capacity, the connection has failed.");
                    closesocket(client_socket);
                    continue;
                }

                clients[pos].sock = client_socket;
                clientThreads[pos] = thread(thread_func, pos);
                clientThreads[pos].detach();
            }
        }
    }
    catch (const exception& e) {
        PrintInfo("Server Error: " + string(e.what()));
    }

    WSACleanup();
    return 0;
}
