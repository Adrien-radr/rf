#include <ctime>
#include "utils.h"
#include "context.h"

namespace rf {

void _MemPoolAddFreeChunk(mem_pool *Pool, mem_chunk &chunk)
{
	int chunkIdx;
	//find new loc to put the mergec chunk in
	for (chunkIdx = 0; chunkIdx < Pool->NumMemChunks; ++chunkIdx)
	{
		if (chunk.Size > Pool->MemChunks[chunkIdx].Size)
		{
			break;
		}
	}

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

	Pool->MemChunks[chunkIdx] = chunk;
}

void _MemPoolRemoveFreeChunk(mem_pool *Pool, int chunkIdx)
{
	Pool->MemChunks[chunkIdx] = mem_chunk{ 0,0 };
	Pool->NumMemChunks--;

	// consolidate
	for (int i = chunkIdx; i < Pool->NumMemChunks; ++i)
	{
		Pool->MemChunks[i] = Pool->MemChunks[i + 1];
	}
}

// Here we remove one free chunk, cut it with enough size for the asked alloc, and insert the remaining 
// mem chunk in the chunk array, so that no overflow in number of chunks is possible.
void *_MemPoolAlloc(mem_pool *Pool, uint64 Size)
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
		Assert(false);
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
			_MemPoolRemoveFreeChunk(Pool, chunkIdx);
			break;
		}
	}

	if (chunkIdx < 0)
	{
		printf("Pool Alloc chunkIdx<0. Shouldn't happen !?!\n");
		Assert(false);
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

		_MemPoolAddFreeChunk(Pool, chunk);
	}

	return slot;
}

void _MemPoolFree(mem_pool *Pool, void *Ptr)
{
	Assert(Pool);
	mem_addr *ptrAddr = mem_addr__hdr(Ptr);

	// zero the memory under the pointer, return it to the pool's available chunks
	mem_chunk newChunk{ ptrAddr->Loc, ptrAddr->Size };
	memset(Pool->Buffer + newChunk.Loc, 0, newChunk.Size);
	
	// find slot for chunk insert
	int prevChunkFreeIdx = -1;
	int nextChunkFreeIdx = -1;

	// find out if this chunk is the preceding or following chunk in contiguous memory for merging
	for (int i = 0; i < Pool->NumMemChunks; ++i)
	{
		if (Pool->MemChunks[i].Loc == mem_chunk__end(&newChunk))
		{
			nextChunkFreeIdx = i;
		}
		else if (mem_chunk__end(&Pool->MemChunks[i]) == newChunk.Loc)
		{
			prevChunkFreeIdx = i;
		}
	}

	// merge with prev and/or next chunks
	if (prevChunkFreeIdx >= 0 || nextChunkFreeIdx >= 0)
	{
		if (prevChunkFreeIdx >= 0)
		{
			mem_chunk prevChunk = Pool->MemChunks[prevChunkFreeIdx];
			_MemPoolRemoveFreeChunk(Pool, prevChunkFreeIdx);
			newChunk.Loc = prevChunk.Loc;
			newChunk.Size += prevChunk.Size;
		}
		if (nextChunkFreeIdx >= 0)
		{
			mem_chunk nextChunk = Pool->MemChunks[nextChunkFreeIdx];
			_MemPoolRemoveFreeChunk(Pool, nextChunkFreeIdx);
			newChunk.Size += nextChunk.Size;
		}
	}

	_MemPoolAddFreeChunk(Pool, newChunk);
}

