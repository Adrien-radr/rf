#ifndef RADAR_COMMON
#define RADAR_COMMON

#include <cstdio>
#include <cstring>
#include <vector>
#include "linmath.h"

#define RF_MAJOR 0
#define RF_MINOR 0
#define RF_PATCH 1

// Platform
#if defined(_WIN32) || defined(_WIN64)
#   define RF_WIN32 1
#ifdef LIBEXPORT
#   define DLLEXPORT extern "C" __declspec(dllexport)
#else
#   define DLLEXPORT
#endif
#elif defined(__unix__) || defined (__unix) || defined(unix)
#   define RF_UNIX 1
#   define DLLEXPORT extern "C"
#   include <stddef.h>
#else
#   error "Unknown OS. Only Windows & Linux supported for now."
#endif

typedef float real32;
typedef double real64;

typedef char                int8;
typedef unsigned char       uint8;
typedef short int           int16;
typedef unsigned short int  uint16;
typedef int                 int32;
typedef unsigned int        uint32;
typedef long long           int64;
typedef unsigned long long  uint64;

#define MAX_PATH 260
typedef char path[MAX_PATH];

#ifdef DEBUG
#ifndef Assert
#define Assert(expr) if(!(expr)) { printf("Assert %s %d.\n", __FILE__, __LINE__); *(int*)0 = 0; }
#endif
#define DebugPrint(str, ...) printf(str, ##__VA_ARGS__);
#else
#ifndef Assert
#define Assert(expr) 
#endif
#define DebugPrint(str, ...)
#endif

#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif

#define Kilobytes(num) (1024LL*(num))
#define Megabytes(num) (1024LL*Kilobytes(num))
#define Gigabytes(num) (1024LL*Megabytes(num))


#endif
