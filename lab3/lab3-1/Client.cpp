#include <iostream>
#include <stdio.h>
#include <winsock2.h>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <fstream>

using namespace std;
#define cin std::cin
#define cout std::cout

#pragma comment(lib, "ws2_32.lib")

int RouterPORT = 0;
int ClientPORT = 0;

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#define Max_DATA_SIZE 14000
#define MAX_WAIT_TIME 5000
#define MAX_SEND_TIMES 50
#define MaxFileSize 100000000

int init_seq = 0;
const short SYN = 0x1;
const short ACK = 0x2;
const short FIN = 0x4;
const short FileName = 0x8;

struct Message {
    int SrcIP, DestIP;
    short SrcPort, DestPort;
    int Seq;
    int Ack;
    int size;
    short flag;
    short checknum;
    BYTE data[Max_DATA_SIZE];

    Message() : SrcIP(0), DestIP(0), SrcPort(0), DestPort(0),
        Seq(0), Ack(0), size(0), flag(0), checknum(0) {
        memset(data, 0, sizeof(data));
    }

    bool check() {
        unsigned int sum = 0;
        unsigned short* msgStream = (unsigned short*)this;

        for (int i = 0; i < sizeof(*this) / 2; i++) {
            sum += *msgStream++;
            if (sum & 0xFFFF0000) {
                sum &= 0xFFFF;
                sum++;
            }
        }
        return (sum & 0xFFFF) == 0xFFFF;
    };
    void setCheck() {
        this->checknum = 0;
        int sum = 0;
        unsigned short* msgStream = (unsigned short*)this;

        for (int i = 0; i < sizeof(*this) / 2; i++) {
            sum += *msgStream++;
            if (sum & 0xFFFF0000) {
                sum &= 0xFFFF;
                sum++;
            }
        }
        this->checknum = ~(sum & 0xFFFF);
    };
};

string Get_Time() {
    time_t now = time(nullptr);
    struct tm ltm;
    localtime_s(&ltm, &now);

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &ltm);
    return string(buffer);
}

void Print(const string& Info) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD saved_attributes;

    cout << Get_Time() << "   " << Info << endl;
}

bool Three_Shakehands(SOCKET clientSocket, SOCKADDR_IN serverAddr) {
    int AddrLen = sizeof(serverAddr);
    Message msg1, msg2, msg3;
    int resendtimes = 0;

    msg1.SrcPort = ClientPORT;
    msg1.DestPort = RouterPORT;
    msg1.flag += SYN;
    msg1.Seq = init_seq;
    msg1.setCheck();

    int sendByte = sendto(clientSocket, (char*)&msg1, sizeof(msg1), 0, (sockaddr*)&serverAddr, AddrLen);
    clock_t msg1start = clock();
    if (sendByte > 0)
        Print("First Handshake successed.(Send)");

    while (1) {
        int recvByte = recvfrom(clientSocket, (char*)&msg2, sizeof(msg2), 0, (sockaddr*)&serverAddr, &AddrLen);
        if (recvByte > 0) {
            if ((msg2.flag & ACK) && (msg2.flag & SYN) && msg2.check() && (msg2.Ack == msg1.Seq + 1)) {
                Print("Second Handshake successed.(Receive)");
                break;
            }
            else
                Print("Second Handshake failed.(Receive)");
        }

        if (clock() - msg1start > MAX_WAIT_TIME) {
            if (++resendtimes > MAX_SEND_TIMES) {
                Print("First Handshake has reach max times.");
                return false;
            }
            cout << Get_Time() << "   " << "First Handshake Resend, time " << resendtimes << endl;
            sendByte = sendto(clientSocket, (char*)&msg1, sizeof(msg1), 0, (sockaddr*)&serverAddr, AddrLen);
            msg1start = clock();
            if (sendByte <= 0) {
                Print("First Handshake resended failed.");
                return false;
            }
        }
    }

    msg3.SrcPort = ClientPORT;
    msg3.DestPort = RouterPORT;
    msg3.flag += ACK;
    msg3.Seq = ++init_seq;
    msg3.Ack = msg2.Seq + 1;
    msg3.setCheck();

    sendByte = sendto(clientSocket, (char*)&msg3, sizeof(msg3), 0, (sockaddr*)&serverAddr, AddrLen);
    if (sendByte == 0) {
        Print("Third Handshake failed.(Send)");
        return false;
    }
    Print("Third Handshake success.(Send)");
    return true;
}