void *_MemPoolRealloc(mem_pool *Pool, void *Ptr, uint64 Size)
{
	Assert(Pool);
	// check if we can just extend the current chunk forward
	mem_addr *ptrAddr = mem_addr__hdr(Ptr);
	uint64 contiguousChunkStart = mem_chunk__end((mem_chunk*)ptrAddr);

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
		mem_chunk contiguousChunk = Pool->MemChunks[chunkIdx];
		uint64 totalSize = ptrAddr->Size + contiguousChunk.Size;
		uint64 slotLoc = AlignUp(AlignUp(ptrAddr->Loc, MEM_POOL_ALIGNMENT) + 1, MEM_POOL_ALIGNMENT);
		uint64 hdrSize = slotLoc - ptrAddr->Loc;
		uint64 allocSize = Size + hdrSize;
		uint64 slotEnd = slotLoc + Size;
		if (totalSize >= allocSize)
		{ // grouped chunks are large enough to fit the realloc, use that
			_MemPoolRemoveFreeChunk(Pool, chunkIdx);

			// add the unneeded part back to the available list
			mem_chunk cutChunk{ slotEnd, totalSize - allocSize };

			_MemPoolAddFreeChunk(Pool, cutChunk);

			ptrAddr->Size = allocSize;
			return Ptr;
		}
	}

	// if we cant extend, find a new chunk and move the memory there
	void *retPtr = _MemPoolAlloc(Pool, Size);
	if (retPtr)
	{
		memcpy(retPtr, Ptr, ptrAddr->Size);
		_MemPoolFree(Pool, Ptr);
		return retPtr;
	}

	printf("Error during MemPoolRealloc, didn't find a suitable realloc location.\n");
	Assert(false);

	return nullptr;
}

void _MemPoolPrintStatus(mem_pool *Pool)
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

real32 PoolOccupancy(mem_pool *Pool)
{
	uint64 freeSpace = 0;
	for (int i = 0; i < Pool->NumMemChunks; ++i)
	{
		freeSpace += Pool->MemChunks[i].Size;
	}

	real64 freeRatio = freeSpace / (real64)(Pool->Capacity);
	return (real32)(1.0 - freeRatio);
}


void *_MemBufGrow(mem_pool *Pool, void *Ptr, uint64 Count, uint64 ElemSize)
{
	uint64 newCapacity = Max((uint64)(MEM_BUF_GROW_FACTOR * BufCapacity(Ptr)), Max(Count, 16));
	uint64 newAllocSize = newCapacity * ElemSize + offsetof(mem_buf, BufferData);
	mem_buf *retBuf;
	if (Ptr)
	{
		mem_buf *oldBuf = mem_buf__hdr(Ptr);
		retBuf = (mem_buf*)_MemPoolRealloc(Pool, oldBuf, newAllocSize);
	}
	else
	{
		retBuf = (mem_buf*)_MemPoolAlloc(Pool, newAllocSize);
		retBuf->Size = 0;
	}
	retBuf->Capacity = newCapacity;
	retBuf->Pool = Pool;
	return (void*)retBuf->BufferData;
}

// Takes in a mem_buf char*, writes the given formatted string in
// Extends the mem_buf capacity (from pool realloc) if too small
void StrCat(char **StrBuf, const char *StrFmt, ...)
{
	// try vsnprintf the fmt'ed str in, grow the buffer if we couldnt write it all in
	va_list args;
	va_start(args, StrFmt);
	uint64 remainingCap = BufCapacity(*StrBuf) - BufSize(*StrBuf);
	uint64 strLen = vsnprintf(BufEnd(*StrBuf), remainingCap, StrFmt, args) + 1;
	va_end(args);
	if (strLen > remainingCap)
	{
		_MemBufCheckGrowth(StrBuf, BufSize(*StrBuf) + strLen);
		va_start(args, StrFmt);
		remainingCap = BufCapacity(*StrBuf) - BufSize(*StrBuf);
		strLen = vsnprintf(BufEnd(*StrBuf), remainingCap, StrFmt, args) + 1;
		va_end(args);
	}
	mem_buf__hdr(*StrBuf)->Size += strLen - 1;
}

// create a new mem_buf string from nothing
char *Str(mem_pool *Pool, const char *StrFmt, ...)
{
	va_list args;
	va_start(args, StrFmt);
	char c;
	uint64 strLen = vsnprintf(&c, 1, StrFmt, args) + 1;
	char *retBuf = Buf<char>(Pool, strLen);
	vsnprintf(retBuf, strLen, StrFmt, args);
	va_end(args);
	mem_buf__hdr(retBuf)->Size = strLen - 1;
	return retBuf;
}

