#pragma once

#include "ServerData.hpp"

int AddMessageSocket(SOCKET &client);

int GetChatMessage(SOCKET &client, DB &db, UserData &currUser);

int CreateChatroom(SOCKET &client, DB &db, UserData &currUser);

int JoinChatroom(SOCKET &client, DB &db, UserData &currUser);

int RejectInvitation(SOCKET &client, DB &db, UserData &currUser);

int SyncLastShown(SOCKET &client, DB &db, UserData &currUser);

int PushUnreadCounts(SOCKET &client, DB &db, UserData &currUser);

int GetGroupChatroom(SOCKET &client, DB &db, UserData &currUser);

int QuitChatroom(SOCKET &client, DB &db, UserData &currUser);

int UpdateChatroom(SOCKET &client, DB &db, UserData &currUser);
