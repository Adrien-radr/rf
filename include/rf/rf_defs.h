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

/* TMP - for safekeeping while working on it

// have a header with common variables (size, capacity)
// have a preallocated array (at the start of the pool) that will store all the memlocations that are available.
// this array is sorted from largest available loc to smallest.
// the back of the pool (neverallocated memory) is in this array (probably at the top nearly always)
// this array is of a fixed size, with the smallest chunks of memory getting pushed out of it over time.
// the rational is that small chunks that get pushed out should be so small compared to the ones above that they shouldnt matter in the long run
// e.g., a chunk of 1byte fragmented in the middle of the pool shouldnt matter compared to the rest of the unnalocated pool and other larger chunks that could have been dealloc'ed

// TODO - when getting a chunk from the pool, go from biggest to smallest, since it should be faster than the other way around
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

struct mem_pool
{
	uint64		Capacity;
	mem_chunk	MemChunks[MEM_POOL_CHUNK_LIST_SIZE];
	int32		NumMemChunks;
	uint8		*Buffer;
};

mem_pool *MemPoolCreate(uint64 PoolCapacity)
{
	mem_pool *pool = (mem_pool*)calloc(1, sizeof(mem_pool));
	pool->Buffer = (uint8*)calloc(1, PoolCapacity);
	pool->MemChunks[0] = mem_chunk{ 0, PoolCapacity };
	pool->NumMemChunks = 1;
	return pool;
}

void MemPoolDestroy(mem_pool **Pool)
{
	free((*Pool)->Buffer);
	free(*Pool);
	*Pool = nullptr;
}

void MemPoolPrintStatus(mem_pool *Pool)
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

inline bool IsAligned(uint64 p, uint64 align)
{
	Assert(align > 0);
	return (p & align - 1) == 0;
}

inline uint64 AlignUp(uint64 Size, uint64 Align)
{
	return Size + (((~Size) + 1) & (Align-1));
}

// Here we remove one free chunk, cut it with enough size for the asked alloc, and insert the remaining
// mem chunk in the chunk array, so that no overflow in number of chunks is possible.
void *MemPoolAlloc(mem_pool *Pool, uint64 Size)
{
	Assert(Pool);
	//Assert(Pool->NumMemChunks);
	// alloc so that the returned pointer is aligned to MEM_POOL_ALIGNMENT
	// and has enough space before it to squeeze in the mem_addr header
	uint64 allocSize = AlignUp(AlignUp(Size, MEM_POOL_ALIGNMENT) + 1, MEM_POOL_ALIGNMENT);
	//Assert(Pool->MemChunks[0].Size >= allocSize);

	if (!Pool->NumMemChunks || Pool->MemChunks[0].Size < allocSize)
	{
		printf("Alloc Error : not enough memory available in pool (asking %llu, available %d chunks maxSize %llu).\n",
			allocSize, Pool->NumMemChunks, Pool->MemChunks[0].Size);
		return nullptr;
	}

	// find first fitting chunk in list that bounds queried size
	// start bottom up, getting the first that's large enough
	int chunkIdx = Pool->NumMemChunks - 1;
	mem_chunk chunk = Pool->MemChunks[chunkIdx];
	for (chunkIdx; chunkIdx >= 0; --chunkIdx)
	{
		if (Pool->MemChunks[chunkIdx].Size >= allocSize)
		{
			// get the chunk from the array, erase it from there
			chunk = Pool->MemChunks[chunkIdx];
			Pool->MemChunks[chunkIdx] = mem_chunk{ 0,0 };
			Pool->NumMemChunks--;

			// consolidate array
			for (int i = chunkIdx; i < Pool->NumMemChunks; ++i)
			{
				Pool->MemChunks[i] = Pool->MemChunks[i + 1];
			}
			break;
		}
	}

	if (chunkIdx < 0)
	{
		printf("Pool Alloc chunkIdx<0. Shouldn't happen !?!\n");
		return nullptr;
	}

	// get the aligned chunk for return
	// fill the hdr with loc/size info
	uint64 slotLoc = AlignUp(AlignUp(chunk.Loc, MEM_POOL_ALIGNMENT) + 1, MEM_POOL_ALIGNMENT);
	void *slot = (void*)(Pool->Buffer + slotLoc);
	mem_addr *slotHdr = mem_addr__hdr(slot);
	slotHdr->Loc = chunk.Loc;
	slotHdr->Size = allocSize;


	// put the cut part back in the available memchunk section
	void *remainingPart = (void*)(Pool->Buffer + chunk.Loc + allocSize);
	uint64 remaining = chunk.Size - allocSize;

	if (remaining)
	{
		chunk.Loc = (uint64)((uint8*)remainingPart - Pool->Buffer);
		chunk.Size = remaining;

		// find sorted location the new free chunk
		int insertIdx = 0;
		for (insertIdx; insertIdx < Pool->NumMemChunks; ++insertIdx)
		{
			if (chunk.Size > Pool->MemChunks[insertIdx].Size)
			{
				break;
			}
		}
		// move smaller chunks down
		for (int moveIdx = Pool->NumMemChunks - 1; moveIdx >= insertIdx; --moveIdx)
		{
			Pool->MemChunks[moveIdx] = Pool->MemChunks[moveIdx - 1];
		}

		Pool->MemChunks[insertIdx] = chunk;
		Pool->NumMemChunks++;
	}

	return slot;
}

void MemPoolFree(mem_pool *Pool, void *Ptr)
{
	mem_addr *ptrAddr = mem_addr__hdr(Ptr);

	// zero the memory under the pointer, return it to the pool's available chunks
	mem_chunk newChunk{ ptrAddr->Loc, ptrAddr->Size };
	memset(Pool->Buffer + newChunk.Loc, 0, newChunk.Size);

	// find slot for chunk insert
	int chunkIdx;
	for (chunkIdx = Pool->NumMemChunks; chunkIdx > 0; --chunkIdx)
	{
		if (newChunk.Size <= Pool->MemChunks[chunkIdx - 1].Size)
		{
			break;
		}
	}

	// push smaller chunks up the array to make way for insert
	int moveIdx = Pool->NumMemChunks;

	if (moveIdx < MEM_POOL_CHUNK_LIST_SIZE)
	{ // already max number of recorded chunks, the smaller will be pushed out the list by design
		Pool->NumMemChunks++;
	}
	else
	{
		moveIdx = MEM_POOL_CHUNK_LIST_SIZE - 1;
	}

	for (moveIdx; moveIdx > chunkIdx; --moveIdx)
	{
		Pool->MemChunks[moveIdx] = Pool->MemChunks[moveIdx - 1];
	}

	Pool->MemChunks[chunkIdx] = newChunk;
}

void *MemPoolRealloc(mem_pool *Pool, void *Ptr, uint64 Size)
{
	// check if we can just extend the current chunk forward
	mem_addr *ptrAddr = mem_addr__hdr(Ptr);
	uint64 contiguousChunkStart = ptrAddr->Loc + ptrAddr->Size;

	int chunkIdx;
	for (chunkIdx = 0; chunkIdx < Pool->NumMemChunks; ++chunkIdx)
	{
		if (Pool->MemChunks[chunkIdx].Loc == contiguousChunkStart)
		{
			break;
		}
	}

	if (chunkIdx < Pool->NumMemChunks)
	{
		uint64 totalSize = ptrAddr->Size + Pool->MemChunks[chunkIdx].Size;
		if (totalSize >= (Size + 2 * sizeof(uint64)))
		{ // grouped chunks are large enough to fit the realloc, use that
			Pool->NumMemChunks--;

			mem_chunk oldChunk = Pool->MemChunks[chunkIdx];

			// consolidate array
			for (int i = chunkIdx; i < Pool->NumMemChunks; ++i)
			{
				Pool->MemChunks[i] = Pool->MemChunks[i + 1];
			}

			// TODO: cut the new chunk o fit the realloc, push the second part back to pool available chunks
			// the new size should be Size, aligned on 16, with 16 bytes in front for the header.
			// that new size should fit in the totalSize as the above test does
			// but a new chunk should only be added to the pool if the realloc doesnt take the
			//mem_chunk newChunk{ ,0 };

			ptrAddr->Size = totalSize;
			return Ptr;
		}
	}
	else
	{
		// if we cant extend, find a new chunk and move the memory there
		printf("not implemented yet.\n");
		return nullptr;
	}
}

template<typename T>
inline T *Alloc(mem_pool *Pool, uint32 Count)
{
	return (T*)MemPoolAlloc(Pool, Count * sizeof(T));
}
*/
#define MEM_POOL_CHUNK_LIST_SIZE 256
#define MEM_POOL_ALIGNMENT 16

