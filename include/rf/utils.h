#ifndef RF_UTILS_H
#define RF_UTILS_H

#include "rf_defs.h"
#include "log.h"
#include "cJSON.h"

namespace rf {
#define DEFAULT_DATE_FMT "%a %d %b %Y"
#define DEFAULT_TIME_FMT "%H:%M:%S"

/// Returns as a string in Dst the current date/time depending on the asked format
size_t  GetDateTime(char *Dst, size_t DstSize, char const *Fmt);
/// Ask the running thread to sleep for an amount of ms
void    PlatformSleep(uint32 MillisecondsToSleep);

/// Retrieve system information from the OS (refer to system_info struct)
void	GetSystemInfo(system_info &SysInfo);

/// Get and set the system's clipboard content
/// Returns a char string that HAS TO BE FREED by the caller
char	*GetClipboardContent();
void	SetClipboardContent(char *Content);

/// Concatenate 2 strings so that Dst = Str1 + Str2 (in std::string terms)
void    ConcatStrings(path Dst, path const Str1, path const Str2);

/// Fills the given path with the absolute path to the running executable
void    GetExecutablePath(path Path);
/// Checks if a file on disk exist and returns the result
bool    DiskFileExists(path const Filename);
/// Copy a file on disk
void    DiskFileCopy(path const DstPath, path const SrcPath);

/// Reads the content of Filename and returns it.
/// Also returns the file size in out-parameter if needed
/// Context is needed for the scratch alloc of opening the file
void    *ReadFileContents(context *Context, path const Filename, int32 *FileSize);

/// Same as previous, but doesn't necessitate the Context to call
/// This one is LESS recommended. It allocates the receiving buffer on the heap and 
/// the caller HAS to free it. It should only be used if the Context doesn't exist yet
void    *ReadFileContentsNoContext(path const Filename, int32 *FileSize);

/// Returns the index of the first iterance of the character in the given string
/// returns -1 if none is found
int     FindFirstOf(char const *Str, char charToFind);

/// Returns the number of byte characters that the UTF8 string is composed of
/// If 1, it is a normal ascii string
/// Returns -1 if the string is not UTF8
/// If Unicode is non NULL, it is filled with the trailing unicode first char of the string
int     UTF8CharCount(char const *Str, uint16 *Unicode = NULL);

/// Returns the length (number of characters) of an UTF8 string
uint32  UTF8Len(char const *Str, uint32 MaxChar = -1);

/// Converts the UTF8 string to an unsigned integers (e.g. for indexing)
uint16  UTF8CharToInt(char const *Str, size_t *CharAdvance);

/// Returns a pointer to the first non-whitespace character in a pointed string buffer
/// This does not erase anything
char *GetFirstNonWhitespace(char *Src);

template<typename T>
inline T JSON_Get(cJSON *Root, char const *ValueName, T const &DefaultValue)
{
}

template<>
inline int JSON_Get(cJSON *Root, char const *ValueName, int const &DefaultValue)
{
	if (Root)
	{
		cJSON *Obj = cJSON_GetObjectItem(Root, ValueName);
		if (Obj)
			return Obj->valueint;
	}
	return DefaultValue;
}

template<>
inline double JSON_Get(cJSON *Root, char const *ValueName, double const &DefaultValue)
{
	if (Root)
	{
		cJSON *Obj = cJSON_GetObjectItem(Root, ValueName);
		if (Obj)
			return Obj->valuedouble;
	}
	return DefaultValue;
}

template<>
inline vec3f JSON_Get(cJSON *Root, char const *ValueName, vec3f const &DefaultValue)
{
	if (Root)
	{
		cJSON *Obj = cJSON_GetObjectItem(Root, ValueName);
		if (Obj && cJSON_GetArraySize(Obj) == 3)
		{
			vec3f Ret;
			Ret.x = (real32)cJSON_GetArrayItem(Obj, 0)->valuedouble;
			Ret.y = (real32)cJSON_GetArrayItem(Obj, 1)->valuedouble;
			Ret.z = (real32)cJSON_GetArrayItem(Obj, 2)->valuedouble;
			return Ret;
		}
	}

	return DefaultValue;
}

template<>
inline vec4f JSON_Get(cJSON *Root, char const *ValueName, vec4f const &DefaultValue)
{
	if (Root)
	{
		cJSON *Obj = cJSON_GetObjectItem(Root, ValueName);
		if (Obj && cJSON_GetArraySize(Obj) == 4)
		{
			vec4f Ret;
			Ret.x = (real32)cJSON_GetArrayItem(Obj, 0)->valuedouble;
			Ret.y = (real32)cJSON_GetArrayItem(Obj, 1)->valuedouble;
			Ret.z = (real32)cJSON_GetArrayItem(Obj, 2)->valuedouble;
			Ret.w = (real32)cJSON_GetArrayItem(Obj, 3)->valuedouble;
			return Ret;
		}
	}

	return DefaultValue;
}

template<>
inline col4f JSON_Get(cJSON *Root, char const *ValueName, col4f const &DefaultValue)
{
	if (Root)
	{
		cJSON *Obj = cJSON_GetObjectItem(Root, ValueName);
		if (Obj && cJSON_GetArraySize(Obj) == 4)
		{
			col4f Ret;
			Ret.x = (real32)cJSON_GetArrayItem(Obj, 0)->valuedouble;
			Ret.y = (real32)cJSON_GetArrayItem(Obj, 1)->valuedouble;
			Ret.z = (real32)cJSON_GetArrayItem(Obj, 2)->valuedouble;
			Ret.w = (real32)cJSON_GetArrayItem(Obj, 3)->valuedouble;
			return Ret;
		}
	}

	return DefaultValue;
}

template<>
inline std::string JSON_Get(cJSON *Root, char const *ValueName, std::string const &DefaultValue)
{
	if (Root)
	{
		cJSON *Obj = cJSON_GetObjectItem(Root, ValueName);
		std::string Ret;
		if (Obj)
			return std::string(Obj->valuestring);
	}

	return DefaultValue;
}

}
#endif
