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

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#define Max_DATA_SIZE 14000
#define MAX_WAIT_TIME 500
#define MAX_SEND_TIMES 50
#define MaxFileSize 100000000

int ServerPORT = 0;
int RouterPORT = 0;

int init_seq = 0;
const short SYN = 0x1;
const short ACK = 0x2;
const short FIN = 0x4;
const short FileName = 0x8;

struct Message {
    int SrcIP, DestIP;
    short SrcPort, DestPort;
    int SeqNum;
    int AckNum;
    int size;
    short flag;
    short checkNum;
    BYTE data[Max_DATA_SIZE];

    Message() : SrcIP(0), DestIP(0), SrcPort(0), DestPort(0),
        SeqNum(0), AckNum(0), size(0), flag(0), checkNum(0) {
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
        this->checkNum = 0;
        int sum = 0;
        unsigned short* msgStream = (unsigned short*)this;

        for (int i = 0; i < sizeof(*this) / 2; i++) {
            sum += *msgStream++;
            if (sum & 0xFFFF0000) {
                sum &= 0xFFFF;
                sum++;
            }
        }
        this->checkNum = ~(sum & 0xFFFF);
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

bool Three_Shakehands(SOCKET serverSocket, SOCKADDR_IN clientAddr) {
	int AddrLen = sizeof(clientAddr);
	Message msg1, msg2, msg3;
	int resendtimes = 0;

	while (1) {
		int recvByte = recvfrom(serverSocket, (char*)&msg1, sizeof(msg1), 0, (sockaddr*)&clientAddr, &AddrLen);

		if (recvByte > 0) {
			if (!(msg1.flag & SYN) || !msg1.check()) {
				Print("First Handshake failed.(Receive)");
				return false;
			}
			Print("First Handshake successed.(Receive)");

			msg2.SrcPort = ServerPORT;
			msg2.DestPort = RouterPORT;
			msg2.SeqNum = init_seq;
			msg2.AckNum = msg1.SeqNum + 1;
			msg2.flag += SYN;
			msg2.flag += ACK;
			msg2.setCheck();

			int sendByte = sendto(serverSocket, (char*)&msg2, sizeof(msg2), 0, (sockaddr*)&clientAddr, AddrLen);
			clock_t msg2start = clock();

			if (sendByte == 0) {
				Print("Second Handshake failed.(Send)");
				return false;
			}
			Print("Second Handshake successed.(Send)");

			while (true) {
				int recvByte = recvfrom(serverSocket, (char*)&msg3, sizeof(msg3), 0, (sockaddr*)&clientAddr, &AddrLen);
				if (recvByte > 0) {
					if ((msg3.flag & ACK) && msg3.check() && (msg3.AckNum == msg2.SeqNum + 1)) {
						init_seq++;
						Print("Third Handshake successed.(Receive)");
						return true;
					}
					else {
						Print("Third Handshake failed.(Receive)");
						return false;
					}
				}

				if (clock() - msg2start > MAX_WAIT_TIME) {
					if (++resendtimes > MAX_SEND_TIMES) {
						Print("Second Handshake resended times have reached max num.");
						return false;
					}
					cout << Get_Time() << "   " << "Second Handshake Resend, time " << resendtimes << endl;
					int sendByte = sendto(serverSocket, (char*)&msg2, sizeof(msg2), 0, (sockaddr*)&clientAddr, AddrLen);
					msg2start = clock();
					if (sendByte > 0)
						Print("Second Handshake resended successfully.");
					else
						Print("Second Handshake resended failedly.");
				}
			}
		}
	}
	return false;
}


//ʵ�ֵ������Ľ���
bool rec_Message(Message& recvMsg, SOCKET serverSocket, SOCKADDR_IN clientAddr)
{
	int AddrLen = sizeof(clientAddr);

	while (1){
		int recvByte = recvfrom(serverSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&clientAddr, &AddrLen);
		if (recvByte > 0){
			if (recvMsg.check() && (recvMsg.SeqNum == init_seq + 1)){
				Message replyMessage;
				replyMessage.SrcPort = ServerPORT;
				replyMessage.DestPort = RouterPORT;
				replyMessage.flag += ACK;
				replyMessage.SeqNum = init_seq++;
				replyMessage.AckNum = recvMsg.SeqNum;
				replyMessage.setCheck();
				sendto(serverSocket, (char*)&replyMessage, sizeof(replyMessage), 0, (sockaddr*)&clientAddr, sizeof(SOCKADDR_IN));
				cout << Get_Time() << "   " << "[Send]seq = " << recvMsg.SeqNum << endl;
				cout << Get_Time() << "   " << "[Send]seq = " << replyMessage.SeqNum << ", ack = " << replyMessage.AckNum << endl;
				return true;
			}

			else if (recvMsg.check() && (recvMsg.SeqNum != init_seq + 1)){
				Message replyMessage;
				replyMessage.SrcPort = ServerPORT;
				replyMessage.DestPort = RouterPORT;
				replyMessage.flag += ACK;
				replyMessage.SeqNum = init_seq;
				replyMessage.AckNum = init_seq;
				replyMessage.setCheck();
				sendto(serverSocket, (char*)&replyMessage, sizeof(replyMessage), 0, (sockaddr*)&clientAddr, sizeof(SOCKADDR_IN));
				cout << Get_Time() << "   " << "[Cumulative Confirmation][Rereceive]seq = " << recvMsg.SeqNum << ", [Send]ack = " << replyMessage.AckNum << endl;
			}
		}
		else if (recvByte == 0)
		{
			return false;
		}
	}
	return true;
}

bool Four_Wavehands(SOCKET serverSocket, SOCKADDR_IN clientAddr) {
	int AddrLen = sizeof(clientAddr);
	Message msg1, msg2, msg3, msg4;
	int resendtimes = 0;

	while (1) {
		int recvByte = recvfrom(serverSocket, (char*)&msg1, sizeof(msg1), 0, (sockaddr*)&clientAddr, &AddrLen);
		if (recvByte == 0) {
			Print("First Handwave failed.(Receive)");
			return false;
		}
		else if (recvByte > 0) {
			if (!(msg1.flag & FIN) || !(msg1.flag & ACK) || !msg1.check()) {
				Print("First Handwave failed.(Receive)");
				return false;
			}
			Print("First Handwave successed.(Receive)");

			msg2.SrcPort = ServerPORT;
			msg2.DestPort = RouterPORT;
			msg2.SeqNum = init_seq++;
			msg2.AckNum = msg1.SeqNum + 1;
			msg2.flag += ACK;
			msg2.setCheck();
			int sendByte = sendto(serverSocket, (char*)&msg2, sizeof(msg2), 0, (sockaddr*)&clientAddr, AddrLen);
			clock_t msg2start = clock();
			if (sendByte == 0) {
				Print("Second Handwave failed.(Send)");
				return false;
			}
			Print("Second Handwave successed.(Send)");
			break;
		}
	}

	msg3.SrcPort = ServerPORT;
	msg3.DestPort = RouterPORT;
	msg3.flag += FIN;
	msg3.flag += ACK;
	msg3.SeqNum = init_seq++;
	msg3.setCheck();
	int sendByte = sendto(serverSocket, (char*)&msg3, sizeof(msg3), 0, (sockaddr*)&clientAddr, AddrLen);
	clock_t msg3start = clock();
	if (sendByte == 0) {
		Print("Third Handwave failed.(Send)");
		return false;
	}
	Print("Third Handwave successed.(Send)");

	while (1) {
		int recvByte = recvfrom(serverSocket, (char*)&msg4, sizeof(msg4), 0, (sockaddr*)&clientAddr, &AddrLen);
		if (recvByte == 0) {
			Print("Forth Handwave failed.(Receive)");
			return false;
		}
		else if (recvByte > 0) {
			if ((msg4.flag & ACK) && msg4.check() && (msg4.AckNum == msg3.SeqNum + 1)) {
				Print("Forth Handwave successed.(Receive)");
				break;
			}
			else {
				Print("Forth Handwave failed.(Receive)");
				return false;
			}
		}
		if (clock() - msg3start > MAX_WAIT_TIME) {
			cout << Get_Time() << "   " << "Third Handwave Resend, time " << ++resendtimes << endl;
			int sendByte = sendto(serverSocket, (char*)&msg3, sizeof(msg3), 0, (sockaddr*)&clientAddr, AddrLen);
			msg3start = clock();
			if (sendByte > 0) {
				Print("Third Handshake resended successfully.");
				continue;
			}
			else {
				Print("Third Handshake resended failedly.");
			}
		}
		if (resendtimes == MAX_SEND_TIMES) {
			Print("Third Handshake resended times have reached max num.");
			return false;
		}
	}
	Print("Server Connection has closed.");
	return true;
}

int main(){
	cout << "Please Input Router Port: ";
	cin >> RouterPORT;
	cout << "Please Input Server Port: ";
	cin >> ServerPORT;

	WSADATA wsaDataStruct;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaDataStruct);
	if (result != 0) {
		Print("Init Winsock Server failed.");
		return -1;
	}
	if (wsaDataStruct.wVersion != MAKEWORD(2, 2)) {
		Print("Winsock version is wrong.");
		WSACleanup();
		return -1;
	}
	Print("Init Winsock Server successfully.");

	SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (serverSocket == INVALID_SOCKET) {
		Print("Create socket failed.");
		return -1;
	}

	unsigned long mode = 1;
	if (ioctlsocket(serverSocket, FIONBIO, &mode) != NO_ERROR) {
		Print("Unable to set socket to non blocking mode.");
		closesocket(serverSocket);
		return -1;
	}
	Print("Create socket successfully.");

	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	serverAddr.sin_port = htons(ServerPORT);

	int tem = bind(serverSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr));
	if (tem == SOCKET_ERROR) {
		Print("Bind failed.");
		return -1;
	}
	Print("Bind successed.");
	Print("Start listening...");