void _ArenaGrow(mem_arena *Arena, uint64 MinSize)
{
	uint64 size = Max(MinSize, MEM_ARENA_BLOCK_SIZE);
	Arena->Ptr = PoolAlloc<uint8>(Arena->Pool, size);
	Arena->BlockEnd = Arena->Ptr + size;
	if (!Arena->Blocks)
	{
		Arena->Blocks = Buf<uint8*>(Arena->Pool);
	}
	BufPush(Arena->Blocks, Arena->Ptr);
}

void *_ArenaAlloc(mem_arena *Arena, mem_pool *Pool, uint64 Size, bool Reserve)
{
	if (Size > (uint64)(Arena->BlockEnd - Arena->Ptr))
	{
		Arena->Pool = Pool;
		_ArenaGrow(Arena, Size);
	}
	void *ptr = Arena->Ptr;
	if (!Reserve)
	{
		Arena->Ptr = Arena->Ptr + Size;
	}
	return ptr;
}

void ArenaFree(mem_arena *Arena)
{
	Assert(Arena->Pool);
	for (uint8 **it = Arena->Blocks; it != BufEnd(Arena->Blocks); ++it)
	{
		PoolFree(Arena->Pool, *it);
	}
	BufFree(Arena->Blocks);
	Arena->Pool = nullptr;
	Arena->Ptr = Arena->BlockEnd = nullptr;
}

void ConcatStrings(path Dst, path const Str1, path const Str2)
{
    strncpy(Dst, Str1, MAX_PATH);
    strncat(Dst, Str2, MAX_PATH);
}

bool DiskFileExists(path const Filename)
{
    FILE *fp = fopen(Filename, "rb");
    if(fp)
    {
        fclose(fp);
        return true;
    }
    return false;
}

void *ReadFileContentsNoContext(path const Filename, int32 *FileSize)
{
    char *Contents = NULL;
    FILE *fp = fopen(Filename, "rb");

    if(fp)
    {
        if(0 == fseek(fp, 0, SEEK_END))
        {
            int32 Size = ftell(fp);
            rewind(fp);
            Contents = (char*)calloc(1, Size+1);
            size_t Read = fread(Contents, Size, 1, fp);
            if(Read != 1)
            {
                printf("File Open Error [%s] : Reading error or EOF reached.\n", Filename);
            }
            Contents[Size] = 0;
            if(FileSize)
            {
                *FileSize = Size+1;
            }
        }
        else
        {
			printf("File Open Error [%s] : fseek not 0.\n", Filename);
        }
        fclose(fp);
    }
    else
    {
		printf("File Open Error [%s] : Couldn't open file.\n", Filename);
    }

    return (void*)Contents;
}

void *ReadFileContents(context *Context, path const Filename, int32 *FileSize)
{
    char *Contents = NULL;
    FILE *fp = fopen(Filename, "rb");

    if(fp)
    {
        if(0 == fseek(fp, 0, SEEK_END))
        {
            int32 Size = ftell(fp);
            rewind(fp);
			Contents = PoolAlloc<char>(Context->ScratchPool, Size + 1);
            size_t Read = fread(Contents, Size, 1, fp);
            if(Read != 1)
            {
                LogError("File Open Error [%s] : Reading error or EOF reached.", Filename);
            }
            Contents[Size] = 0;
            if(FileSize)
            {
                *FileSize = Size+1;
            }
        }
        else
        {
            LogError("File Open Error [%s] : fseek not 0.", Filename);
        }
        fclose(fp);
    }
    else
    {
        LogError("File Open Error [%s] : Couldn't open file.", Filename);
    }

    return (void*)Contents;
}

int FindFirstOf(char const *Str, char charToFind)
{
    int idx = -1;
    char const *pStr = Str;
    while(pStr && *pStr && *pStr != charToFind)
    {
        idx++;
        pStr++;
    }
    if(idx >= 0) idx++; // to account for starting at -1
    return idx;
}

size_t GetDateTime(char *Dst, size_t DstSize, char const *Fmt)
{
    time_t rawtime;
    struct tm * timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    return strftime(Dst, DstSize, Fmt, timeinfo);
}

