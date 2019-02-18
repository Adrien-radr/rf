#ifndef RF_DEFS_H
#define RF_DEFS_H

#include "rf_common.h"
#include "keys.h"

// TODO - See if 256 is enough for more one liner ui strings
#define CONSOLE_CAPACITY 128
#define CONSOLE_STRINGLEN 256
#define UI_STRINGLEN 256
#define UI_MAXSTACKOBJECT 256

#define KEY_HIT(KeyState) ((KeyState >> 0x1) & 1)
#define KEY_RELEASED(KeyState) ((KeyState >> 0x2) & 1)
#define KEY_PRESSED(KeyState) ((KeyState >> 0x3) & 1)
#define MOUSE_HIT(MouseState) KEY_HIT(MouseState)
#define MOUSE_RELEASED(MouseState) KEY_RELEASED(MouseState)
#define MOUSE_PRESSED(MouseState) KEY_PRESSED(MouseState)

#include <memory>
/// Memory pool and arena helper functions
namespace rf {

#define MEM_POOL_CHUNK_LIST_SIZE 256
#define MEM_POOL_ALIGNMENT 16

struct mem_chunk
{
	uint64 Loc;
	uint64 Size;
};

struct mem_addr
{
	uint64 Loc;
	uint64 Size;
	void *Ptr;
};

#define mem_addr__hdr(a) ((mem_addr*)((uint8*)(a) - offsetof(mem_addr, Ptr)))
inline uint64 mem_chunk__end(mem_chunk *chunk) { return chunk->Loc + chunk->Size; }

struct mem_pool
{
	uint32		Capacity;
	mem_chunk	MemChunks[MEM_POOL_CHUNK_LIST_SIZE];
	int32		NumMemChunks;
	uint8		*Buffer;
};

inline mem_pool *MemPoolCreate(uint32 PoolCapacity)
{
	mem_pool *pool = (mem_pool*)calloc(1, sizeof(mem_pool));
	pool->Buffer = (uint8*)calloc(1, (size_t)PoolCapacity);
	pool->MemChunks[0] = mem_chunk{ 0, PoolCapacity };
	pool->NumMemChunks = 1;
	return pool;
}

inline void MemPoolDestroy(mem_pool **Pool)
{
	free((*Pool)->Buffer);
	free(*Pool);
	*Pool = nullptr;
}

inline void _MemPoolPrintStatus(mem_pool *Pool)
{
	for (int i = 0; i < Pool->NumMemChunks; ++i)
	{
		printf("free chunk %d : loc %llu size %llu.\n", i, Pool->MemChunks[i].Loc, Pool->MemChunks[i].Size);
	}
	if (!Pool->NumMemChunks)
	{
		printf("no free chunks\n");
	}
}

// Following functions are internal and shouldn't be used.
// Use the Alloc/Realloc/Free functions below instead
void _MemPoolAddFreeChunk(mem_pool *Pool, mem_chunk &chunk);
void _MemPoolRemoveFreeChunk(mem_pool *Pool, int chunkIdx);
void *_MemPoolAlloc(mem_pool *Pool, uint64 Size);
void *_MemPoolRealloc(mem_pool *Pool, void *Ptr, uint64 Size);
void _MemPoolFree(mem_pool *Pool, void *Ptr);

template<typename T>
inline T *PoolAlloc(mem_pool *Pool, uint32 Count)
{
	return (T*)_MemPoolAlloc(Pool, Count * sizeof(T));
}

template<typename T>
inline T *PoolRealloc(mem_pool *Pool, T *Ptr, uint32 Count)
{
	return (T*)_MemPoolRealloc(Pool, (void*)Ptr, Count * sizeof(T));
}

template<typename T>
inline void PoolFree(mem_pool *Pool, T *Ptr)
{
	_MemPoolFree(Pool, (void*)Ptr);
}

struct memory_arena
{
	uint8   *BasePtr;   // Start of Arena, in bytes
	uint64  Size;       // Used amount of memory
	uint64  Capacity;   // Total size of arena
};

inline void InitArena(memory_arena *Arena, uint64 Capacity, void *BasePtr)
{
	Arena->BasePtr = (uint8*)BasePtr;
	Arena->Capacity = Capacity;
	Arena->Size = 0;
}

inline void ClearArena(memory_arena *Arena)
{
	// TODO - Do we need to wipe this to 0 each time ? Profile it to see if its
	// a problem. It seems like best practice
	memset(Arena->BasePtr, 0, sizeof(uint8) * Arena->Capacity);
	Arena->Size = 0;
}

inline void *_PushArenaData(memory_arena *Arena, uint64 ElemSize, uint64 ElemCount, uint32 Align)
{
	void *ArenaSlot = (void*)(Arena->BasePtr + Arena->Size);
	uint64 Remaining = Arena->Capacity - Arena->Size;

	void *AlignedSlot = std::align(Align, ElemSize, ArenaSlot, Remaining);
	if (AlignedSlot)
	{
		Arena->Size = (uint64)((uint8*)AlignedSlot - Arena->BasePtr) + ElemSize * ElemCount;
		return AlignedSlot;
	}
	printf("ALLOC ERROR : arena <size %llu, cap %llu>, alloc <size %llu, count %llu, align $%lu>\n",
		Arena->Size, Arena->Capacity, ElemSize, ElemCount, Align);
	return nullptr;
}
}

