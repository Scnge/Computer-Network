#include <iostream>
#include <stdio.h>
#include <winsock2.h>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <fstream>
#include <windows.h>
#include <queue>
#include <vector>

using namespace std;
#define cin std::cin
#define cout std::cout 

#pragma comment(lib, "ws2_32.lib")

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#define Max_DATA_SIZE 14000
#define MAX_WAIT_TIME 5000
#define MAX_SEND_TIMES 50
#define MaxFileSize 100000000
#define Window_Size 20

int RouterPORT = 0;
int ClientPORT = 0;

int init_seq = 0;
const short SYN = 0x1;
const short ACK = 0x2;
const short FIN = 0x4;
const short FileName = 0x8;

int start = 0;
int arrive = 0;
int messagestart;
bool finish = false;
bool resend = false;

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

//定义创建ACK接收线程时候传过去的参数
struct parameters {
	SOCKET clientSocket;
	SOCKADDR_IN serverAddr;
	int nummessage;
};

queue<Message> messageBuffer; // 缓冲区存已发送未确认的message
HANDLE mutex; // 辅助加锁

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

void updateBuffer(int ackNum) {
	while (!messageBuffer.empty()) {
		const Message& frontMsg = messageBuffer.front();
		if (frontMsg.SeqNum <= ackNum) {
			messageBuffer.pop();  // 删除已确认的消息
		}
		else
			break;  // 一旦遇到一个未确认的消息，停止循环
	}
}

