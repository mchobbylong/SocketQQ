#include "ServerUser.hpp"
#include "ServerChat.hpp"

UserData GetUserByName(DB &db, char *name) {
	UserData user;
	MYSQL_RES *res = db.query("select uid, name, pwd from user where name='%s'", name);
	if (mysql_num_rows(res) > 0) {
		MYSQL_ROW row = mysql_fetch_row(res);
		user.uid = atoi(row[0]);
		strcpy(user.name, row[1]);
		strcpy(user.pwd, row[2]);
	}
	mysql_free_result(res);
	return user;
}

bool IsRepeatName(DB &db, char *name) {
	UserData user = GetUserByName(db, name);
	return user.uid > 0;
}

int Register(SOCKET &socket, DB &db) {
	UserData user;
	int ret = SUCCESS;
	CheckRet(Recv(socket, user));

	DataHead res(CMD_REGISTER);

	if (IsRepeatName(db, user.name)) {
		res.response = RES_ERROR;
		ret = ERROR_OTHER;
	}
	else {
		res.response = RES_SUCCESS;
		char pwdbuf[MAXBUFF * 2 + 10];
		db.escape(user.pwd, pwdbuf);
		db.query("insert into user (name, pwd) values ('%s', '%s')", user.name, pwdbuf);
		// user = GetUserByName(db, user.name);
		printf("User %s is now registered.\n", user.name);
	}
	CheckRet(Send(socket, res));
	return ret;
}

int Login(SOCKET &socket, DB &db, UserData &currUser, bool &isLogin) {
	UserData user;
	int ret;
	CheckRet(Recv(socket, user));

	DataHead res(CMD_LOGIN);
	
	currUser = GetUserByName(db, user.name);
	if (currUser.uid > 0 && strcmp(user.pwd, currUser.pwd) == 0) {
		if (g_onlineUsers.count(currUser.uid)) {
			res.response = RES_ERROR;
			printf("User %s tries to duplicate login, rejected.\n", currUser.name);
		}
		else {
			isLogin = 1;
			res.response = RES_SUCCESS;

			AcquireWrite(rwlock_onlineUsers);
			g_onlineUsers.insert(currUser.uid);
			Release(rwlock_onlineUsers);

			printf("User %s is logged in.\n", currUser.name);
		}
	}
	else {
		res.response = RES_ERROR;
		if (currUser.uid > 0)
			printf("User %s tries to login, but failed.\n", currUser.name);
	}
	
	CheckRet(Send(socket, res));
	if (res.response == RES_SUCCESS) {
		CheckRet(Send(socket, currUser));
		CheckRet(PushUnreadCounts(socket, db, currUser));
	}
	return SUCCESS;
}

int Logout(SOCKET &socket, DB &db, UserData &currUser) {
	AcquireWrite(rwlock_onlineUsers);
	g_onlineUsers.erase(currUser.uid);
	Release(rwlock_onlineUsers);

	AcquireWrite(rwlock_msgSockets);
	if (g_msgSockets.count(currUser.uid)) {
		closesocket(g_msgSockets[currUser.uid]);
		g_msgSockets.erase(currUser.uid);
	}
	Release(rwlock_msgSockets);

	printf("User %s is log out.\n", currUser.name);
	return SUCCESS;
}

int SendOnlineUsers(SOCKET &socket, DB &db) {
	int ret;
	AcquireRead(rwlock_onlineUsers);

	int total = g_onlineUsers.size();
	DataHead head(CMD_GET_ONLINE_USER, RES_SUCCESS, total);
	CheckRet(Send(socket, head));

	UserData user;
	forVec(g_onlineUsers, user.uid) {
		MYSQL_RES *res = db.query("select name from user where uid=%d", user.uid);
		MYSQL_ROW row = mysql_fetch_row(res);
		strcpy(user.name, row[0]);
		mysql_free_result(res);
		CheckRet(Send(socket, user));
	}

	Release(rwlock_onlineUsers);
	return SUCCESS;
}
