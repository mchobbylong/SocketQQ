#pragma once 

#include <global.hpp>
#include "ClientUI.hpp"
#include "MessageWindow.hpp"

extern SOCKET g_serverSocket, g_msgSocket;
extern UserData g_me;
extern bool g_isLogin;
extern bool g_isExit;

// extern LOCK rwlock_chatrooms;
// extern map<int, Chatroom> g_chatrooms;

extern LOCK rwlock_chatWindows;
extern map<int, MessageWindow> g_chatWindows;
