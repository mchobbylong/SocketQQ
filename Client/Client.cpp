#include <global.hpp>
#include "ClientData.hpp"
#include "ClientUser.hpp"
#include "ClientChat.hpp"

sockaddr_in		server_addr;
hostent			*hp;
char			*server_name = "localhost";
unsigned short	port = DEFAULT_PORT;
unsigned int	addr;

SOCKET			g_serverSocket = INVALID_SOCKET;
SOCKET			g_msgSocket = INVALID_SOCKET;
UserData		g_me;
bool			g_isLogin = 0;
bool			g_isExit = 0;

// LOCK rwlock_chatrooms = LOCK_INIT;
// map<int, Chatroom> g_chatrooms;

LOCK rwlock_chatWindows = LOCK_INIT;
map<int, MessageWindow> g_chatWindows;

void CloseSocket(void *socket) {
	SOCKET sock = *(SOCKET*)socket;
	if (sock != INVALID_SOCKET) closesocket(sock);
}

SOCKET NewServerSocket() {
	SOCKET ret = socket(AF_INET, SOCK_STREAM, 0);	//TCP socket

	if (ret == INVALID_SOCKET){
		fprintf(stderr, "socket() failed with error %d\n", WSAGetLastError());
		return INVALID_SOCKET;
	}

	if (connect(ret, (struct sockaddr *)&server_addr, sizeof(server_addr))
		== SOCKET_ERROR) {
		fprintf(stderr, "connect() failed with error %d\n", WSAGetLastError());
		return INVALID_SOCKET;
	}
	
	return ret;
}

void *RecvMessage(void* NOTHING) {
	pthread_cleanup_push(closesocket, (void*)g_msgSocket);
	g_msgSocket = NewServerSocket();
	DataHead head(CMD_LISTEN_MESSAGE);
	if (Send(g_msgSocket, head) != SUCCESS || Send(g_msgSocket, g_me) != SUCCESS) {
		closesocket(g_msgSocket);
		return 0;
	}

	while (1) {
		if (Recv(g_msgSocket, head) != SUCCESS) break;

		switch (head.cmd) {
			case CMD_NEW_CHATROOM:
				OpenChatWindow();
				break;
			case CMD_BROADCAST_MESSAGE:
				DisplayMessage();
				break;
			case CMD_UPDATE_CHATROOM:
				UpdateChatroom();
				break;
		}
	}
	pthread_cleanup_pop(1);
	return 0;
}

void PrintMenu() {
	puts("");
	puts("+-------------------------------+");
	puts("|             Menu              |");
	puts("+-------------------------------+");
	puts("| >>. Send message              |");
	puts("|  1. Private chat              |");
	puts("|  2. Group chat                |");
	puts("|  3. Get online users          |");
	puts("|  4. Accept/reject invitation  |");
	puts("|  5. Chatroom management       |");
	puts("|  6. Close chat window         |");
	puts("|  7. Logout and exit           |");
	puts("+-------------------------------+");
	puts("  Tips:");
	puts("    - Input \">> ChatroomID Message\" to send messages quickly!");
	puts("    - Input \"~\" to see this menu again.");
}

