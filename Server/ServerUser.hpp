#pragma once

#include "ServerData.hpp"

UserData GetUserByName(DB &db, char *name);

bool IsRepeatName(char *name, DB &db);

int Register(SOCKET &socket, DB &db);

int Login(SOCKET &socket, DB &db, UserData &currUser, bool &isLogin);

int Logout(SOCKET &socket, DB &db, UserData &currUser);

int SendOnlineUsers(SOCKET &socket, DB &db);