bool send_Message(Message& sendMsg, SOCKET clientSocket, SOCKADDR_IN serverAddr) {
    sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
    cout << Get_Time() << "   Client sent: "
        << "SrcPort: " << sendMsg.SrcPort << ", DestPort: " << sendMsg.DestPort << ", Seq: " << sendMsg.Seq << ", Checksum: " << sendMsg.checknum << endl;

    int msgStart = clock();
    Message recvMsg;
    int AddrLen = sizeof(serverAddr);
    int resendtimes = 0;

    while (1) {
        int recvByte = recvfrom(clientSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&serverAddr, &AddrLen);
        if (recvByte > 0) {
            if ((recvMsg.flag & ACK) && (recvMsg.Ack == sendMsg.Seq)) {
                cout << Get_Time() << "   " << "[Receive]ACK for ack = " << recvMsg.Ack << endl;
                return true;
            }
        }

        if (clock() - msgStart > MAX_WAIT_TIME) {
            cout << Get_Time() << "   " << "Message with seq = " << sendMsg.Seq << " timed out, resended time " << ++resendtimes << endl;
            int sendByte = sendto(clientSocket, (char*)&sendMsg, sizeof(sendMsg), 0, (sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
            msgStart = clock();
            if (sendByte > 0) {
                Print("Message retransmitted successfully");
                break;
            }
            else {
                Print("Message retransmission failed");
            }
        }

        if (resendtimes == MAX_SEND_TIMES) {
            Print("Maximum retransmission limit reached, sending failed");
            return false;
        }
    }
    return true;
}

bool Four_Wavehands(SOCKET clientSocket, SOCKADDR_IN serverAddr) {
    int AddrLen = sizeof(serverAddr);
    Message msg1, msg2, msg3, msg4;
    int resendtimes = 0;

    // Send first FIN message
    msg1.SrcPort = ClientPORT;
    msg1.DestPort = RouterPORT;
    msg1.flag += FIN;
    msg1.flag += ACK;
    msg1.Seq = ++init_seq;
    msg1.setCheck();
    int sendByte = sendto(clientSocket, (char*)&msg1, sizeof(msg1), 0, (sockaddr*)&serverAddr, AddrLen);
    clock_t msg1start = clock();
    if (sendByte == 0) {
        Print("First Handwave failed.(Send)");
        return false;
    }
    Print("First Handwave successed.(Send)");

    
    while (1) {
        int recvByte = recvfrom(clientSocket, (char*)&msg2, sizeof(msg2), 0, (sockaddr*)&serverAddr, &AddrLen);
        if (recvByte > 0) {
            if ((msg2.flag & ACK) && msg2.check() && (msg2.Ack == msg1.Seq + 1)) {
                Print("Second Handwave successed.(Receive)");
                break;
            }
        }

        if (clock() - msg1start > MAX_WAIT_TIME) {
            cout << Get_Time() << "   " << "First Handwave Resend, time " << ++resendtimes << endl;
            sendByte = sendto(clientSocket, (char*)&msg1, sizeof(msg1), 0, (sockaddr*)&serverAddr, AddrLen);
            msg1start = clock();
            if (sendByte > 0) {
                Print("First Handwave resended successfully.");
                break;
            }
            else
                Print("First Handwave resended fialedly.");
        }
        if (resendtimes == MAX_SEND_TIMES) {
            Print("First Handwave resended times have reached max num.");
            return false;
        }
    }

    while (1) {
        int recvByte = recvfrom(clientSocket, (char*)&msg3, sizeof(msg3), 0, (sockaddr*)&serverAddr, &AddrLen);
        if (recvByte > 0) {
            if ((msg3.flag & ACK) && (msg3.flag & FIN) && msg3.check()) {
                Print("Third Handwave successed.(Receive)");
                break;
            }
        }
    }

    // Send final ACK
    msg4.SrcPort = ClientPORT;
    msg4.DestPort = RouterPORT;
    msg4.flag += ACK;
    msg4.Seq = ++init_seq;
    msg4.Ack = msg3.Seq + 1;
    msg4.setCheck();
    sendByte = sendto(clientSocket, (char*)&msg4, sizeof(msg4), 0, (sockaddr*)&serverAddr, AddrLen);
    if (sendByte == 0) {
        Print("Forth Handwave failed.(Send)");
        return false;
    }
    Print("Forth Handwave successed.(Send)");

    // TIME_WAIT state
    int tempclock = clock();
    Print("Client in TIME_WAIT state");
    Message tmp;
    while (clock() - tempclock < 2 * MAX_WAIT_TIME) {
        int recvByte = recvfrom(clientSocket, (char*)&tmp, sizeof(tmp), 0, (sockaddr*)&serverAddr, &AddrLen);
        if (recvByte > 0) {
            sendByte = sendto(clientSocket, (char*)&msg4, sizeof(msg4), 0, (sockaddr*)&serverAddr, AddrLen);
            Print("Resent final ACK during TIME_WAIT");
        }
    }
    Print("Client successfully closed connection!");
    return true;
}

int main() {
    int RouterPORT, ClientPORT;
    cout << "Please Input Router Port: ";
    cin >> RouterPORT;
    cout << "Please Input Client Port: ";
    cin >> ClientPORT;

    WSADATA wsaDataStruct;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaDataStruct);
    if (result != 0) {
        Print("Failed to initialize Winsock");
        return -1;
    }
    if (wsaDataStruct.wVersion != MAKEWORD(2, 2)) {
        Print("Unsupported Winsock version");
        WSACleanup();
        return -1;
    }
    Print("Initialized Winsock successfully");

    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        Print("Failed to create socket");
        return -1;
    }

    unsigned long mode = 1;
    if (ioctlsocket(clientSocket, FIONBIO, &mode) != NO_ERROR) {
        Print("Failed to set socket to non-blocking mode");
        closesocket(clientSocket);
        return -1;
    }
    Print("Created socket successfully");

    SOCKADDR_IN serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(RouterPORT);

    SOCKADDR_IN clientAddr;
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    clientAddr.sin_port = htons(ClientPORT);

    bind(clientSocket, (LPSOCKADDR)&clientAddr, sizeof(clientAddr));

    bool connected = Three_Shakehands(clientSocket, serverAddr);
    if (!connected) {
        Print("Failed to establish client connection");
        return -1;
    }

    string filename;
    cout << "Enter the filename to send: ";
    cin >> filename;
    int starttime = clock();
    string realname = filename;

    ifstream fin(filename.c_str(), ifstream::binary);
    if (!fin) {
        Print("Unable to open file.");
        return -1;
    }

    BYTE* filemsg = new BYTE[MaxFileSize];
    unsigned int fileSize = 0;
    BYTE byte = fin.get();
    while (fin) {
        filemsg[fileSize++] = byte;
        byte = fin.get();
    }
    fin.close();

    Message nameMessage;
    nameMessage.SrcPort = ClientPORT;
    nameMessage.DestPort = RouterPORT;
    nameMessage.size = fileSize;
    nameMessage.flag += FileName;
    nameMessage.Seq = ++init_seq;
    for (int i = 0; i < realname.size(); i++)
        nameMessage.data[i] = realname[i];
    nameMessage.data[realname.size()] = '\0';
    nameMessage.setCheck();
    if (!send_Message(nameMessage, clientSocket, serverAddr)) {
        Print("Failed to send filename and file size");
        return -1;
    }
    Print("Filename and file size sent successfully");

    int batchNum = fileSize / Max_DATA_SIZE;
    int leftNum = fileSize % Max_DATA_SIZE;
    for (int i = 0; i < batchNum; i++) {
        Message dataMsg;
        dataMsg.SrcPort = ClientPORT;
        dataMsg.DestPort = RouterPORT;
        dataMsg.Seq = ++init_seq;
        for (int j = 0; j < Max_DATA_SIZE; j++) {
            dataMsg.data[j] = filemsg[i * Max_DATA_SIZE + j];
        }
        dataMsg.setCheck();
        if (!send_Message(dataMsg, clientSocket, serverAddr)) {
            Print("Failed to send full data packet");
            return -1;
        }
        cout << Get_Time() << "   " << "Full data packet " << i + 1 << " sent successfully" << endl << endl;
    }

    if (leftNum > 0) {
        Message dataMsg;
        dataMsg.SrcPort = ClientPORT;
        dataMsg.DestPort = RouterPORT;
        dataMsg.Seq = ++init_seq;
        for (int j = 0; j < leftNum; j++) {
            dataMsg.data[j] = filemsg[batchNum * Max_DATA_SIZE + j];
        }
        dataMsg.setCheck();
        if (!send_Message(dataMsg, clientSocket, serverAddr)) {
            Print("Failed to send partial data packet");
            return -1;
        }
        Print("Partial data packet sent successfully");
    }

    int endtime = clock();
    cout << "Total transfer time: " << (endtime - starttime) << "ms" << endl;
    cout << "Average throughput: " << ((float)fileSize) / (endtime - starttime) << " bytes/ms" << endl << endl;
    delete[] filemsg;

    bool breaked = Four_Wavehands(clientSocket, serverAddr);
    if (!breaked) {
        Print("Failed to disconnect client");
        return -1;
    }
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
