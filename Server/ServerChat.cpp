#include "ServerChat.hpp"
#include "ServerUser.hpp"

int AddMessageSocket(SOCKET &client) {
	int ret = SUCCESS;
	UserData user;
	CheckRet(Recv(client, user));
	// TODO: verify user
	AcquireWrite(rwlock_msgSockets);
	g_msgSockets[user.uid] = client;
	Release(rwlock_msgSockets);
	client = INVALID_SOCKET;
	return SUCCESS;
}

int OpenChatroom(SOCKET &clientMsg, Chatroom &room) {
	DataHead head(CMD_NEW_CHATROOM);
	int ret;
	CheckRet(Send(clientMsg, head));
	CheckRet(Send(clientMsg, room));
	head.cmd = CMD_NULL;
	CheckRet(Send(clientMsg, head));
	return SUCCESS;
}

int SyncLastShown(SOCKET &client, DB &db, UserData &currUser) {
	Chatroom room;
	int ret;
	CheckRet(Recv(client, room));
	db.query("update user_in_chatroom set lastShown=current_timestamp where cid=%d and uid=%d", room.cid, currUser.uid);
	return SUCCESS;
}

UserInChat GetUserInChat(DB &db, int uid, int cid) {
	UserInChat ret(uid, cid);
	MYSQL_RES *res = db.query("select lastShown from user_in_chatroom where uid=%d and cid=%d", uid, cid);
	if (mysql_num_rows(res)) {
		MYSQL_ROW row = mysql_fetch_row(res);
		strcpy(ret.lastShown, row[0]);
	}
	else ret.uid = ret.cid = 0;
	return ret;
}

void SendChatMessage(int uid, ChatMessage msg) {
	DataHead head(CMD_BROADCAST_MESSAGE);
	AcquireRead(rwlock_msgSockets);
	if (g_msgSockets.count(uid)) {
		Send(g_msgSockets[uid], head);
		Send(g_msgSockets[uid], msg);
	}
	Release(rwlock_msgSockets);
}

void AddUserIntoChatroom(DB &db, int uid, int cid) {
	db.query("insert into user_in_chatroom (uid, cid) values (%d, %d)", uid, cid);
}

Chatroom GetChatroomByCID(DB &db, int cid, int uid) {
	MYSQL_RES *res = db.query("select cid, masterUID, name, pwd, type from chatroom where cid=%d", cid);
	MYSQL_ROW row;
	Chatroom room;
	if (mysql_num_rows(res) > 0) {
		row = mysql_fetch_row(res);
		room.cid = atoi(row[0]);
		room.masterUID = atoi(row[1]);
		strcpy(room.name, row[2]);
		strcpy(room.pwd, row[3]);
		room.type = atoi(row[4]);
	}
	mysql_free_result(res);

	// Create name for private chatroom
	if (room.type == 0) {
		res = db.query("select name from user_in_chatroom natural join user where cid=%d and uid<>%d", cid, uid);
		if (mysql_num_rows(res) > 0) {
			row = mysql_fetch_row(res);
			sprintf(room.name, STR_PRIVATE_ROOM_TITLE, row[0]);
		}
		else sprintf(room.name, STR_PRIVATE_ROOM_TITLE, "nobody");
		mysql_free_result(res);
	}
	return room;
}

int OpenExistingChatroom(SOCKET &client, DB &db, UserData &currUser, int cid) {
	int ret;
	Chatroom room = GetChatroomByCID(db, cid, currUser.uid);

	// Get unread message
	UserInChat relation = GetUserInChat(db, currUser.uid, cid);
	
	vector<ChatMessage> unreads;
	MYSQL_RES *res = db.query("select fromUID, fromName, msg, time from chatmessage where cid=%d and time>='%s'", cid, relation.lastShown);
	MYSQL_ROW row;
	while (row = mysql_fetch_row(res))
		unreads.push_back(ChatMessage(cid, atoi(row[0]), row[1], row[2], row[3]));
	mysql_free_result(res);

	// Broadcast unread message
	GetCurrentDatetime;
	AcquireRead(rwlock_msgSockets);
	CheckRet(OpenChatroom(g_msgSockets[currUser.uid], room));
	Sprintf(msg, STR_BACK_TO_CHATROOM, unreads.size());
	SendChatMessage(currUser.uid, ChatMessage(cid, 0, STR_SYSTEM_NAME, msg, currentTime));
	ChatMessage Msg;
	forVec(unreads, Msg)
		SendChatMessage(currUser.uid, Msg);
	Release(rwlock_msgSockets);

	return SUCCESS;
}