int    UTF8CharCount(char const *Str, uint16 *OutUnicode)
{
    if(strlen(Str) == 0) return -1;

    int CharCount, Unicode;

    uint8 BaseCH = Str[0];
    if(BaseCH <= 0x7F) // <128, normal char
    {
        Unicode = BaseCH;
        CharCount = 1;
    }
    else if(BaseCH <= 0xBF)
    {
        return -1; // Not an UTF-8 char
    }
    else if(BaseCH <= 0xDF) // <223, 1 more char follows
    {
        Unicode = BaseCH & 0x1F;
        CharCount = 2;
    }
    else if(BaseCH <= 0xEF) // <239, 2 more char follows
    {
        Unicode = BaseCH & 0x0F;
        CharCount = 3;
    }
    else if(BaseCH <= 0xF7) // <247, 3 more char follows
    {
        Unicode = BaseCH & 0x07;
        CharCount = 4;
    }
    else
    {
        return -1; // Not an UTF-8 Char
    }

    if(OutUnicode)
        *OutUnicode = Unicode;

    return CharCount;
}

uint32 UTF8Len(char const *Str, uint32 MaxChar)
{
	uint32 Len = 0;
	uint32 i = 0;
    while(Str[i] && i < MaxChar)
    {
        Len += (Str[i++] & 0xc0) != 0x80;
    }

    return Len;
}

uint16 UTF8CharToInt(char const *Str, size_t *CharAdvance)
{
    uint16 Unicode = 0;
    uint8 BaseCH = Str[0];

    int Count = UTF8CharCount(Str, &Unicode);
    if(Count < 0)
        return 0;

    *CharAdvance = Count; // for the leading char

    for (size_t j = 1; j < *CharAdvance; ++j)
    {
        uint8 ch = Str[j];
        if (ch < 0x80 || ch > 0xBF)
            return 0; // Not an UTF-8
        Unicode <<= 6;
        Unicode += ch & 0x3F;
    }

    return Unicode;
}

char *GetFirstNonWhitespace( char *Src )
{
	while ( Src && *Src && *Src == ' ' ) Src++;
	return Src;
}

struct cpu_info
{
	int CPUCount;
	double CPUGHz;
	bool x64;
	char CPUVendor[0x20];
	char CPUBrand[0x40];
};

/// Platform dependent functions
#ifdef RF_WIN32
#include <Windows.h>
#include <intrin.h>
#include <powerbase.h>

struct mem_status
{
	size_t availVirtual;
	size_t totalVirtual;
	size_t totalPhysical;
};

