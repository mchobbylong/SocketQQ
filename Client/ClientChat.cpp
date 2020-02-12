#include "ClientChat.hpp"
#include "ClientUser.hpp"

int OpenChatWindow() {
	Chatroom room;
	int ret;
	CheckRet(Recv(g_msgSocket, room));
	
	MessageWindow window(room, g_me);
	window.open();
	AcquireWrite(rwlock_chatWindows);
	g_chatWindows[room.cid] = window;
	Release(rwlock_chatWindows);

	DataHead head(CMD_NULL);
	CheckRet(Recv(g_msgSocket, head));
	return SUCCESS;
}

int UpdateChatroom() {
	Chatroom room;
	int ret;
	CheckRet(Recv(g_msgSocket, room));

	AcquireWrite(rwlock_chatWindows);
	if (g_chatWindows.count(room.cid))
		g_chatWindows[room.cid].updateRoom(room);
	Release(rwlock_chatWindows);
	return SUCCESS;
}

int CloseChatWindow(int _cid) {
	int cid = _cid > 0 ? _cid : GetNum("Input the chatroom id you want to close: ");
	int ret = SUCCESS;
	AcquireWrite(rwlock_chatWindows);
	if (g_chatWindows.count(cid)) {
		g_chatWindows[cid].close();
		DataHead head(CMD_SYNC_LASTSHOWN);
		CheckRet(Send(g_serverSocket, head));
		CheckRet(Send(g_serverSocket, g_chatWindows[cid].room));
		g_chatWindows.erase(cid);
	}
	else puts("This chatroom is not available!");
	Release(rwlock_chatWindows);
	return SUCCESS;
}

int DisplayMessage() {
	ChatMessage msg;
	int ret;
	CheckRet(Recv(g_msgSocket, msg));

	AcquireRead(rwlock_chatWindows);
	if (g_chatWindows.count(msg.cid)) {
		g_chatWindows[msg.cid].printMessage(msg);
	}
	Release(rwlock_chatWindows);
	return SUCCESS;
}

int SendChatMessage(int cid, char* msg) {
	ChatMessage Msg(cid, g_me.uid, g_me.name, msg);

	DataHead head(CMD_NEW_MESSAGE);
	int ret;
	CheckRet(Send(g_serverSocket, head));
	CheckRet(Send(g_serverSocket, Msg));
	CheckRet(Recv(g_serverSocket, head));
	if (head.response != RES_SUCCESS)
		puts("You are not in the chatroom!");
	return SUCCESS;
}

int ReceiveUnreadCounts() {
	printf("Your unread messages: ");
	DataHead head(CMD_NULL);
	int ret;
	bool hasUnread = 0;

	CheckRet(Recv(g_serverSocket, head));
	while (head.response == RES_SUCCESS) {
		hasUnread = 1;
		Chatroom room;
		CheckRet(Recv(g_serverSocket, room));
		printf("\n- [%d] %s: %d unread message(s)", room.cid, room.name, head.dataLen);
		CheckRet(Recv(g_serverSocket, head));
	}

	if (!hasUnread) printf("None");
	printf("\n");
	return SUCCESS;
}

int AddUserInChat(UserData &user) {
	int ret;
	DataHead head(CMD_ADD_USER_INTO_CHAT, RES_SUCCESS);
	CheckRet(Send(g_serverSocket, head));
	CheckRet(Send(g_serverSocket, user));
	CheckRet(Recv(g_serverSocket, head));
	if (head.response == RES_SUCCESS) {
		printf("Add user \"%s\" success!\n", user.name);
		return SUCCESS;
	}
	printf("Add user \"%s\" failed! (either the user is not found or is not online)\n", user.name);
	return ERROR_OTHER;
}