bool Three_Shakehands(SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	int AddrLen = sizeof(serverAddr);
	Message msg1, msg2, msg3;
	int resendtimes = 0;

	msg1.SrcPort = ClientPORT;
	msg1.DestPort = RouterPORT;
	msg1.flag += SYN;
	msg1.SeqNum = init_seq;
	msg1.setCheck();

	int sendByte = sendto(clientSocket, (char*)&msg1, sizeof(msg1), 0, (sockaddr*)&serverAddr, AddrLen);
	clock_t msg1start = clock();
	if (sendByte > 0)
		Print("First Handshake successed.(Send)");

	while (1) {
		int recvByte = recvfrom(clientSocket, (char*)&msg2, sizeof(msg2), 0, (sockaddr*)&serverAddr, &AddrLen);
		if (recvByte > 0) {
			if ((msg2.flag & ACK) && (msg2.flag & SYN) && msg2.check() && (msg2.AckNum == msg1.SeqNum + 1)) {
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
	msg3.SeqNum = ++init_seq;
	msg3.AckNum = msg2.SeqNum + 1;
	msg3.setCheck();

	sendByte = sendto(clientSocket, (char*)&msg3, sizeof(msg3), 0, (sockaddr*)&serverAddr, AddrLen);
	clock_t msg3start = clock();
	if (sendByte == 0) {
		Print("Third Handshake failed.(Send)");
		return false;
	}
	Print("Third Handshake success.(Send)");

	start = init_seq + 1;
	arrive = init_seq + 1;
	return true;
}

//接收ack的线程
DWORD WINAPI recvackthread(PVOID useparameter) {
	mutex = CreateMutex(NULL, FALSE, NULL);
	if (mutex == NULL) {
        Print("CreateMutex failed.");
        return 1;
    }

	parameters* p = (parameters*)useparameter;
	if (p == NULL) {
        Print("Invalid parameters.");
        CloseHandle(mutex);
        return 1;
    }

	SOCKADDR_IN serverAddr = p->serverAddr;
	SOCKET clientSocket = p->clientSocket;
	int nummessage = p->nummessage;
	int AddrLen = sizeof(serverAddr);

	int errorack = -1;
	int errorcount = 0;

	unsigned long mode = 1;
	if (ioctlsocket(clientSocket, FIONBIO, &mode) == SOCKET_ERROR) {
        Print("ioctlsocket failed.");
        CloseHandle(mutex);
        return 1;
    }

	while (1) {
		Message recvMsg;
		int recvByte = recvfrom(clientSocket, (char*)&recvMsg, sizeof(recvMsg), 0, (sockaddr*)&serverAddr, &AddrLen);

		if (recvByte > 0) {
			if (recvMsg.check()) {
				if (recvMsg.AckNum >= start) {
					WaitForSingleObject(mutex, INFINITE);
					updateBuffer(recvMsg.AckNum);
					start = recvMsg.AckNum + 1;
					cout << Get_Time() << "   " << "[Receive] ack = " << recvMsg.AckNum << endl;
					cout << Get_Time() << "   " << "[Window] Window Residual Size: " << (Window_Size - (arrive - start)) << ", has sent but not received confirmation: " << (arrive - start) << endl;
					ReleaseMutex(mutex); 
				}
				if (start != arrive)
					messagestart = clock();

				// 结束
				if (recvMsg.AckNum == nummessage + 1) {
					finish = true;
					return 0;
				}
				// 快速重传
				if (errorack != recvMsg.AckNum) {
					errorcount = 0;
					errorack = recvMsg.AckNum;
				}
				else
					errorcount++;

				if (errorcount == 3)
					resend = true;
			}
		}
	}
	CloseHandle(mutex);
	return 0;
}

bool Four_Wavehands(SOCKET clientSocket, SOCKADDR_IN serverAddr) {
	init_seq = arrive;
	int AddrLen = sizeof(serverAddr);
	Message msg1, msg2, msg3, msg4;
	int resendtimes = 0;

	msg1.SrcPort = ClientPORT;
	msg1.DestPort = RouterPORT;
	msg1.flag += FIN;
	msg1.flag += ACK;
	msg1.SeqNum = ++init_seq;
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
		if (recvByte == 0) {
			Print("Second Handwave failed.(Receive)");
			return false;
		}
		else if (recvByte > 0) {
			if ((msg2.flag & ACK) && msg2.check() && (msg2.AckNum == msg1.SeqNum + 1)) {
				Print("Second Handwave successed.(Receive)");
				break;
			}
			else
				continue;
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
		if (recvByte == 0) {
			Print("Third Handwave failed.(Receive)");
			return false;
		}
		else if (recvByte > 0)
		{
			if (recvByte > 0) {
				if ((msg3.flag & ACK) && (msg3.flag & FIN) && msg3.check()) {
					Print("Third Handwave successed.(Receive)");
					break;
				}
			}
			else
				continue;
		}
	}

	msg4.SrcPort = ClientPORT;
	msg4.DestPort = RouterPORT;
	msg4.flag += ACK;
	msg4.SeqNum = init_seq;
	msg4.AckNum = msg3.SeqNum + 1;
	msg4.setCheck();
	sendByte = sendto(clientSocket, (char*)&msg4, sizeof(msg4), 0, (sockaddr*)&serverAddr, AddrLen);
	if (sendByte == 0) {
		Print("Forth Handwave failed.(Send)");
		return false;
	}
	Print("Forth Handwave successed.(Send)");

	int tempclock = clock();
	Print("Client in TIME_WAIT state");
	Message tmp;
	while (clock() - tempclock < 2 * MAX_WAIT_TIME) {
		int recvByte = recvfrom(clientSocket, (char*)&tmp, sizeof(tmp), 0, (sockaddr*)&serverAddr, &AddrLen);
		if (recvByte == 0) {
			Print("Received error message while in TIMETWAIT status, exiting");
			return false;
		}
		else if (recvByte > 0) {
			sendByte = sendto(clientSocket, (char*)&msg4, sizeof(msg4), 0, (sockaddr*)&serverAddr, AddrLen);
			Print("Resent final ACK during TIME_WAIT");
		}
	}
	Print("Client successfully closed connection!");
	return true;
}

int main() {
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

	mutex = CreateMutex(NULL, FALSE, NULL);
	ifstream fin(filename.c_str(), ifstream::binary);
	if (!fin) {
		Print("Unable to open file.");
		return -1;
	}

	BYTE* fileBuffer = new BYTE[MaxFileSize];
	unsigned int fileSize = 0;
	BYTE byte = fin.get();
	while (fin) {
		fileBuffer[fileSize++] = byte;
		byte = fin.get();
	}
	fin.close();

	int batchNum = fileSize / Max_DATA_SIZE;
	int leftNum = fileSize % Max_DATA_SIZE;
	int nummessage;

	if (leftNum != 0)
		nummessage = batchNum + 2;
	else
		nummessage = batchNum + 1;

	parameters useparameter;
	useparameter.serverAddr = serverAddr;
	useparameter.clientSocket = clientSocket;
	useparameter.nummessage = nummessage;
	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)recvackthread, &useparameter, 0, 0);

	int count = 0;
	while (1) {
		if (arrive < start + Window_Size && arrive < nummessage + 2) {
			Message datamessage;
			if (arrive == 2) {
				datamessage.SrcPort = ClientPORT;
				datamessage.DestPort = RouterPORT;
				datamessage.size = fileSize;
				datamessage.flag += FileName;
				datamessage.SeqNum = arrive;

				for (int i = 0; i < realname.size(); i++)
					datamessage.data[i] = realname[i];
				datamessage.data[realname.size()] = '\0';
				datamessage.setCheck();
			}
			else if (arrive == batchNum + 3 && leftNum > 0) {
				datamessage.SrcPort = ClientPORT;
				datamessage.DestPort = RouterPORT;
				datamessage.SeqNum = arrive;
				for (int j = 0; j < leftNum; j++)
					datamessage.data[j] = fileBuffer[batchNum * Max_DATA_SIZE + j];

				datamessage.setCheck();
			}
			else {
				datamessage.SrcPort = ClientPORT;
				datamessage.DestPort = RouterPORT;
				datamessage.SeqNum = arrive;
				for (int j = 0; j < Max_DATA_SIZE; j++)
				{
					datamessage.data[j] = fileBuffer[count * Max_DATA_SIZE + j];
				}
				datamessage.setCheck();
				count++;
			}
			if (start == arrive)
				messagestart = clock();
			
			WaitForSingleObject(mutex, INFINITE);

			messageBuffer.push(datamessage);
			sendto(clientSocket, (char*)&datamessage, sizeof(datamessage), 0, (sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
			arrive++;
			cout << Get_Time() << "   " << "[Send] seq = " << datamessage.SeqNum << ", checknum = " << datamessage.checkNum << endl;
			cout << Get_Time() << "   " << "[Window] Window size: " << Window_Size << ", has sent but not received confirmation: " << (arrive - start) << endl;
			ReleaseMutex(mutex);
			
		}

		// 超时重传 快速重传
		if (resend || clock() - messagestart > MAX_WAIT_TIME) {
			if (resend)
				Print("Fast Retransmit...");
			
			WaitForSingleObject(mutex, INFINITE); 
			for (int i = 0; i < arrive - start; i++) {
				Message resendMsg = messageBuffer.front();
				sendto(clientSocket, (char*)&resendMsg, sizeof(resendMsg), 0, (sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
				cout << Get_Time() << "   " << "[Resend] seq = " << resendMsg.SeqNum << endl;

				messageBuffer.push(resendMsg);
				messageBuffer.pop();
			}
			ReleaseMutex(mutex); 
			
			messagestart = clock();
			resend = false;
		}

		if (finish == true)
			break;
	}
	CloseHandle(hThread);

	int endtime = clock();
	cout << Get_Time() << "   " << "Total transfer time: " << (endtime - starttime) << "ms" << endl;
	cout << Get_Time() << "   " << "Average throughput: " << ((float)fileSize) / (endtime - starttime) << " bytes/ms" << endl;
	delete[] fileBuffer;
	CloseHandle(mutex);

	bool breaked = Four_Wavehands(clientSocket, serverAddr);
	if (!breaked) {
		Print("Failed to disconnect client");
		return -1;
	}
	closesocket(clientSocket);
	WSACleanup();

	return 0;
}