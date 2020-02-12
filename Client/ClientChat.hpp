#pragma once

#include "ClientData.hpp"

int OpenChatWindow();

int UpdateChatroom();

int CloseChatWindow(int _cid = 0);

int DisplayMessage();

int SendChatMessage(int cid, char *msg);

int ReceiveUnreadCounts();

int OpenPrivateChat();

int OpenGroupChat();

int DealInvitation();

int ChatroomManageMenu();
