#include "ClientUser.hpp"

int Login() {
	Sprintf(nameQuestion, "Input username (not more than %d chars): ", USERNAME_LEN);
	GetName(nameQuestion, g_me.name);
	printf("Input your password (not more than %d chars): ", USERPWD_LEN);
	gets(g_me.pwd);

	int ret;
	DataHead head(CMD_LOGIN);
	CheckRet(Send(g_serverSocket, head));
	CheckRet(Send(g_serverSocket, g_me));
	CheckRet(Recv(g_serverSocket, head));
	if (head.response != RES_SUCCESS)
		ret = ERROR_OTHER;
	if (ret == SUCCESS) {
		// printf("Login success!\n");
		CheckRet(Recv(g_serverSocket, g_me));
		g_isLogin = 1;
	}
	else printf("Login failed!\n");
	return SUCCESS;
}

int Register() {
	Sprintf(nameQuestion, "Input username (not more than %d chars): ", USERNAME_LEN);
	GetName(nameQuestion, g_me.name);
	printf("Input your password (not more than %d chars): ", USERPWD_LEN);
	gets(g_me.pwd);

	int ret;
	DataHead head(CMD_REGISTER);
	CheckRet(Send(g_serverSocket, head));
	CheckRet(Send(g_serverSocket, g_me));
	CheckRet(Recv(g_serverSocket, head));
	if (head.response != RES_SUCCESS)
		ret = ERROR_OTHER;
	if (ret == SUCCESS)
		printf("Register success!\n");
	else printf("Register failed!\n");
	return SUCCESS;
}

int Logout() {
	int ret;
	DataHead head(CMD_SYNC_LASTSHOWN);
	AcquireWrite(rwlock_chatWindows);
	int cid;
	MessageWindow window;
	forMap(g_chatWindows, cid, window) {
		window.close();
		CheckRet(Send(g_serverSocket, head));
		CheckRet(Send(g_serverSocket, window.room));
	}
	Release(rwlock_chatWindows);

	head = DataHead(CMD_LOGOUT);
	CheckRet(Send(g_serverSocket, head));
	g_isLogin = 0;
	puts("You are logged out!");
	return SUCCESS;
}

int GetOnlineUser() {
	DataHead head(CMD_GET_ONLINE_USER);
	int ret;
	CheckRet(Send(g_serverSocket, head));
	CheckRet(Recv(g_serverSocket, head));
	
	int total = head.dataLen;
	printf("There are %d online user(s):\n", total);
	UserData user;
	while (total--) {
		CheckRet(Recv(g_serverSocket, user));
		printf("%4d: %s\n", user.uid, user.name);
	}
	return SUCCESS;
}