int CreateChatroom(SOCKET &client, DB &db, UserData &currUser) {
	int ret = SUCCESS;
	DataHead resp(CMD_NULL);
	Chatroom room;
	CheckRet(Recv(client, room));
	
	// Determine if the user wants to join an old group chat
	if (room.cid > 0) {
		UserInChat relation = GetUserInChat(db, currUser.uid, room.cid);
		resp.cmd = CMD_NEW_CHATROOM;
		if (relation.cid > 0) {
			resp.response = RES_SUCCESS;
			CheckRet(OpenExistingChatroom(client, db, currUser, room.cid));
		}
		else resp.response = RES_ERROR;
		CheckRet(Send(client, resp));
		return SUCCESS;
	}

	vector<UserData> users;
	UserData user;
	bool isAbort = 0, isFinish = 0;
	do {
		CheckRet(Recv(client, resp));
		switch (resp.cmd) {
			case CMD_ABORT_NEW_CHATROOM:
				isAbort = 1;
				break;
			case CMD_ADD_USER_INTO_CHAT:
				if (resp.response != RES_SUCCESS) {
					isFinish = 1;
					break;
				}

				CheckRet(Recv(client, user));

				// Check if the user exists, and if it is itself
				user = GetUserByName(db, user.name);
				if (user.uid == 0 || user.uid == currUser.uid) {
					resp = DataHead(CMD_ADD_USER_INTO_CHAT, RES_ERROR);
					CheckRet(Send(client, resp));
					break;
				}

				// If it is a private chat, judge whether there is a group chat created before
				if (room.type == 0) {
					MYSQL_RES *res = db.query("\
						select cid \
						from chatroom natural join user_in_chatroom \
						where type=0 and uid=%d and cid in ( \
							select cid \
							from chatroom natural join user_in_chatroom \
							where type=0 and uid=%d \
						)", currUser.uid, user.uid);
					if (mysql_num_rows(res)) {
						resp = DataHead(CMD_ADD_USER_INTO_CHAT, RES_SUCCESS);
						CheckRet(Send(client, resp));

						MYSQL_ROW row = mysql_fetch_row(res);
						room.cid = atoi(row[0]);
						mysql_free_result(res);
						break;
					}
					mysql_free_result(res);
				}

				// Judge if the user is online
				AcquireRead(rwlock_onlineUsers);
				if (!g_onlineUsers.count(user.uid))
					resp = DataHead(CMD_ADD_USER_INTO_CHAT, RES_ERROR);
				else {
					resp = DataHead(CMD_ADD_USER_INTO_CHAT, RES_SUCCESS);
					users.push_back(user);
				}
				CheckRet(Send(client, resp));
				Release(rwlock_onlineUsers);

				break;
			case CMD_GET_ONLINE_USER:
				CheckRet(SendOnlineUsers(client, db));
				break;
			default:
				printf("User %s send a wrong head in CreateChatroom().\n", currUser.name);
		}
	} while (!isAbort && !isFinish);

	if (isAbort) return SUCCESS;
	
	if (room.cid > 0) {
		resp = DataHead(CMD_NEW_CHATROOM, RES_SUCCESS);
		CheckRet(Send(client, resp));
		return OpenExistingChatroom(client, db, currUser, room.cid);
	}

	generate_password(room.pwd);
	db.query("insert into chatroom (type, name, masterUID, pwd) values (%d, '%s', %d, '%s')", room.type, room.name, room.masterUID, room.pwd);

	MYSQL_RES *res = db.query("select last_insert_id()");
	MYSQL_ROW row = mysql_fetch_row(res);
	room.cid = atoi(row[0]);
	mysql_free_result(res);

	AddUserIntoChatroom(db, currUser.uid, room.cid);

	resp = DataHead(CMD_NEW_CHATROOM, RES_SUCCESS);
	CheckRet(Send(client, resp));

	AcquireRead(rwlock_msgSockets);

	// Open the chat window of a group owner
	if (room.type == 0)
		sprintf(room.name, STR_PRIVATE_ROOM_TITLE, user.name);
	CheckRet(OpenChatroom(g_msgSockets[currUser.uid], room));

	// Open the invitee chat window
	if (room.type == 0)
		sprintf(room.name, STR_PRIVATE_ROOM_TITLE, currUser.name);
	Sprintf(inviteMsg, STR_RECEIVE_INVITATION, currUser.name);
	GetCurrentDatetime;
	forVec(users, user) {
		CheckRet(OpenChatroom(g_msgSockets[user.uid], room));
		SendChatMessage(user.uid, ChatMessage(room.cid, 0, STR_SYSTEM_NAME, inviteMsg, currentTime));
		Sprintf(msgg, STR_INVITE_SUCCESS, user.name);
		SendChatMessage(currUser.uid, ChatMessage(room.cid, 0, STR_SYSTEM_NAME, msgg, currentTime));
	}

	Release(rwlock_msgSockets);
	return SUCCESS;
}