	SOCKADDR_IN clientAddr;
	clientAddr.sin_family = AF_INET;
	clientAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	clientAddr.sin_port = htons(RouterPORT);

	bool isConn = Three_Shakehands(serverSocket, clientAddr);
	if (isConn == 0) {
		Print("Server connection failed.");
		return -1;
	}
	Print("Waiting for file...");

	int AddrLen = sizeof(clientAddr);
	Message nameMessage;
	unsigned int fileSize;
	char fileName[40] = { 0 };

	while (1) {
		int recvByte = recvfrom(serverSocket, (char*)&nameMessage, sizeof(nameMessage), 0, (sockaddr*)&clientAddr, &AddrLen);
		if (recvByte > 0) {
			if (nameMessage.check() && (nameMessage.SeqNum == init_seq + 1) && (nameMessage.flag & FileName)) {
				fileSize = nameMessage.size;
				for (int i = 0; nameMessage.data[i]; i++)
					fileName[i] = nameMessage.data[i];
				cout << Get_Time() << "   " << "[Receive]Filename: " << fileName << ", Filesize: " << fileSize << endl;

				Message replyMessage;
				replyMessage.SrcPort = ServerPORT;
				replyMessage.DestPort = RouterPORT;
				replyMessage.flag += ACK;
				replyMessage.SeqNum = init_seq++;
				replyMessage.AckNum = nameMessage.SeqNum;
				replyMessage.setCheck();
				sendto(serverSocket, (char*)&replyMessage, sizeof(replyMessage), 0, (sockaddr*)&clientAddr, sizeof(SOCKADDR_IN));
				cout << Get_Time() << "   " << "[Receive]seq = " << nameMessage.SeqNum << endl;
				cout << Get_Time() << "   " << "[Send]seq = " << replyMessage.SeqNum << ", ack = " << replyMessage.AckNum << endl;
				break;
			}

			// ���seqֵ����ȷ���������ģ������ۼ�ȷ�ϵ�ACK���ģ�ack=init_seq-1��
			else if (nameMessage.check() && (nameMessage.SeqNum != init_seq + 1) && (nameMessage.flag & FileName)) {
				Message replyMessage;
				replyMessage.SrcPort = ServerPORT;
				replyMessage.DestPort = RouterPORT;
				replyMessage.flag += ACK;
				replyMessage.SeqNum = init_seq + 1;
				replyMessage.AckNum = init_seq;
				replyMessage.setCheck();
				sendto(serverSocket, (char*)&replyMessage, sizeof(replyMessage), 0, (sockaddr*)&clientAddr, sizeof(SOCKADDR_IN));
				cout << Get_Time() << "   " << "[Cumulative Confirmation][Rereceive]seq = " << nameMessage.SeqNum << ", [Send]ack = " << replyMessage.AckNum << endl;
			}
		}
	}

