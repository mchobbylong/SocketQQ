#include "ServerData.hpp"

void generate_password(char *pwd) {
	char allowChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	int len = strlen(allowChars);
	for (int i = 0; i < CHATPWD_LEN; ++i)
		pwd[i] = allowChars[rand() % len];
	pwd[CHATPWD_LEN] = 0;
}