void BroadcastMessage(DB &db, ChatMessage msg) {
	char msgbuf[MAXBUFF * 2 + 10];
	db.escape(msg.msg, msgbuf);
	db.query("insert into chatmessage (cid, fromUID, fromName, msg) values (%d, %d, '%s', '%s')", msg.cid, msg.fromUID, msg.fromName, msgbuf);
	GetCurrentDatetime;
	strcpy(msg.time, currentTime);
	MYSQL_RES *res = db.query("select uid from user_in_chatroom where cid=%d", msg.cid);
	MYSQL_ROW row;
	while (row = mysql_fetch_row(res)) {
		int uid = atoi(row[0]);
		SendChatMessage(uid, msg);
	}
	mysql_free_result(res);
}

int GetChatMessage(SOCKET &client, DB &db, UserData &currUser) {
	ChatMessage msg;
	int ret;
	CheckRet(Recv(client, msg));
	msg.fromUID = currUser.uid;
	strcpy(msg.fromName, currUser.name);
	DataHead head(CMD_NEW_MESSAGE);
	
	// Check if this user is in the group
	UserInChat relation = GetUserInChat(db, currUser.uid, msg.cid);
	if (!relation.cid) {
		head.response = RES_ERROR;
		CheckRet(Send(client, head));
		return SUCCESS;
	}

	BroadcastMessage(db, msg);
	head.response = RES_SUCCESS;
	CheckRet(Send(client, head));
	return SUCCESS;
}

int JoinChatroom(SOCKET &client, DB &db, UserData &currUser) {
	int ret;
	Chatroom room;
	CheckRet(Recv(client, room));
	
	DataHead head(CMD_JOIN_CHATROOM);
	
	Chatroom currRoom = GetChatroomByCID(db, room.cid, currUser.uid);
	if (currRoom.cid == 0)
		head.response = RES_NULL;
	else if (strcmp(currRoom.pwd, room.pwd) != 0)
		head.response = RES_PWD_ERROR;
	else {
		UserInChat relation = GetUserInChat(db, currUser.uid, room.cid);
		if (relation.cid)
			head.response = RES_ERROR;
		else {
			AddUserIntoChatroom(db, currUser.uid, room.cid);
			Sprintf(msg, STR_USER_JOIN_CHATROOM, currUser.name);
			BroadcastMessage(db, ChatMessage(room.cid, 0, STR_SYSTEM_NAME, msg));
			head.response = RES_SUCCESS;
		}
	}
	
	return Send(client, head);
}

int RejectInvitation(SOCKET &client, DB &db, UserData &currUser) {
	Chatroom room;
	int ret;
	CheckRet(Recv(client, room));
	Sprintf(msg, STR_INVITE_REJECTED, currUser.name);
	GetCurrentDatetime;
	SendChatMessage(room.masterUID, ChatMessage(room.cid, 0, STR_SYSTEM_NAME, msg, currentTime));
	return SUCCESS;
}

int PushUnreadCounts(SOCKET &client, DB &db, UserData &currUser) {
	vector<int> cids;
	MYSQL_RES *res = db.query("select cid from user_in_chatroom where uid=%d", currUser.uid);
	MYSQL_ROW row;
	while (row = mysql_fetch_row(res))
		cids.push_back(atoi(row[0]));
	mysql_free_result(res);

	// Scan chat rooms, count and push the number of unread messages
	int cid, ret;
	DataHead head(CMD_PUSH_UNREAD, RES_SUCCESS);
	forVec(cids, cid) {
		UserInChat relation = GetUserInChat(db, currUser.uid, cid);
		res = db.query("select count(*) from chatmessage where cid=%d and time>='%s'", cid, relation.lastShown);
		row = mysql_fetch_row(res);
		int count = atoi(row[0]);
		mysql_free_result(res);
		if (count) {
			Chatroom room = GetChatroomByCID(db, cid, currUser.uid);
			head.dataLen = count;
			CheckRet(Send(client, head));
			CheckRet(Send(client, room));
		}
	}

	// Announce Client that the pushing is finished
	head.response = RES_NULL;
	CheckRet(Send(client, head));
	return SUCCESS;
}