	int batchNum = fileSize / Max_DATA_SIZE;
	int leftNum = fileSize % Max_DATA_SIZE;
	BYTE* fileBuffer = new BYTE[fileSize];

	for (int i = 0; i < batchNum; i++) {
		Message dataMsg;
		if (rec_Message(dataMsg, serverSocket, clientAddr)) {
			cout << Get_Time() << "   " << i + 1 << " full load data message received successfully." << endl;
		}
		else {
			cout << Get_Time() << "   " << i + 1 << " full load data message received failedly." << endl;
			return -1;
		}
		for (int j = 0; j < Max_DATA_SIZE; j++) {
			fileBuffer[i * Max_DATA_SIZE + j] = dataMsg.data[j];
		}
	}

	if (leftNum > 0) {
		Message dataMsg;
		if (rec_Message(dataMsg, serverSocket, clientAddr))
			Print("Not full load data message received successfully.");
		else {
			Print("Not full load data message received failedly.");
			return -1;
		}
		for (int j = 0; j < leftNum; j++) {
			fileBuffer[batchNum * Max_DATA_SIZE + j] = dataMsg.data[j];
		}
	}

	FILE* outputfile;
	outputfile = fopen(fileName, "wb");
	if (fileBuffer != 0) {
		fwrite(fileBuffer, fileSize, 1, outputfile);
		fclose(outputfile);
	}
	Print("File has written successfully.");
	delete[] fileBuffer;

	bool breaked = Four_Wavehands(serverSocket, clientAddr);
	if (!breaked) {
		Print("Disconnection failed.");
		return -1;
	}
	closesocket(serverSocket);
	WSACleanup();

	return 0;
}