#pragma once

#include <global.hpp>
#include <mysql.h>

#define STR_PRIVATE_ROOM_TITLE		"Chat with %s"
#define STR_RECEIVE_INVITATION		"User \"%s\" wants to chat with you, agree or not?"
#define STR_USER_NOT_EXIST			"Invited user \"%s\" does not exist!"
#define STR_USER_NOT_ONLINE			"Invited user \"%s\" is not online!"
#define STR_INVITE_SUCCESS			"User \"%s\" is successfully invited!"
#define STR_INVITE_REJECTED			"User \"%s\" has rejected your invitation."
#define STR_USER_JOIN_CHATROOM		"User \"%s\" has joined the chatroom."
#define STR_BACK_TO_CHATROOM		"Welcome back to the chatroom. Fetching %d unread message(s)..."
#define STR_USER_QUIT_CHATROOM		"User \"%s\" has quitted the chatroom."
#define STR_NEW_ROOM_MASTER			"User \"%s\" has become the new chatroom master!"

struct DB {
	MYSQL *db;
	
	DB() {
		db = mysql_init(NULL);
		my_bool reconnect = 1;
		mysql_optionsv(db, MYSQL_OPT_RECONNECT, (void *)&reconnect);
		mysql_set_character_set(db, "latin1");
		if (!mysql_real_connect(db, "localhost", "root", "", "qq", 3306, NULL, 0)) {
			printf("DB connection failed: %s\n", mysql_error(db));
		};
		mysql_autocommit(db, 1);
	}

	void ping() {	// keep db alive
		mysql_ping(db);
	}

	int escape(char *from, char *to) {
		return mysql_real_escape_string(db, to, from, strlen(from));
	}

	MYSQL_RES* query(char *sqlformat, ...) {
		va_list argList;
		va_start(argList, sqlformat);
		char sql[MAXBUFF];
		vsnprintf(sql, MAXBUFF, sqlformat, argList);
		va_end(argList);

		ping();
		int ret = mysql_real_query(db, sql, strlen(sql));
		if (ret) printf("DB query failed: %s\n%s\n", sql, mysql_error(db));
		return mysql_store_result(db);
	}

	void getTime(char *time) {
		MYSQL_RES *res = query("select current_timestamp()");
		MYSQL_ROW row = mysql_fetch_row(res);
		strcpy(time, row[0]);
		mysql_free_result(res);
	}

	void close() {
		mysql_close(db);
	}
};

#define GetCurrentDatetime	char currentTime[MAXBUFF]; db.getTime(currentTime)

struct ServerThread {
	pthread_t pid;
	SOCKET client;
	DB db;

	void clean() {
		if (client != INVALID_SOCKET) closesocket(client);
		db.close();
	}
};

// user_in_chatroom relation
struct UserInChat {
	int uid;
	int cid;
	char lastShown[TIME_LEN + 1];

	UserInChat(int _uid = 0, int _cid = 0) : uid(_uid), cid(_cid) { lastShown[0] = 0; }
};

void generate_password(char *pwd);

extern LOCK rwlock_msgSockets;
extern map<int, SOCKET> g_msgSockets;	// <uid, SOCKET>

extern LOCK rwlock_onlineUsers;
extern set<int> g_onlineUsers;	// <uid, Y/N>
