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

/*
	RF Memory System

	# Pool
	- Are initialized once with a static amount of memory, they cannot be realloc'ed with a larger memory span.
	- The developer should define beforehand how much memory his application will need.
	- The goal is to have 1 calloc() at init and 1 free() at end of application.
	- With a pool with a set amount of memory, one can ask for some of that pool's memory (alloc), give it back(free) or extend it(realloc)
	- Manages free chunks in memory internally, sorted by decreasing size.
	- A just freed chunk will try to merge itself with other free chunks if they are contiguous to avoid too much defragmentation

		MEM_POOL_CHUNK_LIST_SIZE (def=256) - defines how much free chunks are stored in the pool internals. A created pool starts with 1 chunk
			spanning the whole allocated capacity. Chunks are sorted in decreasing order of size. Chunks that fall off the CHUNK_LIST_SIZE are 
			forgotten forever. The rationale for this is that they should be small anyway and not very useful to further allocation because of the
			merging strategy
		MEM_POOL_ALIGNMENT (def=16) - each pool automatically aligns the memory chunks it gives to askers to that value.

	# Arena (static large-block memory)
	- Use the pool system to ask for blocks of contiguous memory (e.g. blocks of 1KB, 1MB, etc), and store those blocks internally for use by systems
	- Ultimately, each subsystem of the application should make use of its own arena so that everything is delimited
	- The goal here is easy dealloc of a whole subsystem
	- Arenas dynamically grow block by block by asking from its linked pool. It can grow for as much as there is memory in the pool
	- Individual allocations in an arena are not deallocable, only allocs are allowed in arenas, that will push the arena ptr forward in its own 
	  pool-alloc'ed memory.

		MEM_ARENA_BLOCK_SIZE (def=1MB) - Block size for arena allocation. this is the min amount of memory retrieved from the pool each time the
			arena has to grow

	# Buf (strechy buffer, dynamic array)
		 From Per Vognsen's Bitwise, from Sean Barrett
		 Adapted to C++ and Pool system
	- Tries to emulate std::vector-like behaviour
	- Grows dynamically relatively to its current size by a constant factor
	- Gets memory from a pool

		MEM_BUF_GROW_FACTOR (def=1.5) - Constant growth factor that multiplies the current capacity of the dynamic buffer when over-capacity
*/

#define MEM_POOL_CHUNK_LIST_SIZE 256
#define MEM_POOL_ALIGNMENT 16
#define MEM_BUF_GROW_FACTOR 1.5
#define MEM_ARENA_BLOCK_SIZE (1llu * MB)

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

struct mem_pool
{
	uint32		Capacity;
	mem_chunk	MemChunks[MEM_POOL_CHUNK_LIST_SIZE];
	int32		NumMemChunks;
	uint8		*Buffer;
};

struct mem_buf
{
	uint64	 Size;
	uint64	 Capacity;
	mem_pool *Pool;
	uint8	 BufferData[1];
};

struct mem_arena
{
	uint8		*Ptr;
	uint8		*BlockEnd;
	mem_pool	*Pool;
	uint8		**Blocks;

	mem_arena() : Ptr(nullptr), BlockEnd(nullptr), Pool(nullptr), Blocks(nullptr) {}
};

#define mem_addr__hdr(a) ((mem_addr*)((uint8*)(a) - offsetof(mem_addr, Ptr)))
#define mem_buf__hdr(b) ((mem_buf*)((uint8*)(b) - offsetof(mem_buf, BufferData)))
inline uint64 mem_chunk__end(mem_chunk *chunk) { return chunk->Loc + chunk->Size; }

// Following functions are internal and shouldn't be used.
// Use the public interface functions further below instead
void _MemPoolAddFreeChunk(mem_pool *Pool, mem_chunk &chunk);
void _MemPoolRemoveFreeChunk(mem_pool *Pool, int chunkIdx);
void *_MemPoolAlloc(mem_pool *Pool, uint64 Size);
void *_MemPoolRealloc(mem_pool *Pool, void *Ptr, uint64 Size);
void _MemPoolFree(mem_pool *Pool, void *Ptr);
void _MemPoolPrintStatus(mem_pool *Pool);

