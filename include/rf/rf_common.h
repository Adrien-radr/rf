#ifndef RF_COMMON
#define RF_COMMON

#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include "linmath.h"

#define RF_MAJOR 0
#define RF_MINOR 1
#define RF_PATCH 0

// Platform
#if defined(_WIN32) || defined(_WIN64)
#   define RF_WIN32 1
#   define NOMINMAX 1
#   include <Windows.h>
#   define ALIGNED(...) __declspec(align(__VA_ARGS__))
#ifdef LIBEXPORT
#   define DLLEXPORT extern "C" __declspec(dllexport)
#else
#   define DLLEXPORT
#endif
#elif defined(__unix__) || defined (__unix) || defined(unix)
#   define RF_UNIX 1
#   define DLLEXPORT extern "C"
#   include <stddef.h>
#   define ALIGNED(...) __attribute__((aligned(__VA_ARGS__)))
#else
#   error "Unknown OS. Only Windows & Linux supported for now."
#endif

typedef float real32;
typedef double real64;

typedef signed char         int8;
typedef unsigned char       uint8;
typedef short int           int16;
typedef unsigned short int  uint16;
typedef int                 int32;
typedef unsigned int        uint32;
typedef long long           int64;
typedef unsigned long long  uint64;

#define MAX_PATH 260
#define MAX_STRLEN 512
typedef char path[MAX_PATH];
typedef char str[MAX_STRLEN];

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
#define D_ONLY(x) x
#else
#define D_ONLY(x)
#endif

#define KB (1024llu)
#define MB (1024llu * KB)
#define GB (1024llu * MB)

inline bool IsAligned(uint32 Size, uint32 Align)
{
	Assert(Align > 0);
	return (Size & (Align - 1)) == 0;
}

inline uint32 AlignUp(uint32 Size, uint32 Align)
{
	return Size + (((~Size) + 1) & (Align - 1));
}

#endif
