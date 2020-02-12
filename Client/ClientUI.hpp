#pragma once

#include <global.hpp>

#define clsLine	while (getchar() != '\n')

bool GetYN(char *question);

int GetNum(char *question);

void GetName(char *question, char *ans, char *extraAllowed = "");
