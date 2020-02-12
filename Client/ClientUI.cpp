#include "ClientUI.hpp"

bool GetYN(char *question) {
	bool hasAns = 0;
	char ch;
	do {
		printf("%s [Y/N]: ", question);
		scanf(" %c", &ch);
		ch = tolower(ch);
		if (ch == 'y' || ch == 'n') hasAns = 1;
		else puts("Wrong input!");
		clsLine;
	} while (!hasAns);
	return ch == 'y';
}

int GetNum(char *question) {
	bool hasAns = 0;
	int num = -100000;
	do {
		printf("%s", question);
		scanf(" %d", &num);
		if (num != -100000) hasAns = 1;
		else puts("Wrong input!");
		clsLine;
	} while (!hasAns);
	return num;
}

bool findChar(char *str, char ch) {
	int len = strlen(str);
	for (int i = 0; i < len; ++i)
		if (str[i] == ch) return 1;
	return 0;
}

void GetName(char *question, char *ans, char *extraAllowed) {
	int len = 0;
	do {
		printf("%s", question);
		char ch;
		while (ch = getchar()) {
			if (ch == '\n') {
				ans[len] = 0;
				break;
			}
			if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || findChar(extraAllowed, ch))) {
				puts("Your input contains unallowed chars!");
				clsLine;
				len = 0;
				break;
			}
			ans[len++] = ch;
		}
	} while (len == 0 || ans[len] != '\0');
}