int OpenPrivateChat() {
	int ret;

	Chatroom room;
	room.masterUID = g_me.uid;
	room.type = 0;

	DataHead head(CMD_NEW_CHATROOM, RES_NULL, 1);
	CheckRet(Send(g_serverSocket, head));
	CheckRet(Send(g_serverSocket, room));

	UserData user;
	bool isFound = 0, isAbort = 0;
	do {
		GetName("\nInput the name of one online user you want to chat with\n(\"~\" to list online users, \"!\" to abort): ", user.name, "~!");
		if (strcmp(user.name, "!") == 0)
			isAbort = 1;
		else if (strcmp(user.name, "~") == 0) {
			CheckRet(GetOnlineUser());
		}
		else {
			isFound = AddUserInChat(user) == SUCCESS;
		}
	} while (!isFound && !isAbort);
	
	if (isAbort) {
		head = DataHead(CMD_ABORT_NEW_CHATROOM);
		CheckRet(Send(g_serverSocket, head));
		return SUCCESS;
	}

	// Announce to Server that user sending is finished
	head = DataHead(CMD_ADD_USER_INTO_CHAT, RES_NULL);
	CheckRet(Send(g_serverSocket, head));

	CheckRet(Recv(g_serverSocket, head));
	if (head.response == RES_SUCCESS)
		puts("Private chat creation is completed!");
	else
		puts("Private chat creation is failed!");
	return SUCCESS;
}

int OpenGroupChat() {
	int ret;

	// Get the existing group chatroom from Server
	printf("You are in the following group chats: ");
	DataHead head(CMD_GET_GROUP_CHATROOM);
	CheckRet(Send(g_serverSocket, head));
	CheckRet(Recv(g_serverSocket, head));
	bool hasChat = 0;
	while (head.response == RES_SUCCESS) {
		hasChat = 1;
		Chatroom existRoom;
		CheckRet(Recv(g_serverSocket, existRoom));
		printf("\n- [%d] %s", existRoom.cid, existRoom.name);
		CheckRet(Recv(g_serverSocket, head));
	}
	if (!hasChat) printf("None");
	printf("\n\n");

	head = DataHead(CMD_NEW_CHATROOM);
	Chatroom room;
	room.cid = GetNum("Input the ID of one group chat, or 0 to create a new one: ");
	if (room.cid > 0) {
		CheckRet(Send(g_serverSocket, head));
		CheckRet(Send(g_serverSocket, room));
		CheckRet(Recv(g_serverSocket, head));
		if (head.response != RES_SUCCESS)
			puts("Failed to enter this chatroom! (Did you input the correct id?)");
		return SUCCESS;
	}

	room.masterUID = g_me.uid;
	room.type = 1;
	GetName("Input the name of your new chatroom: ", room.name, " ");

	CheckRet(Send(g_serverSocket, head));
	CheckRet(Send(g_serverSocket, room));

	UserData user;
	bool isFound = 0, isAbort = 0;
	do {
		GetName("\nInput the name of one online user you want to chat with\n(\"~\" to list online users, \"!\" to abort, \".\" to finish): ", user.name, "~!.");
		if (strcmp(user.name, "!") == 0)
			isAbort = 1;
		else if (strcmp(user.name, "~") == 0) {
			CheckRet(GetOnlineUser());
		}
		else if (strcmp(user.name, ".") == 0) {
			isFound = 1;
		}
		else
			AddUserInChat(user);
	} while (!isFound && !isAbort);

	if (isAbort) {
		head = DataHead(CMD_ABORT_NEW_CHATROOM);
		CheckRet(Send(g_serverSocket, head));
		return SUCCESS;
	}

	// Announce to Server that the user sending is finished
	head = DataHead(CMD_ADD_USER_INTO_CHAT, RES_NULL);
	CheckRet(Send(g_serverSocket, head));

	CheckRet(Recv(g_serverSocket, head));
	if (head.response == RES_SUCCESS)
		puts("Group chat creation is completed!");
	else
		puts("Group chat creation is failed!");
	return SUCCESS;
}

