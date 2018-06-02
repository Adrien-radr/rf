#include <ctime>
#include "utils.h"
#include "context.h"

namespace rf {
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
            Contents = (char*)ctx::AllocScratch(Context, Size+1);
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

size_t UTF8Len(char const *Str, size_t MaxChar)
{
    size_t Len = 0;
    size_t i = 0;
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

/// Platform dependent functions
#ifdef RF_WIN32
#include <Windows.h>
#include <intrin.h>
#include <powerbase.h>

struct cpu_info
{
	int CPUCount;
	double CPUGHz;
	bool x64;
	char CPUVendor[0x20];
	char CPUBrand[0x40];
};

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

void GetSystemInfo( system_info &SysInfo )
{
	// TODO - Unix Version
	memset( &SysInfo, 0, sizeof( system_info ) );
	printf( "GetSystemInfo not implemented on Unix.\n" );
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