struct mem_chunk
{
	uint32 Loc;
	uint32 Size;
	uint8  *Ptr;
};

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

inline void MemPoolPrintStatus(mem_pool *Pool)
{
	for (int i = 0; i < Pool->NumMemChunks; ++i)
	{
		printf("free chunk %d : loc %lu size %lu.\n", i, Pool->MemChunks[i].Loc, Pool->MemChunks[i].Size);
	}
	if (!Pool->NumMemChunks)
	{
		printf("no free chunks\n");
	}
}

// Here we remove one free chunk, cut it with enough size for the asked alloc, and insert the remaining 
// mem chunk in the chunk array, so that no overflow in number of chunks is possible.
inline void *_MemPoolAlloc(mem_pool *Pool, uint32 Size)
{
	Assert(Pool);
	Assert(Pool->NumMemChunks);
	uint32 allocSize = AlignUp(Size, MEM_POOL_ALIGNMENT);
	Assert(Pool->MemChunks[0].Size >= allocSize);

	// find first fitting chunk in list that bounds queried size
	// start bottom up, getting the first that's large enough
	int chunkIdx = Pool->NumMemChunks - 1;
	mem_chunk chunk = Pool->MemChunks[chunkIdx];
	for (chunkIdx; chunkIdx >= 0; --chunkIdx)
	{
		if (Pool->MemChunks[chunkIdx].Size >= allocSize)
		{
			// get the chunk from the array, erase it from there
			chunk = Pool->MemChunks[chunkIdx];
			Pool->MemChunks[chunkIdx] = mem_chunk{ 0,0,nullptr };
			Pool->NumMemChunks--;

			// consolidate array
			for (int i = chunkIdx; i < Pool->NumMemChunks; ++i)
			{
				Pool->MemChunks[i] = Pool->MemChunks[i + 1];
			}
			break;
		}
	}

	if (chunkIdx < 0)
	{
		printf("Shouldn't happen !?!\n");
		return nullptr;
	}


	// cut the one found to fit the asked size and return that chunk. 
	// align the cut part to next aligned location, and put it back in the available memchunk section
	void *slot = (void*)(Pool->Buffer + chunk.Loc);
	void *remainingPart = (void*)(Pool->Buffer + chunk.Loc + allocSize);
	size_t remaining = chunk.Size - allocSize;

	if (remaining)
	{
		chunk.Loc = (uint32)((uint8*)remainingPart - Pool->Buffer);
		chunk.Size = (uint32)remaining;

		// find sorted location the new free chunk 
		int insertIdx = 0;
		for (insertIdx; insertIdx < Pool->NumMemChunks; ++insertIdx)
		{
			if (chunk.Size > Pool->MemChunks[insertIdx].Size)
			{
				break;
			}
		}
		// move smaller chunks down
		for (int moveIdx = Pool->NumMemChunks - 1; moveIdx >= insertIdx; --moveIdx)
		{
			Pool->MemChunks[moveIdx] = Pool->MemChunks[moveIdx - 1];
		}

		Pool->MemChunks[insertIdx] = chunk;
		Pool->NumMemChunks++;
	}

	return slot;
}

template<typename T>
inline T *MemPoolAlloc(mem_pool *Pool, uint32 Count)
{
	return (T*)_MemPoolAlloc(Pool, Count * sizeof(T));
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