// not in the SDK anymore, redefining as per MSDN
typedef struct _PROCESSOR_POWER_INFORMATION {
	ULONG Number;
	ULONG MaxMhz;
	ULONG CurrentMhz;
	ULONG MhzLimit;
	ULONG MaxIdleState;
	ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;

// NOTE : expect a MAX_PATH string as Path
void GetExecutablePath(path Path)
{
    HMODULE ExecHandle = GetModuleHandleW(NULL);
    GetModuleFileNameA(ExecHandle, Path, MAX_PATH);

    char *LastPos = strrchr(Path, '\\');
    unsigned int NumChar = (unsigned int)(LastPos - Path) + 1;

    // Null-terminate the string at that position before returning
    Path[NumChar] = 0;
}

void DiskFileCopy(path const DstPath, path const SrcPath)
{
    CopyFileA(SrcPath, DstPath, FALSE);
}

void PlatformSleep(uint32 MillisecondsToSleep)
{
    Sleep(MillisecondsToSleep);
}

static bool CheckSSESupport()
{
	int CPUInfo[4];
	__cpuid( CPUInfo, 1 );
	return CPUInfo[3] & ( 1 << 25 ) || false;
}

static void GetCpuInfo( cpu_info &CPUInfo )
{
	int cpuInfo[4] = { -1 };
	__cpuid( cpuInfo, 0 );
	int nIDs = cpuInfo[0];


	memset( CPUInfo.CPUVendor, 0, sizeof( CPUInfo.CPUVendor ) );
	*reinterpret_cast<int*>( CPUInfo.CPUVendor ) = cpuInfo[1];
	*reinterpret_cast<int*>( CPUInfo.CPUVendor + 4 ) = cpuInfo[3];
	*reinterpret_cast<int*>( CPUInfo.CPUVendor + 8 ) = cpuInfo[2];

	if ( !strcmp( CPUInfo.CPUVendor, "GenuineIntel" ) )
	{
		memcpy( CPUInfo.CPUVendor, "Intel", 6 );
	}
	else if ( !strcmp( CPUInfo.CPUVendor, "AuthenticAMD" ) )
	{
		memcpy( CPUInfo.CPUVendor, "AMD", 4 );
	}
	else
	{
		memcpy( CPUInfo.CPUVendor, "Unknown CPU", 13 );
	}

	__cpuid( cpuInfo, 0x80000000 );
	int nExIDs = cpuInfo[0];

	memset( CPUInfo.CPUBrand, 0, sizeof( CPUInfo.CPUBrand ) );

	if ( nExIDs >= 0x80000004 )
	{
		__cpuid( cpuInfo, 0x80000002 );
		memcpy( CPUInfo.CPUBrand, cpuInfo, 4 * sizeof( int ) );
		__cpuid( cpuInfo, 0x80000003 );
		memcpy( CPUInfo.CPUBrand + 16, cpuInfo, 4 * sizeof( int ) );
		__cpuid( cpuInfo, 0x80000004 );
		memcpy( CPUInfo.CPUBrand + 32, cpuInfo, 4 * sizeof( int ) );
	}


	SYSTEM_INFO sysinfo;
	GetSystemInfo( &sysinfo );
	CPUInfo.CPUCount = sysinfo.dwNumberOfProcessors;

	size_t PowerInfoBufSize = sysinfo.dwNumberOfProcessors * sizeof( PROCESSOR_POWER_INFORMATION );
	PROCESSOR_POWER_INFORMATION *PowerInfoBuf = (PROCESSOR_POWER_INFORMATION*)malloc( PowerInfoBufSize );
	LONG ntpi = CallNtPowerInformation( ProcessorInformation, NULL, 0, PowerInfoBuf, (ULONG)PowerInfoBufSize );

	CPUInfo.CPUGHz = PowerInfoBuf[0].MaxMhz * 0.001;

	free( PowerInfoBuf );
}

static void QueryMemStatus( mem_status *mstat )
{
	MEMORYSTATUSEX statusex;
	HMODULE hm;

	hm = GetModuleHandle( L"kernel32.dll" );
	bool ( WINAPI *MemoryStatusEx ) ( IN OUT LPMEMORYSTATUSEX lpBuf );
	if ( hm )
	{
		MemoryStatusEx = ( bool( WINAPI* )( IN OUT LPMEMORYSTATUSEX ) ) GetProcAddress( hm, "GlobalMemoryStatusEx" );
		if ( MemoryStatusEx )
		{
			statusex.dwLength = sizeof( statusex );
			if ( MemoryStatusEx( &statusex ) )
			{
				mstat->availVirtual = statusex.ullAvailVirtual;
				mstat->totalVirtual = statusex.ullTotalVirtual;
				mstat->totalPhysical = statusex.ullTotalPhys;
				return;
			}
		}
	}
}

static void GetOSVersion( os_version *OsVersion )
{
    memcpy(OsVersion->OSName, "Windows", 7);

	HMODULE hm = GetModuleHandleW( L"ntdll.dll" );

	RTL_OSVERSIONINFOW rovi;
	memset( &rovi, 0, sizeof( rovi ) );

	typedef NTSTATUS( WINAPI *RtlGetVersionPtr )( PRTL_OSVERSIONINFOW );
	if ( hm )
	{
		RtlGetVersionPtr Function = (RtlGetVersionPtr)GetProcAddress( hm, "RtlGetVersion" );
		if ( Function )
		{
			Function( &rovi );
		}
	}
	OsVersion->Major = rovi.dwMajorVersion;
	OsVersion->Minor = rovi.dwMinorVersion;
	OsVersion->Build = rovi.dwBuildNumber;
}

void GetSystemInfo( system_info &SysInfo )
{
	memset( &SysInfo, 0, sizeof( system_info ) );

	cpu_info CPUInfo;
	GetCpuInfo( CPUInfo );

	mem_status memstat = { 0 };
	QueryMemStatus( &memstat );
	int SystemMemMB = static_cast<int>( ceilf( memstat.totalPhysical / float( MB ) ) );

	bool SSE = CheckSSESupport();

	SysInfo.CPUCountLogical = CPUInfo.CPUCount;
	SysInfo.CPUGHz = CPUInfo.CPUGHz;
	SysInfo.SystemMB = SystemMemMB;
	SysInfo.SSESupport = SSE;
	memcpy( SysInfo.CPUName, CPUInfo.CPUVendor, sizeof( SysInfo.CPUName ) );
	memcpy( SysInfo._CPUBrand, CPUInfo.CPUBrand, sizeof( SysInfo._CPUBrand ) );
	SysInfo.CPUBrand = SysInfo._CPUBrand;
	SysInfo.CPUBrand = GetFirstNonWhitespace( SysInfo.CPUBrand );
	GetOSVersion( &SysInfo.OSVersion );
}

char *GetClipboardContent()
{
	char *content = nullptr;
	char *tmpClipboardText;

	if ( OpenClipboard( NULL ) != 0 )
	{
		HANDLE hc;

		if ( ( hc = GetClipboardData( CF_TEXT ) ) != 0 )
		{
			tmpClipboardText = (char*)GlobalLock( hc );
			if ( tmpClipboardText )
			{
				content = (char*)malloc( GlobalSize( hc ) + 1 );
				strncpy( content, tmpClipboardText, GlobalSize( hc ) );
				GlobalUnlock( hc );
				// remove trailing
				//strtok( content, "\n\b\r" );
			}
		}
		CloseClipboard();
	}

	return content;
}

void SetClipboardContent( char *Content )
{
	if ( OpenClipboard( NULL ) != 0 )
	{
		EmptyClipboard();
		HGLOBAL ContentCpy = GlobalAlloc( GMEM_MOVEABLE, strlen( Content ) + 1 );
		if ( !ContentCpy )
		{
			CloseClipboard();
			return;
		}

		LPSTR Str = (LPSTR)GlobalLock( ContentCpy );
		strcpy( Str, Content );
		GlobalUnlock( ContentCpy );

		SetClipboardData( CF_TEXT, ContentCpy );

		CloseClipboard();
	}
}

#else
#ifdef RF_UNIX
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>

// NOTE : expect a MAX_PATH string as Path
void GetExecutablePath(path Path)
{
    struct stat Info;
    path StatProc;

    pid_t pid = getpid();
    snprintf(StatProc, MAX_PATH, "/proc/%d/exe", pid);
    ssize_t BytesRead = readlink(StatProc, Path, MAX_PATH);
    if(-1 == BytesRead)
    {
        printf("Fatal Error : Can't query Executable path.\n");
        return;;
    }

    Path[BytesRead] = 0;

    char *LastPos = strrchr(Path, '/');
    unsigned int NumChar = (unsigned int)(LastPos - Path) + 1;

    // Null-terminate the string at that position before returning
    Path[NumChar] = 0;
}

static void CopyFile(path const Src, path const Dst)
{
    // NOTE - No CopyFile on Linux : open Src, read it and copy it in Dst
    int SFD = open(Src, O_RDONLY);
    int DFD = open(Dst, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    char FileBuf[4 * KB];

    // Read until we can't anymore
    while(1)
    {
        ssize_t Read = read(SFD, FileBuf, sizeof(FileBuf));
        if(!Read)
        {
            break;
        }
        ssize_t Written = write(DFD, FileBuf, Read);
        if(Read != Written)
        {
            printf("Copy File Error [%s].\n", Dst);
        }
    }

    close(SFD);
    close(DFD);
}

void DiskFileCopy(path const DstPath, path const SrcPath)
{
    CopyFile(SrcPath, DstPath);
}

void PlatformSleep(uint32 MillisecondsToSleep)
{
    struct timespec TS;
    TS.tv_sec = MillisecondsToSleep / 1000;
    TS.tv_nsec = (MillisecondsToSleep % 1000) * 1000000;
    nanosleep(&TS, NULL);
}

static void GetCpuInfo( cpu_info &cpuInfo )
{
    memcpy(cpuInfo.CPUVendor, "Unknown CPU", 13);
    memcpy(cpuInfo.CPUBrand, "Unknown", 7);
    cpuInfo.CPUCount = get_nprocs();

    // get more data from cpuinfo
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if(fp)
    {
        size_t len;
        ssize_t bytesRead;
        char *line = NULL;

        int procCount = 0;
        bool vendorStrFound = false;

        while((bytesRead = getline(&line, &len, fp)) != -1)
        {
            if(!strncmp(line, "processor", 9))
            {
                if(procCount == 1) // only read the 1st proc entryy
                    break;
                procCount++;
            }

            if(!strncmp(line, "vendor_id", 9))
            { // CPU vendor
                int vendorIdIdx = FindFirstOf(line, ':');
                if(vendorIdIdx >= 0)
                {
                    vendorIdIdx += 2; // first char of the vendor string
                    char *vendorIdStr = line + vendorIdIdx;
                    if(!strncmp(vendorIdStr, "GenuineIntel", 12))
                        memcpy(cpuInfo.CPUVendor, "Intel", 6);
                    else if(!strncmp(vendorIdStr, "AuthenticAMD", 12))
                        memcpy(cpuInfo.CPUVendor, "AMD", 4);
                }
            }

            if(!strncmp(line, "model name", 10))
            { // CPU model
                int brandIdIdx = FindFirstOf(line, ':');
                if(brandIdIdx >= 0)
                {
                    brandIdIdx += 2;
                    char *brandIdStr = line + brandIdIdx;
                    size_t brandIdStrLen = strlen(brandIdStr);
                    brandIdStrLen -= 1; // remove trailing \n
                    memcpy(cpuInfo.CPUBrand, brandIdStr, brandIdStrLen);

                    // get Ghz from that string (reported frequency, not actual on unix...)
                    int freqIdx = FindFirstOf(brandIdStr, '@');
                    freqIdx += 2;
                    char *freqStr = line + brandIdIdx + freqIdx;
                    int freqIdxEnd = FindFirstOf(freqStr, 'G');
                    char freqTmpStr[8] = { 0 };
                    memcpy(freqTmpStr, freqStr, freqIdxEnd);
                    cpuInfo.CPUGHz = atof(freqTmpStr);
                }
            }

            free(line); line = NULL;
        }

        fclose(fp);
    }
}

static void GetOSVersion(os_version *osVersion)
{
    memcpy(osVersion->OSName, "Unix", 4);
}

void GetSystemInfo( system_info &SysInfo )
{
	// TODO - Unix Version
	memset( &SysInfo, 0, sizeof( system_info ) );

    cpu_info cpuInfo;
    GetCpuInfo(cpuInfo);

    SysInfo.CPUCountLogical = cpuInfo.CPUCount;
	SysInfo.CPUGHz = cpuInfo.CPUGHz;
    //SysInfo.SystemMB = SystemMemMB;
	//SysInfo.SSESupport = SSE;
	memcpy( SysInfo.CPUName, cpuInfo.CPUVendor, sizeof( SysInfo.CPUName ) );
	memcpy( SysInfo._CPUBrand, cpuInfo.CPUBrand, sizeof( SysInfo._CPUBrand ) );
	SysInfo.CPUBrand = SysInfo._CPUBrand;
	SysInfo.CPUBrand = GetFirstNonWhitespace( SysInfo.CPUBrand );
	GetOSVersion( &SysInfo.OSVersion );
}

char *GetClipboardContent()
{
	// TODO - Unix Version
	printf( "GetClipboardContent not implemented on Unix.\n" );
	return nullptr;
}

void SetClipboardContent( char *Content )
{
	// TODO - Unix Version
	printf( "SetClipboardContent not implemented on Unix.\n" );
}

#endif
#endif
}