void Menu() {
	pthread_t msgThread;
	int ret = SUCCESS;
	while (!g_isExit && ret == SUCCESS && !g_isLogin) {
		int option = GetNum("\nDo you want to login (1), register (2), or exit (3)? [1/2/3] ");

		switch (option) {
			case 1:
				ret = Login();
				break;
			case 2:
				ret = Register();
				break;
			case 3:
				g_isExit = 1;
				break;
			default:
				printf("Wrong choice!\n");
				break;
		}
	}

	if (ret == SUCCESS && g_isLogin) {
		// change title
		Sprintf(CONSOLE_TITLE, "You are \"%s\"", g_me.name);
		SetConsoleTitle(CONSOLE_TITLE);
		// create message listener thread
		pthread_create(&msgThread, NULL, RecvMessage, NULL);

		// menu
		system("cls");
		printf("You login the system as: %s\n\n", g_me.name);
		// recieve counts of unread messages
		ReceiveUnreadCounts();
		PrintMenu();
		while (ret == SUCCESS && g_isLogin) {
			printf("\nType your command: ");
			char cmdStr[MAXBUFF], subStr[MAXBUFF] = "", msg[MAXBUFF] = "", *pcmdStr = cmdStr, *psubStr = subStr;
			gets(cmdStr);

			// Check if sending message by ">>"
			while (*pcmdStr != 0 && *pcmdStr == ' ') ++pcmdStr;
			sscanf(pcmdStr, "%s", subStr);
			int cmd = -1, subLen = strlen(subStr), cid = -1;
			pcmdStr += subLen;
			if (strlen(subStr) > 1 && subStr[0] == '>' && subStr[1] == '>') {
				strcat(subStr, pcmdStr);
				psubStr += 2;
				while (*psubStr != 0 && *psubStr == ' ') ++psubStr;
				sscanf(psubStr, "%s", msg);
				cid = atoi(msg);
				if (cid < 1) {
					puts("This chatroom is not available!");
					continue;
				}

				psubStr += strlen(msg);
				while (*psubStr != 0 && *psubStr == ' ') ++psubStr;
				strcpy(msg, psubStr);
				if (strlen(msg) == 0) {
					puts("Empty chat message is not allowed!");
					continue;
				}
				
				ret = SendChatMessage(cid, msg);
				// If the window should be closed
				if (ret == SUCCESS && strcmp(msg, "Bye") == 0)
					ret = CloseChatWindow(cid);
				continue;
			}
			else if (strlen(subStr) > 0 && subStr[0] == '~') {
				PrintMenu();
				continue;
			}
			cmd = atoi(subStr);
			
			switch (cmd) {
				case 1:
					ret = OpenPrivateChat();
					break;
				case 2:
					ret = OpenGroupChat();
					break;
				case 3:
					ret = GetOnlineUser();
					break;
				case 4:
					ret = DealInvitation();
					break;
				case 5:
					ret = ChatroomManageMenu();
					break;
				case 6:
					ret = CloseChatWindow();
					break;
				case 7:
					ret = Logout();
					break;
				default:
					puts("Wrong input!");
					break;
			}
		}
		pthread_cancel(msgThread);
	}
	if (ret != SUCCESS) {
		// error processing
	}
}

int main(int argc, char **argv) {
	int ret = 0;

	WSADATA wsaData;

	if (argc != 3) {
		printf("Server IP and Port are not defined, using default '%s' and '%d'\n", server_name, port);
	}
	else {
		server_name = argv[1];
		port = atoi(argv[2]);
	}

	if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR){
		// stderr: standard error are printed to the screen.
		fprintf(stderr, "WSAStartup failed with error %d\n", WSAGetLastError());
		//WSACleanup function terminates use of the Windows Sockets DLL. 
		WSACleanup();
		return -1;
	}

	if (isalpha(server_name[0]))
		hp = gethostbyname(server_name);
	else {
		addr = inet_addr(server_name);
		hp = gethostbyaddr((char*)&addr, 4, AF_INET);
	}

	if (hp == NULL) {
		fprintf(stderr, "Cannot resolve address: %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}
	memset(&server_addr, 0, sizeof(server_addr));
	memcpy(&(server_addr.sin_addr), hp->h_addr, hp->h_length);
	server_addr.sin_family = hp->h_addrtype;
	server_addr.sin_port = htons(port);

	printf("Client connecting to: %s\n", hp->h_name);
	g_serverSocket = NewServerSocket();
	if (g_serverSocket != INVALID_SOCKET) {
		Menu();
		closesocket(g_serverSocket);
	}
	else
		puts("Fatal: error connecting to the server!");

	WSACleanup();

	// Clean chat windows
	int cid;
	MessageWindow window;
	forMap(g_chatWindows, cid, window)
		window.close();
	printf("Bye!\n");
	return 0;
}