#define POOL_OFFSET(Pool, Structure) ((uint8*)(Pool) + sizeof(Structure))

template<typename T>
T* Alloc(rf::memory_arena *Arena, uint32 Count = 1, uint32 Align = 1)
{
	return (T*)rf::_PushArenaData(Arena, sizeof(T), Count, Align);
}

namespace rf {
struct context;
typedef uint8 key_state;
typedef uint8 mouse_state;

struct os_version
{
	char OSName[0x20];
	unsigned Major;
	unsigned Minor;
	unsigned Build;
};

struct system_info
{
	os_version OSVersion;

	int CPUCountLogical;
	int CPUCountPhysical;
	double CPUGHz;
	int SystemMB;
	bool SSESupport;
	bool x64;

	char CPUName[0x20];
	char *CPUBrand;
	char _CPUBrand[0x40];
	char GPUDesc[1024];
};

// Contains all input for a frame
struct input
{
	real64 dTime;
	real64 dTimeFixed;

	int32  MousePosX;
	int32  MousePosY;
	int32  MouseDX;
	int32  MouseDY;
	int32  MouseDZ; // wheel

	key_state Keys[KEY_LAST + 1];

	mouse_state MouseLeft;
	mouse_state MouseRight;
};

namespace ui
{
enum theme_color
{
	COLOR_RED,
	COLOR_GREEN,
	COLOR_BLUE,
	COLOR_BLACK,
	COLOR_WHITE,

	COLOR_DEBUGFG,
	COLOR_PANELFG,
	COLOR_PANELBG,
	COLOR_TITLEBARBG,
	COLOR_BORDERBG,
	COLOR_CONSOLEFG,
	COLOR_CONSOLEBG,
	COLOR_SLIDERBG,
	COLOR_SLIDERFG,
	COLOR_BUTTONBG,
	COLOR_BUTTONPRESSEDBG,
	COLOR_PROGRESSBARBG,
	COLOR_PROGRESSBARFG,
};

enum decoration_flag
{
	DECORATION_NONE = 0x0,
	DECORATION_TITLEBAR = 1 << 1,
	DECORATION_RESIZE = 1 << 2,
	DECORATION_RGBTEXTURE = 1 << 3,
	DECORATION_MARGIN = 1 << 4,
	DECORATION_BORDER = 1 << 5,
	DECORATION_INVISIBLE = 1 << 6,
	DECORATION_FOCUS = 1 << 7,
};

enum theme_font
{
	FONT_DEFAULT,
	FONT_CONSOLE,
	FONT_AWESOME
};

struct text_line
{
	char        String[UI_STRINGLEN];
	vec2i       Position;
	theme_font  Font;
	theme_color Color;
};
}

typedef char console_log_string[CONSOLE_STRINGLEN];
struct console_log
{
	console_log_string MsgStack[CONSOLE_CAPACITY];
	uint32 WriteIdx;
	uint32 ReadIdx;
	uint32 StringCount;
};
}
#endif
