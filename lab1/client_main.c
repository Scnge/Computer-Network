#include <stdio.h>
#include <string.h>
#include <WinSock2.h>

#pragma comment(lib,"ws2_32.lib")

int main()
{
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	// 创建socket
	SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == client_socket)
	{
		printf("Create socket failed.\n");
		return -1;
	}

	// 连接服务器
	struct sockaddr_in target;
	target.sin_family = AF_INET; 
	target.sin_port = htons(8080); 
	target.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (-1 == connect(client_socket, (struct sockaddr*)&target, sizeof(target)))
	{
		printf("Connect server failed.\n");
		closesocket(client_socket);
		return -1;
	}

	// 开始通讯 send recv
	while (1)
	{
		char sbuffer[1024] = { 0 };
		scanf("%s", sbuffer);
		send(client_socket, sbuffer, strlen(sbuffer), 0);

		char rbuffer[1024] = { 0 };
		int ret = recv(client_socket, rbuffer, 1024, 0);
		if (ret <= 0)break;
		printf("%s\n", rbuffer);
	}

	// 
	closesocket(client_socket);
}