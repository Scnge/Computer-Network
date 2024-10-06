#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define _WINSOCK_DEPRECATED_NO_WARNINGS // 禁止编译器显示与 Winsock / WIN API 相关的特定警告信息
#define _CRT_SECURE_NO_WARNINGS // 禁止编译器在编译过程中发出关于可能存在安全风险的函数警告

// 接收消息
void ReceiveMessages(SOCKET sock) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            cout << "Disconnected with server." << endl;
            break;
        }
        cout << buffer << endl;
    }
}

int main() {
    string ipAddress;
    int port;
    cout << "Please enter your IP address:";
    cin >> ipAddress;
    cout << "Please enter the port number:";    
    cin >> port;

    string userInput;
    cout << "Enter your username: ";
    cin >> userInput;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 初始化socket
    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        cout << "Create listen socket failed. Error Code:" << GetLastError() << endl;
        // WSACleanup();
        return -1;
    }
    else
        cout << "Create listen socket successed." << endl; 
    
    // 绑定端口号
    struct sockaddr_in target;
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    inet_pton(AF_INET, ipAddress.c_str(), &target.sin_addr);

    if (-1 == connect(client_socket, (struct sockaddr*)&target, sizeof(target)))
    {
        cout << "Connect server failed." << endl;
        closesocket(client_socket);
        // WSACleanup();
        return -1;
    }
    else
        cout << "Connect server successed." << endl;

    // 单线程接收消息
    thread receiveThread(ReceiveMessages, client_socket);

    // 发送循环
    send(client_socket, userInput.c_str(), userInput.size() + 1, 0);

    cout << "Connected successfully. Type 'exit' to close the connection." << endl;

    do {
        getline(cin, userInput);
        if (userInput.size() > 0) {
            int sendResult = send(client_socket, userInput.c_str(), userInput.size() + 1, 0);
            if (sendResult == SOCKET_ERROR) {
                cout << "Send failed. Please try later." << endl;
                continue;
            }
        }
    } while (userInput != "exit");

    // 关闭连接
    shutdown(client_socket, SD_SEND);
    receiveThread.join();

    closesocket(client_socket);
    WSACleanup();
    return 0;
}
