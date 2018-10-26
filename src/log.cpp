#include <cstdarg>
#include <algorithm>
#include "log.h"
#include "utils.h"
#include "context.h"

namespace rf {
namespace log {
    context         *Context;

    FILE static     *LogFile;
    double static   LogTime = 0.0;
    path            LogFilename = "radar.log";
#if RF_WIN32
	HANDLE			hConsole;
#endif

    void Init(context *Context)
    {
        //System->ConsoleLog = (console_log*)PushArenaStruct(&Memory->SessionArena, console_log);

        path LogPath;
        ConcatStrings(LogPath, ctx::GetExePath(Context), LogFilename);
        LogFile = fopen(LogPath, "w");
        if(!LogFile)
        {
            printf("Error opening engine log file %s\n", LogPath);
        }

		char CurrDate[128], CurrTime[64];
		size_t WrittenChar = GetDateTime(CurrDate, 64, DEFAULT_DATE_FMT);
        CurrTime[0] = ' ';
        GetDateTime(CurrTime + 1, 63, DEFAULT_TIME_FMT);
        strncat(CurrDate + WrittenChar, CurrTime, 64);

#if RF_WIN32
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif

        LogInfo("Radar Foundation Log (RF %d.%d.%d)", RF_MAJOR, RF_MINOR, RF_PATCH);
#ifdef DEBUG
        LogInfo("Debug Build");
#else
        LogInfo("Release Build");
#endif
        LogInfo("%s", CurrDate);
        LogInfo( "========================" );
    }

    void Destroy()
    {
        if(LogFile)
        {
            char CurrTime[64];
            GetDateTime(CurrTime, 64, DEFAULT_TIME_FMT);
            LogInfo("Radar Foundation Log End. %s\n", CurrTime);
            fclose(LogFile);
        }
    }

    static void ConsoleLog(console_log *Log, char const *String, size_t CharCount)
    {
        CharCount = std::min(size_t(CONSOLE_STRINGLEN-1), CharCount);
        strncpy(Log->MsgStack[Log->WriteIdx], String, CharCount);
        Log->MsgStack[Log->WriteIdx][CharCount] = 0; // EOL
        Log->WriteIdx = (Log->WriteIdx + 1) % CONSOLE_CAPACITY;

        if(Log->StringCount >= CONSOLE_CAPACITY)
        {
            Log->ReadIdx = (Log->ReadIdx + 1) % CONSOLE_CAPACITY;
        }
        else
        {
            Log->StringCount++;
        }
    }

    void _Msg(log_level LogLevel, char const *File, int Line, char const *Fmt, ...)
    {
        static char const *LogLevelStr[] = {
            "II",
            "EE",
            "DB"
        };
		// TODO - those are for WIN32, find a way to do the same for Unix
		// default color (white on black) is 7
		static int LogLevelColor[] = {
			3,
			4,
			6
		};

        char LocalBuf[256];
        va_list args;
        va_start(args, Fmt);
        vsnprintf(LocalBuf, 256, Fmt, args);
        va_end(args);

        char Str[512];
        int CharCount = 0;

        // STD Output
#if RF_WIN32
		if (LogLevel == LOG_DEBUG || LogLevel == LOG_ERROR)
			CharCount = snprintf(Str, 512, "<%s:%d> %s", File, Line, LocalBuf);
		else
			CharCount = snprintf(Str, 512, "%s", LocalBuf);

		SetConsoleTextAttribute(hConsole, LogLevelColor[LogLevel]);
		printf("%s ", LogLevelStr[LogLevel]);
		SetConsoleTextAttribute(hConsole, 7);
		printf("%s\n", Str);
#else
		if (LogLevel == LOG_DEBUG || LogLevel == LOG_ERROR)
			CharCount = snprintf(Str, 512, "%s <%s:%d> %s", LogLevelStr[LogLevel], File, Line, LocalBuf);
		else
			CharCount = snprintf(Str, 512, "%s %s", LogLevelStr[LogLevel], LocalBuf);
		printf("%s\n", Str);
#endif

        // FILE Output
        fprintf(LogFile, "%s\n", Str);

        // CONSOLE Output
        //ConsoleLog(System->ConsoleLog, Str, CharCount);
    }
}
}
