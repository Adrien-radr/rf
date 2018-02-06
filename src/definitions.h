#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include "radar_common.h"

#define POOL_OFFSET(Pool, Structure) ((uint8*)(Pool) + sizeof(Structure))
#define PushArenaStruct(Arena, Struct) _PushArenaData((Arena), sizeof(Struct))
#define PushArenaData(Arena, Size) _PushArenaData((Arena), (Size))

// TODO - See if 256 is enough for more one liner ui strings
#define CONSOLE_CAPACITY 128
#define CONSOLE_STRINGLEN 256
#define UI_STRINGLEN 256
#define UI_MAXSTACKOBJECT 256

#define KEY_HIT(KeyState) ((KeyState >> 0x1) & 1)
#define KEY_UP(KeyState) ((KeyState >> 0x2) & 1)
#define KEY_DOWN(KeyState) ((KeyState >> 0x3) & 1)
#define MOUSE_HIT(MouseState) KEY_HIT(MouseState)
#define MOUSE_UP(MouseState) KEY_UP(MouseState)
#define MOUSE_DOWN(MouseState) KEY_DOWN(MouseState)

namespace rf {
struct context;

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

inline void *_PushArenaData(memory_arena *Arena, uint64 Size)
{
    Assert(Arena->Size + Size <= Arena->Capacity);
    void *MemoryPtr = Arena->BasePtr + Arena->Size;
    Arena->Size += Size;
    //printf("Current ArenaSize is %llu [max %llu]\n", Arena->Size, Arena->Capacity);

    return (void*)MemoryPtr;
}

typedef uint8 key_state;
typedef uint8 mouse_state;

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

    key_state KeyW;
    key_state KeyA;
    key_state KeyS;
    key_state KeyD;
    key_state KeyR;
    key_state KeyF;
    key_state KeyLShift;
    key_state KeyLCtrl;
    key_state KeyLAlt;
    key_state KeySpace;
    key_state KeyF1;
    key_state KeyF2;
    key_state KeyF3;
    key_state KeyF11;
    key_state KeyNumPlus;
    key_state KeyNumMinus;
    key_state KeyNumMultiply;
    key_state KeyNumDivide;
    key_state KeyTilde;
    key_state KeyLeft;
    key_state KeyRight;
    key_state KeyUp;
    key_state KeyDown;

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

    struct frame_stack
    {
        text_line *TextLines[UI_MAXSTACKOBJECT];
        uint32 TextLineCount;
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
