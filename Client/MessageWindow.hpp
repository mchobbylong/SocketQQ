#pragma once

#include <global.hpp>
#include "..\ConsoleLoggerLib\ConsoleLogger.h"
#pragma comment(lib, "ConsoleLogger.lib")

struct MessageWindow {
	CConsoleLoggerEx window;
	Chatroom room;
	UserData me;
	char logFile[MAXBUFF];
	LOCK lock = LOCK_INIT;
	
	MessageWindow() {}
	MessageWindow(Chatroom &_room, UserData _me) {
		room = _room;
		me = _me;
		sprintf(logFile, "%s-%d (%s).log", me.name, room.cid, room.name);
	}

	void open() {
		Sprintf(title, "[%s]: [%d] %s", me.name, room.cid, room.name);
		window.Create(title, 55, 18);
	}
	
	void close() {
		window.Close();
	}

	void updateRoom(Chatroom &_room) {
		room = _room;
	}

	void printMessage(ChatMessage &msg) {
		Sprintf(strMsg, "%s: %s\n", msg.fromName, msg.msg);
		int color = CConsoleLoggerEx::COLOR_WHITE;
		if (msg.fromUID == 0 || msg.fromUID == me.uid)
			color = CConsoleLoggerEx::COLOR_GREEN;
		AcquireWrite(lock);
		window.cprintf(color, "%s", strMsg);
		FILE *f = fopen(logFile, "a");
		fprintf(f, "[%s] %s", msg.time, strMsg);
		fclose(f);
		Release(lock);
	}
};
