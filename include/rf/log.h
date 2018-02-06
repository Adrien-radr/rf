#ifndef LOG_H
#define LOG_H

#include "rf_defs.h"
#include <string>

namespace rf
{
namespace log
{
    enum log_level
    {
        LOG_INFO,
        LOG_ERROR,
        LOG_DEBUG
    };

    void Init(context *Context);
    void Destroy();
    void _Msg(log_level LogLevel, char const *File, int Line, char const *Fmt, ... );
}
}

#define LogInfo(...) do {\
    rf::log::_Msg(rf::log::LOG_INFO, __FILE__, __LINE__, ##__VA_ARGS__);\
} while(0)

#define LogError(...) do {\
    rf::log::_Msg(rf::log::LOG_ERROR, __FILE__, __LINE__, ##__VA_ARGS__);\
} while(0)

#define LogDebug(...) do {\
    D(rf::log::_Msg(rf::log::LOG_DEBUG, __FILE__, __LINE__, ##__VA_ARGS__);)\
} while(0)

#endif
