#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

void ReceiveMessages(SOCKET sock) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            cout << "Disconnected from server." << endl;
            break;
        }
        cout << "Received: " << buffer << endl;
    }
}

int main() {
    WSADATA wsaData;
    string ipAddress = "127.0.0.1";  // IP Address of the server
    int port = 8080;                // Listening port on the server

    // Initialize WinSock
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Create socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cerr << "Can't create socket, Err #" << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // Fill in a hint structure
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(port);
    inet_pton(AF_INET, ipAddress.c_str(), &hint.sin_addr);

    // Connect to server
    int connResult = connect(sock, (sockaddr*)&hint, sizeof(hint));
    if (connResult == SOCKET_ERROR) {
        cerr << "Can't connect to server, Err #" << WSAGetLastError() << endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Start receiving messages in a separate thread
    thread receiveThread(ReceiveMessages, sock);

    // Send loop
    string userInput;
    cout << "Enter your username: ";
    getline(cin, userInput);
    send(sock, userInput.c_str(), userInput.size() + 1, 0); // Send the username first

    cout << "Connected successfully. Type 'exit' to close the connection." << endl;

    do {
        getline(cin, userInput);
        if (userInput.size() > 0) {  // Make sure the user has typed something
            int sendResult = send(sock, userInput.c_str(), userInput.size() + 1, 0);
            if (sendResult != SOCKET_ERROR) {
                // Successfully sent message to server
            }
        }
    } while (userInput != "exit");

    // Shutdown communication
    shutdown(sock, SD_SEND);
    receiveThread.join(); // Wait for the receiving thread to finish

    // Cleanup
    closesocket(sock);
    WSACleanup();

    return 0;
}
