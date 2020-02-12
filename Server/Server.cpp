#include <global.hpp>
#include "ServerData.hpp"
#include "ServerUser.hpp"
#include "ServerChat.hpp"

bool g_isExit = 0;
vector<ServerThread> g_threads;

LOCK rwlock_msgSockets = LOCK_INIT;
map<int, SOCKET> g_msgSockets;

LOCK rwlock_onlineUsers = LOCK_INIT;
set<int> g_onlineUsers;

int cleanThread(void* thread) {
	ServerThread *sThread = (ServerThread*)thread;
	sThread->clean();
	return 0;
}

void *ClientMain(void* threadP) {
	ServerThread thread = *(ServerThread*)threadP;
	pthread_cleanup_push(cleanThread, &thread);
	
	bool isLogin = 0;
	bool isExit = 0;
	UserData currUser;
	DataHead head(0);
	
	int ret = SUCCESS;
	while (!isExit && ret == SUCCESS) {
		if ((ret = Recv(thread.client, head)) != SUCCESS) break;
		
		if (!isLogin) {
			switch (head.cmd) {
				case CMD_REGISTER:
					ret = Register(thread.client, thread.db);
					break;
				case CMD_LOGIN:
					ret = Login(thread.client, thread.db, currUser, isLogin);
					break;
				case CMD_LISTEN_MESSAGE:
					AddMessageSocket(thread.client);
					ret = ERROR_OTHER;
					break;
				default:
					ret = ERROR_OTHER;
					break;
			}
		}
		else {
			switch (head.cmd) {
				case CMD_NEW_MESSAGE:
					ret = GetChatMessage(thread.client, thread.db, currUser);
					break;
				case CMD_GET_ONLINE_USER:
					ret = SendOnlineUsers(thread.client, thread.db);
					break;
				case CMD_NEW_CHATROOM:
					ret = CreateChatroom(thread.client, thread.db, currUser);
					break;
				case CMD_GET_GROUP_CHATROOM:
					ret = GetGroupChatroom(thread.client, thread.db, currUser);
					break;
				case CMD_JOIN_CHATROOM:
					ret = JoinChatroom(thread.client, thread.db, currUser);
					break;
				case CMD_REJECT_INVITATION:
					ret = RejectInvitation(thread.client, thread.db, currUser);
					break;
				case CMD_SYNC_LASTSHOWN:
					ret = SyncLastShown(thread.client, thread.db, currUser);
					break;
				case CMD_QUIT_CHATROOM:
					ret = QuitChatroom(thread.client, thread.db, currUser);
					break;
				case CMD_UPDATE_CHATROOM:
					ret = UpdateChatroom(thread.client, thread.db, currUser);
					break;
				default:
					ret = ERROR_OTHER;
					break;
			}
		}
	}
	if (isLogin) {
		Logout(thread.client, thread.db, currUser);
	}
	pthread_cleanup_pop(1);
	return 0;
}

void *GetNewClient(void* threadP) {
	ServerThread thread = *(ServerThread*)threadP;
	pthread_cleanup_push(cleanThread, &thread);
	SOCKET g_listen = thread.client;
	
	while (true){
		sockaddr_in clientAddr = {0};
		int addrLen = sizeof(clientAddr);
		SOCKET clientSocket = accept(g_listen, (sockaddr *)&clientAddr, &addrLen);
		if (clientSocket == INVALID_SOCKET) {
			printf("accept() failed: %d\n", GetLastError());
			continue;
		}

		ServerThread clientThread;
		clientThread.client = clientSocket;
		pthread_create(&clientThread.pid, NULL, ClientMain, &clientThread);
		g_threads.push_back(clientThread);
	}
	pthread_cleanup_pop(1);
	return 0;
}

int main() {
	srand((unsigned int)time(NULL));
	int ret = 0;
	WSADATA wsaData;

	if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR){
		// stderr: standard error are printed to the screen.
		fprintf(stderr, "WSAStartup failed with error %d\n", WSAGetLastError());
		//WSACleanup function terminates use of the Windows Sockets DLL. 
		WSACleanup();
		return -1;
	}

	struct sockaddr_in local;
	// Fill in the address structure
	local.sin_family		= AF_INET;
	local.sin_addr.s_addr	= INADDR_ANY;
	local.sin_port			= htons(DEFAULT_PORT);

	SOCKET g_listen = socket(AF_INET, SOCK_STREAM, 0);	//TCp socket

	if (g_listen == INVALID_SOCKET){
		fprintf(stderr, "socket() failed with error %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	if (bind(g_listen, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR){
		fprintf(stderr, "bind() failed with error %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	if (listen(g_listen, 5) == SOCKET_ERROR){
		fprintf(stderr, "listen() failed with error %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}


	// Clean database at startup
	ServerThread threadAccept;
	threadAccept.db.query(" \
		delete from chatroom \
		where cid in ( \
			select * from ( \
				select T.cid \
				from chatroom as T left join user_in_chatroom \
				on T.cid = user_in_chatroom.cid \
				group by T.cid \
				having count(user_in_chatroom.uid) < 2 \
			) as temp \
		)");

	threadAccept.client = g_listen;
	printf("Server is listening at %d, to kill the server please press Q...\n", DEFAULT_PORT);
	pthread_create(&threadAccept.pid, NULL, GetNewClient, &threadAccept);
	char ch;
	while ((ch = getchar()) != NULL) {
		ch = tolower(ch);
		if (ch == 'q') {
			g_isExit = true;
			break;
		}
	}
	
	pthread_cancel(threadAccept.pid);
	// threadAccept.clean();
	for (int i = 0, len = g_threads.size(); i < len; ++i) {
		pthread_cancel(g_threads[i].pid);
		// g_threads[i].clean();
	}

	closesocket(g_listen);
	WSACleanup();
	return 0;
}