#ifndef RF_CONTEXT_H
#define RF_CONTEXT_H

#include "render.h"

#if RADAR_WIN32
#define NOMINMAX
#include <windows.h>
#else
#endif
#include "GLFW/glfw3.h"

namespace rf {
#define MAX_SHADERS 32

struct context_descriptor
{
    memory_arena *SessionArena;
    memory_arena *ScratchArena;

    real32 WindowX, WindowY;        // position (topleft origin)
    int WindowWidth, WindowHeight;  // size
    bool VSync;
    real32 FOV;
    real32 NearPlane, FarPlane;
    path ExecutableName;
};

struct context
{
    // The context (thus the RF library) is initialized with access to
    // both a Session long arena pool for storing rendering resources, 
    // as well as a Scratch pool for storing frame-long temporary data
    memory_arena *SessionArena;
    memory_arena *ScratchArena;

    render_resources RenderResources;

    GLFWwindow *Window;

    mat4f ProjectionMatrix3D;
    mat4f ProjectionMatrix2D;

    bool WireframeMode;
    vec4f ClearColor;

    real32 FOV;
    int WindowWidth;
    int WindowHeight;
    real32 NearPlane, FarPlane;

    real32 WindowSizeLogLevel;

    // Shader program for post processing
    uint32 ProgramPostProcess;

    uint32 Shaders3D[MAX_SHADERS];
    uint32 Shaders3DCount;
    uint32 Shaders2D[MAX_SHADERS];
    uint32 Shaders2DCount;

    bool IsRunning;
    bool IsValid;
};

namespace ctx
{
    enum cursor_type
    {
        CURSOR_NORMAL,
        CURSOR_HRESIZE,
        CURSOR_VRESIZE
    };

    context *Init(context_descriptor const *Desc);
    void Destroy(context *Context);

    bool WindowResized(context *Context);
    void UpdateShaderProjection(context *Context);
    void GetFrameInput(context *Context, input *Input);

    void RegisteredShaderClear(context *Context);
    void RegisterShader3D(context *Context, uint32 ProgramID);
    void RegisterShader2D(context *Context, uint32 ProgramID);

    path const &GetExePath(context *Context);

    void *AllocScratch(context *Context, size_t Size);

    void SetCursor(context *Context, cursor_type CursorType);
    void ShowCursor(context *Context, bool Val);
    GLenum SetWireframeMode(context *Context, GLenum Mode = 0);
}
}

#endif
