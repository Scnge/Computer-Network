#include <stdio.h>
#include <string.h>
#include <WinSock2.h>
#include <ctime>

#pragma comment(lib,"ws2_32.lib")

#define _WINSOCK_DEPRECATED_NO_WARNINGS // 禁止编译器显示关于使用 Winsock API 的特定警告信息
#define _CRT_SECURE_NO_WARNINGS // 关闭一些编译器对安全函数的警告

DWORD WINAPI thread_func(LPVOID lpThreadParameter)
{
	SOCKET client_socket = *(SOCKET*)lpThreadParameter;
	free(lpThreadParameter);

	while (1) {
		// 开始通讯
		char buffer[1024] = { 0 };
		int ret = recv(client_socket, buffer, 1024, 0);
		if (ret <= 0)break;
		printf("%llu: %s\n", client_socket, buffer);

		send(client_socket, buffer, (int)strlen(buffer), 0);
	}

	printf("Socket number:%llu has disconnected.\n", client_socket);
	// 关闭连接
	closesocket(client_socket);

	return 0;
}

int main()
{
	int port;
	printf("Please enter port:");
	scanf("%d", &port);
	
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	// 初始化socket
	SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == listen_socket)
	{
		printf("Create listen socket failed. Error Code: %d\n", GetLastError());
		return -1;
	}
	else	
		printf("Create listen socket successed.\n");
	

	// 绑定端口号
	struct sockaddr_in local = { 0 };
	local.sin_family = AF_INET;
	local.sin_port = htons(port); // 大小端转换
	local.sin_addr.s_addr = inet_addr("0.0.0.0"); // 定义全0地址，全部接受

	if (-1 == bind(listen_socket, (struct sockaddr*)&local, sizeof(local)))
	{
		printf("Bind listen socket failed. Error Code: %d\n", GetLastError());
		return -1;
	}
	else	
		printf("Bind listen socket successed.\n");
	

	// 开启监听
	if (-1 == listen(listen_socket, 10))
	{
		printf("Start listen failed. Error Code: %d\n", GetLastError());
		return -1;
	}
	else	
		printf("Start listen successed.\n");
	

	// 等待客户端连接
	while (1)
	{
		SOCKET client_socket = accept(listen_socket, NULL, NULL);
		if (INVALID_SOCKET == client_socket)
			continue;

		printf("New connection has joined. Socket number:%llu\n", client_socket);

		SOCKET* sockfd = (SOCKET*)malloc(sizeof(SOCKET));
		*sockfd = client_socket;

		// 创建线程
		CreateThread(NULL, 0, thread_func, sockfd, 0, NULL);
	}
		
	return 0;
}