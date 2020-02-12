#pragma once

#include <global.hpp>

// Anser-back code
#define RES_ERROR		-1	// Fail
#define RES_NULL		00	// NULL
#define RES_SUCCESS		01	// Success
#define RES_PWD_ERROR	02	// Wrong password

/* The command code */
#define CMD_NULL                    00	// NULL commmand
// User management
#define CMD_REGISTER                10  // Register
#define CMD_LOGIN                   11  // Login in
#define CMD_LOGOUT		            12  // Login out
#define CMD_GET_ONLINE_USER         13  // Get all online users
#define CMD_SET_USER_NAME           14  // Set username
#define CMD_SET_USER_PWD            15  // Set user password
// Operation on chat massage 
#define CMD_LISTEN_MESSAGE			20	// Register interface for receiving message
#define CMD_NEW_MESSAGE				21	// Send chat massage
#define CMD_BROADCAST_MESSAGE		22	// Broadcast chat message
#define CMD_PUSH_UNREAD				23	// Push unread message count
#define CMD_SYNC_LASTSHOWN			24	// Synchronization last show time
// Operation on chatroom
#define CMD_NEW_CHATROOM            30  // Create new chatroom
#define CMD_ABORT_NEW_CHATROOM		31	// Abort creating new chatroom
#define CMD_OPEN_CHATWINDOW			32	// Open chatroom window
#define CMD_JOIN_CHATROOM			33	// Join a chatroom
#define CMD_REJECT_INVITATION		34	// Reject the invatation
#define CMD_ADD_USER_INTO_CHAT		35	// Invite users to join the chatroom
#define CMD_GET_GROUP_CHATROOM		36	// Get existing chatrooms
#define CMD_UPDATE_CHATROOM			37	// Update chatroom info
// Management for chatroom
#define CMD_QUIT_CHATROOM			40	// Quit chatroom

// Apply internal error codes
#define SUCCESS			00
#define ERROR_MALLOC	01
#define ERROR_RECV		02
#define ERROR_SEND		03
#define ERROR_ACCEPT	04
#define ERROR_OTHER		100

// Length limits of generic string
#define USERNAME_LEN	100
#define USERPWD_LEN		260
#define CHATNAME_LEN	50
#define CHATPWD_LEN		8
#define TIME_LEN		50
#define MAXBUFF			1000

// Generic constant string
#define STR_SYSTEM_NAME		"SYSTEM"

// pthread rwlock
#define LOCK				pthread_rwlock_t
#define LOCK_INIT			PTHREAD_RWLOCK_INITIALIZER
#define AcquireRead(lock)	pthread_cleanup_push(pthread_rwlock_unlock, (void*)&lock); pthread_rwlock_rdlock(&lock)
#define AcquireWrite(lock)	pthread_cleanup_push(pthread_rwlock_unlock, (void*)&lock); pthread_rwlock_wrlock(&lock)
#define Release(lock)		pthread_cleanup_pop(1)

// User data
struct UserData {
	int uid;	// Primary key
	char name[USERNAME_LEN + 1];
	char pwd[USERPWD_LEN + 1];

	UserData(int _uid = 0, char *_name = "", char *_pwd = "") {
		uid = _uid;
		strcpy(name, _name);
		strcpy(pwd, _pwd);
	}
};

// Chatroom
struct Chatroom {
	int cid;
	int type;		// 0 - Single chat; 1 - Group chat
	char name[CHATNAME_LEN + 1];
	int masterUID;	// uid of the room master
	char pwd[CHATPWD_LEN + 1];

	Chatroom(int _type = 0, int _masterUID = 0, char *_name = "", char *_pwd = "") {
		cid = 0;
		type = _type;
		masterUID = _masterUID;
		strcpy(name, _name);
		strcpy(pwd, _pwd);
	}
};


// Chat massage
struct ChatMessage {
	int cid;
	int fromUID;
	char fromName[USERNAME_LEN + 1];
	char time[TIME_LEN + 1];
	char msg[MAXBUFF + 1];

	ChatMessage(int _cid = 0, int _from = 0, char* name = STR_SYSTEM_NAME, char* _msg = "", char* _time = "") {
		cid = _cid;
		fromUID = _from;
		strcpy(fromName, name);
		strcpy(msg, _msg);
		strcpy(time, _time);
	}
};

// Socket communication
#define DEFAULT_PORT 5019

// Data head
struct DataHead {
	int cmd;
	int response;
	int dataLen;
	
	DataHead(int _cmd, int _response = RES_NULL, int _dataLen = 0): cmd(_cmd), response(_response), dataLen(_dataLen) {}
};

/* Socket data encapsulation */
template <class T>
int Recv(SOCKET &socket, T &data, int dataLen = sizeof T) {
	int ret = SUCCESS;
	if (recv(socket, (char*)&data, dataLen, 0) < 0) {
		// printf("recv failed: %d\n", WSAGetLastError());
		ret = ERROR_RECV;
	}
	return ret;
}

template <class T>
int Send(SOCKET &socket, T &data, int dataLen = sizeof T) {
	int ret = SUCCESS;
	if (send(socket, (char*)&data, dataLen, 0) < 0) {
		// printf("send failed: %d\n", WSAGetLastError());
		ret = ERROR_SEND;
	}
	return ret;
}
