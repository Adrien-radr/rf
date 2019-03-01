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
#define Assert(expr) if(!(expr)) { printf("Assert %s line %d. (\"%s\")\n", __FILE__, __LINE__, #expr); *(int*)0 = 0; }
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

inline bool IsAligned(uint64 p, uint64 align)
{
	Assert(align > 0);
	return (p & align - 1) == 0;
}

inline uint64 AlignUp(uint64 Size, uint64 Align)
{
	return Size + (((~Size) + 1) & (Align - 1));
}

inline bool IsPow2(uint32 x)
{
	return x && !(x & (x - 1));
}

inline bool IsPow2(uint64 x)
{
	return x && !(x & (x - 1));
}

inline uint32 NextPow2(uint32 x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;
}

inline uint64 NextPow2(uint64 x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
	x++;
	return x;
}

// multiplicative hash mix function, to avoid as much as possible clusting (for open addressing hmaps)
// from fnv-1a
inline uint64 hash_uint64(uint64 x)
{
#if 1
	static uint64 offset = 14695981039346656037;
	static uint64 prime = 1099511628211;
	x ^= offset;
	x *= prime;
#else
	// this is from per vognsen, but i couldnt find the source or why. it seems a bit related to murmurhash3
	x *= 0xff51afd7ed558ccd;
	x ^= x >> 32;
#endif
	return x;
}

inline uint64 hash_bytes(const char *bytes, uint64 len)
{
	static uint64 offset = 14695981039346656037;
	static uint64 prime = 1099511628211;

	uint64 hash = offset;
	for (uint64 i = 0; i < len; ++i)
	{
		hash ^= bytes[i];
		hash *= prime;
	}

	return hash;
}

#endif