int GetGroupChatroom(SOCKET &client, DB &db, UserData &currUser) {
	int ret;
	MYSQL_RES *res = db.query("select cid, name from chatroom natural join user_in_chatroom where type=1 and uid=%d", currUser.uid);
	MYSQL_ROW row;
	DataHead head(CMD_GET_GROUP_CHATROOM, RES_SUCCESS);
	while (row = mysql_fetch_row(res)) {
		Chatroom room;
		room.cid = atoi(row[0]);
		strcpy(room.name, row[1]);
		CheckRet(Send(client, head));
		CheckRet(Send(client, room));
	}
	head.response = RES_NULL;
	CheckRet(Send(client, head));
	return SUCCESS;
}

void BroadcastChatroom(DB &db, Chatroom &room) {
	DataHead head(CMD_UPDATE_CHATROOM);
	MYSQL_RES *res = db.query("select uid from user_in_chatroom where cid=%d", room.cid);
	MYSQL_ROW row;
	vector<int> uids;
	while (row = mysql_fetch_row(res))
		uids.push_back(atoi(row[0]));
	mysql_free_result(res);

	int uid;
	AcquireRead(rwlock_msgSockets);
	forVec(uids, uid) {
		if (!g_msgSockets.count(uid)) continue;
		Send(g_msgSockets[uid], head);
		Send(g_msgSockets[uid], room);
	}
	Release(rwlock_msgSockets);
}

int QuitChatroom(SOCKET &client, DB &db, UserData &currUser) {
	int cid, ret;
	CheckRet(Recv(client, cid));
	db.query("delete from user_in_chatroom where cid=%d and uid=%d", cid, currUser.uid);
	bool isQuit = mysql_affected_rows(db.db) > 0;
	
	DataHead head(CMD_QUIT_CHATROOM, isQuit ? RES_SUCCESS : RES_NULL);
	CheckRet(Send(client, head));
	
	if (isQuit) {
		GetCurrentDatetime;
		Sprintf(msg, STR_USER_QUIT_CHATROOM, currUser.name);
		BroadcastMessage(db, ChatMessage(cid, 0, STR_SYSTEM_NAME, msg, currentTime));
		
		Chatroom room = GetChatroomByCID(db, cid, currUser.uid);
		if (room.masterUID == currUser.uid) {
			UserData newMaster;
			MYSQL_RES *res = db.query("select user.uid, user.name from user_in_chatroom join user on user_in_chatroom.uid=user.uid where cid=%d limit 1", cid);
			if (mysql_num_rows(res)) {
				MYSQL_ROW row = mysql_fetch_row(res);
				newMaster.uid = atoi(row[0]);
				strcpy(newMaster.name, row[1]);
			}
			mysql_free_result(res);
			
			if (newMaster.uid > 0) {
				db.query("update chatroom set masterUID=%d where cid=%d", newMaster.uid, cid);
				room.masterUID = newMaster.uid;
				BroadcastChatroom(db, room);
				GetCurrentDatetime;
				Sprintf(msg, STR_NEW_ROOM_MASTER, newMaster.name);
				BroadcastMessage(db, ChatMessage(cid, 0, STR_SYSTEM_NAME, msg, currentTime));
			}
		}
	}
	return SUCCESS;
}

int UpdateChatroom(SOCKET &client, DB &db, UserData &currUser) {
	Chatroom room, existRoom;
	int ret;
	CheckRet(Recv(client, room));
	existRoom = GetChatroomByCID(db, room.cid, currUser.uid);
	DataHead head(CMD_UPDATE_CHATROOM);
	if (existRoom.masterUID != currUser.uid) {
		CheckRet(Send(client, head));
		return SUCCESS;
	}

	db.query("update chatroom set name='%s', masterUID=%d, pwd='%s' where cid=%d", room.name, room.masterUID, room.pwd, room.cid);
	head.response = RES_SUCCESS;
	CheckRet(Send(client, head));
	BroadcastChatroom(db, room);
	return SUCCESS;
}
