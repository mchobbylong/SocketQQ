#pragma once
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <set>
#include <algorithm>
#include <pthread.h>
#include <stdarg.h>
using namespace std;

#include <BaseData.hpp>

#define forVec(VEC, item) for (auto iterator = VEC.begin(), VEC_END = VEC.end(); iterator != VEC_END && (item = *iterator, 1); ++iterator)
#define forMap(MAP, itemFir, itemSec) for (auto iterator = MAP.begin(), MAP_END = MAP.end(); iterator != MAP_END && ((itemFir = iterator->first), (itemSec = iterator->second), 1); ++iterator)
#define CheckRet(sth) if ((ret = sth) != SUCCESS) return ret

#define Sprintf(STR, ...) char STR[MAXBUFF]; sprintf(STR, __VA_ARGS__)