void *_MemBufGrow(mem_pool *Pool, void *Ptr, uint64 Count, uint64 ElemSize);

template<typename T>
inline void _MemBufCheckGrowth(T **b, uint64 Size)
{
	uint64 cap = (*b) ? mem_buf__hdr(*b)->Capacity : 0;
	if (Size > cap)
	{
		*b = (T*)_MemBufGrow(mem_buf__hdr(*b)->Pool, *b, Size, sizeof(T));
	}
}

void _ArenaGrow(mem_arena *Arena, uint64 MinSize);
void *_ArenaAlloc(mem_arena *Arena, mem_pool *Pool, uint64 Size);

// ##########################################################################
// Public interface for RF Memory system
inline mem_pool *PoolCreate(uint32 PoolCapacity)
{
	mem_pool *pool = (mem_pool*)calloc(1, sizeof(mem_pool));
	pool->Buffer = (uint8*)calloc(1, (size_t)PoolCapacity);
	pool->MemChunks[0] = mem_chunk{ 0, PoolCapacity };
	pool->NumMemChunks = 1;
	pool->Capacity = PoolCapacity;
	return pool;
}

inline void PoolDestroy(mem_pool **Pool)
{
	free((*Pool)->Buffer);
	free(*Pool);
	*Pool = nullptr;
}

inline void PoolClear(mem_pool *Pool)
{
	Pool->NumMemChunks = 1;
	Pool->MemChunks[0] = mem_chunk{ 0, Pool->Capacity };
	memset(Pool->Buffer, 0, Pool->Capacity);
}

template<typename T>
inline T *PoolAlloc(mem_pool *Pool, uint64 Count)
{
	return (T*)_MemPoolAlloc(Pool, Count * sizeof(T));
}

template<typename T>
inline T *PoolRealloc(mem_pool *Pool, T *Ptr, uint64 Count)
{
	return (T*)_MemPoolRealloc(Pool, (void*)Ptr, Count * sizeof(T));
}

template<typename T>
inline void PoolFree(mem_pool *Pool, T *Ptr)
{
	_MemPoolFree(Pool, (void*)Ptr);
}

#define BufSize(b)		((b) ? mem_buf__hdr(b)->Size : 0)
#define BufCapacity(b)	((b) ? mem_buf__hdr(b)->Capacity : 0)
#define BufEnd(b)		((b) + BufSize(b))
#define BufClear(b)		((b) ? mem_buf__hdr(b)->Size = 0 : 0)
#define BufFree(b)		((b) ? (_MemPoolFree(mem_buf__hdr(b)->Pool, mem_buf__hdr(b)), (b) = nullptr) : 0)

template<typename T>
inline T *Buf(mem_pool *Pool, uint64 Capacity = 0)
{
	return (T*)_MemBufGrow(Pool, nullptr, Capacity, sizeof(T));
}

template<typename T>
inline void BufPush(T *b, T v)
{
	_MemBufCheckGrowth(&b, BufSize(b) + 1);
	b[mem_buf__hdr(b)->Size++] = v;
}

// create a new mem_buf string from nothing
char *Str(mem_pool *Pool, const char *StrFmt, ...);

// concat a new formatted string to an existing mem_buf string (created by Str() or Buf<char>())
void StrCat(char **StrBuf, const char *StrFmt, ...);

template<typename T>
T *ArenaAlloc(mem_arena *Arena, mem_pool *Pool, uint64 Count)
{
	return (T*)_ArenaAlloc(Arena, Pool, Count * sizeof(T));
}

// Frees the whole arena and its blocks, after that, allocing restart from the beginning
void ArenaFree(mem_arena *Arena);

// ##########################################################################


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