int DealInvitation() {
	Chatroom room;
	int cid = GetNum("Input the chatroom ID you want to accept: ");
	AcquireRead(rwlock_chatWindows);
	if (!g_chatWindows.count(cid)) {
		puts("This chatroom is not available!");
		cid = -1;
	}
	else room = g_chatWindows[cid].room;
	Release(rwlock_chatWindows);

	if (cid < 0) return SUCCESS;

	int ret;
	bool ans = GetYN("Do you want to accept this invitaion?");
	if (!ans) {
		DataHead head(CMD_REJECT_INVITATION);
		CheckRet(Send(g_serverSocket, head));
		CheckRet(Send(g_serverSocket, room));
		AcquireWrite(rwlock_chatWindows);
		g_chatWindows[cid].close();
		g_chatWindows.erase(cid);
		Release(rwlock_chatWindows);
		return SUCCESS;
	}

	DataHead head(CMD_JOIN_CHATROOM);
	CheckRet(Send(g_serverSocket, head));
	CheckRet(Send(g_serverSocket, room));
	CheckRet(Recv(g_serverSocket, head));
	switch (head.response) {
		case RES_SUCCESS:
			puts("You have accepted the invitation and joined the chatroom!");
			break;
		case RES_PWD_ERROR:
			puts("Error password! The invitation may be expired.");
			break;
		case RES_NULL:
			puts("This chatroom is no longer available!");
			break;
		case RES_ERROR:
			puts("You are already in this chatroom!");
			break;
	}
	return SUCCESS;
}

int QuitChatroom(int cid) {
	Chatroom room;
	AcquireRead(rwlock_chatWindows);
	room = g_chatWindows[cid].room;
	Release(rwlock_chatWindows);
	
	printf("Attention: You are going to QUIT chatroom:\n [%d]: %s\n", room.cid, room.name);
	bool quit = GetYN("Do you really want to quit this chatroom?");
	if (!quit) return SUCCESS;
	
	int ret;
	DataHead head(CMD_QUIT_CHATROOM);
	CheckRet(Send(g_serverSocket, head));
	CheckRet(Send(g_serverSocket, cid));
	CheckRet(Recv(g_serverSocket, head));
	if (head.response == RES_SUCCESS) {
		puts("Successfully quitted!");
		AcquireWrite(rwlock_chatWindows);
		g_chatWindows[cid].close();
		g_chatWindows.erase(cid);
		Release(rwlock_chatWindows);
	}
	else
		puts("You are already not in this chatroom!");
	return SUCCESS;
}

int ChangeChatroomName(int cid) {
	Chatroom room;
	AcquireRead(rwlock_chatWindows);
	room = g_chatWindows[cid].room;
	Release(rwlock_chatWindows);
	if (room.masterUID != g_me.uid) {
		puts("You are not the master of the chatroom!");
		return SUCCESS;
	}
	if (room.type == 0) {
		puts("A private chat does not have a room name!");
		return SUCCESS;
	}

	GetName("Input the new name of this chatroom: ", room.name, " ");
	DataHead head(CMD_UPDATE_CHATROOM);
	Send(g_serverSocket, head);
	Send(g_serverSocket, room);
	Recv(g_serverSocket, head);
	if (head.response == RES_SUCCESS)
		puts("The name of chatroom is successfully changed!");
	else
		puts("Name changing is failed! (Maybe because you are not the admin of this chatroom)");
	return SUCCESS;
}

int ChatroomManageMenu() {
	int cid = GetNum("Input the chatroom ID you want to manage: ");
	bool hasRoom;
	// Check whether this chatroom exists
	AcquireRead(rwlock_chatWindows);
	hasRoom = g_chatWindows.count(cid) > 0;
	Release(rwlock_chatWindows);
	if (!hasRoom) {
		puts("You need to open this chatroom for further management!");
		return SUCCESS;
	}

	puts("");
	puts("+---------------------------------------+");
	puts("|         Chatroom Management           |");
	puts("+---------------------------------------+");
	puts("|  0. Back to main menu                 |");
	puts("|  1. Quit chatroom                     |");
	puts("|  2. Change chatroom name (Admin)      |");
	puts("+---------------------------------------+");
	
	int ret = SUCCESS;
	while (ret == SUCCESS) {
		int cmd = GetNum("\nChoose your management action: ");
		switch (cmd) {
			case 0:
				return SUCCESS;
			case 1:
				CheckRet(QuitChatroom(cid));
				break;
			case 2:
				CheckRet(ChangeChatroomName(cid));
				break;
			default:
				puts("Wrong choice!");
		}

		// Check whether this chatroom exists
		AcquireRead(rwlock_chatWindows);
		hasRoom = g_chatWindows.count(cid) > 0;
		Release(rwlock_chatWindows);
		if (!hasRoom)
			return SUCCESS;
	}
	return ret;
}
