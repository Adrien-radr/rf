#include "context.h"
#include "utils.h"
#include "ui.h"
//#include "sound.h"

namespace rf {

bool static FramePressedKeys[KEY_LAST+1] = {};
bool static FrameReleasedKeys[KEY_LAST+1] = {};
bool static FrameDownKeys[KEY_LAST+1] = {};

int  static FrameModKeys = 0;

bool static FramePressedMouseButton[8] = {};
bool static FrameDownMouseButton[8] = {};
bool static FrameReleasedMouseButton[8] = {};

int  static FrameMouseWheel = 0;

int  static ResizeWidth;
int  static ResizeHeight;
bool static Resized = true;

GLFWcursor static *CursorNormal = NULL;
GLFWcursor static *CursorHResize = NULL;
GLFWcursor static *CursorVResize = NULL;

void ProcessKeyboardEvent(GLFWwindow *Window, int Key, int Scancode, int Action, int Mods)
{
    if(Action == GLFW_PRESS)
    {
        FramePressedKeys[Key] = true;
        FrameDownKeys[Key] = true;
        FrameReleasedKeys[Key] = false;
    }
    if(Action == GLFW_RELEASE)
    {
        FramePressedKeys[Key] = false;
        FrameDownKeys[Key] = false;
        FrameReleasedKeys[Key] = true;
    }

    FrameModKeys = Mods;
}

void ProcessMouseButtonEvent(GLFWwindow* Window, int Button, int Action, int Mods)
{
    if(Action == GLFW_PRESS)
    {
        FramePressedMouseButton[Button] = true;
        FrameDownMouseButton[Button] = true;
        FrameReleasedMouseButton[Button] = false;
    }
    if(Action == GLFW_RELEASE)
    {
        FramePressedMouseButton[Button] = false;
        FrameDownMouseButton[Button] = false;
        FrameReleasedMouseButton[Button] = true;
    }

    FrameModKeys = Mods;
}

void ProcessMouseWheelEvent(GLFWwindow *Window, double XOffset, double YOffset)
{
    FrameMouseWheel = (int) YOffset;
}

void ProcessWindowSizeEvent(GLFWwindow *Window, int Width, int Height)
{
    Resized = true;
    ResizeWidth = Width;
    ResizeHeight = Height;
}

void ProcessErrorEvent(int Error, const char* Description)
{
    LogInfo("GLFW Error : %s\n", Description);
}

key_state BuildKeyState(int32 Key)
{
    key_state State = 0;
    State |= (FramePressedKeys[Key] << 0x1);
    State |= (FrameReleasedKeys[Key] << 0x2);
    State |= (FrameDownKeys[Key] << 0x3);

    return State;
}

mouse_state BuildMouseState(int32 MouseButton)
{
    mouse_state State = 0;
    State |= (FramePressedMouseButton[MouseButton] << 0x1);
    State |= (FrameReleasedMouseButton[MouseButton] << 0x2);
    State |= (FrameDownMouseButton[MouseButton] << 0x3);

    return State;
}

namespace ctx {
    void RegisterShader3D(context *Context, uint32 ProgramID)
    {
        Assert(Context->Shaders3DCount < MAX_SHADERS);
        Context->Shaders3D[Context->Shaders3DCount++] = ProgramID;
    }

    void RegisterShader2D(context *Context, uint32 ProgramID)
    {
        Assert(Context->Shaders2DCount < MAX_SHADERS);
        Context->Shaders2D[Context->Shaders2DCount++] = ProgramID;
    }

    void RegisteredShaderClear(context *Context)
    {
        Context->Shaders3DCount = 0;
        Context->Shaders2DCount = 0;
    }

    void UpdateShaderProjection(context *Context)
    {
        // Notify the shaders that uses it
        for(uint32 i = 0; i < Context->Shaders3DCount; ++i)
        {
            glUseProgram(Context->Shaders3D[i]);
            SendMat4(glGetUniformLocation(Context->Shaders3D[i], "ProjMatrix"), Context->ProjectionMatrix3D);
        }

        for(uint32 i = 0; i < Context->Shaders2DCount; ++i)
        {
            glUseProgram(Context->Shaders2D[i]);
            SendMat4(glGetUniformLocation(Context->Shaders2D[i], "ProjMatrix"), Context->ProjectionMatrix2D);
        }
    }

    bool WindowResized(context *Context)
    {
        if(Resized)
        {
            Resized = false;

            glViewport(0, 0, ResizeWidth, ResizeHeight);
            Context->WindowWidth = ResizeWidth;
            Context->WindowHeight = ResizeHeight;
            Context->ProjectionMatrix3D = mat4f::Perspective(Context->FOV, 
                    Context->WindowWidth / (real32)Context->WindowHeight, Context->NearPlane, Context->FarPlane);
            Context->ProjectionMatrix2D = mat4f::Ortho(0.f, (float)Context->WindowWidth, 0.f, (float)Context->WindowHeight, 0.1f, 1.f);
            Context->WindowSizeLogLevel = log2f((float)Max(Context->WindowHeight, Context->WindowWidth));

            UpdateShaderProjection(Context);
            return true;
        }
        return false;
    }

    context *Init(context_descriptor const *Desc)
    {
        bool GLFWValid = false, GLEWValid = false;//, SoundValid = false;

        context *Context = (context*)PushArenaData(Desc->SessionArena, sizeof(context));
        Context->SessionArena = Desc->SessionArena;
        Context->ScratchArena = Desc->ScratchArena;

        GetExecutablePath(Context->RenderResources.ExecutablePath);

        // Init log
        log::Init(Context);

		GetSystemInfo( Context->SysInfo );
		LogInfo( "%s %u.%u.%u", Context->SysInfo.OSVersion.OSName, Context->SysInfo.OSVersion.Major, Context->SysInfo.OSVersion.Minor, Context->SysInfo.OSVersion.Build );
		LogInfo( "CPU : [%s] %s, %d cores at %.2lf GHz", Context->SysInfo.CPUName, Context->SysInfo.CPUBrand, Context->SysInfo.CPUCountLogical, Context->SysInfo.CPUGHz );
		LogInfo( "Using %d MB RAM", Context->SysInfo.SystemMB );
		LogInfo( "SSE Support : %s", Context->SysInfo.SSESupport ? "yes" : "no" );

        GLFWValid = glfwInit() == GLFW_TRUE;
        if(Context && GLFWValid)
        {
            glfwSetErrorCallback(ProcessErrorEvent);
#if 0
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
#endif

            Context->Window = glfwCreateWindow(Desc->WindowWidth, Desc->WindowHeight, Desc->ExecutableName, NULL, NULL);
            if(Context->Window)
            {
                glfwMakeContextCurrent(Context->Window);

                // TODO - Only in windowed mode for debug
                glfwSetWindowPos(Context->Window, (int)Desc->WindowX, (int)Desc->WindowY);
                glfwSwapInterval(Desc->VSync);

                glfwSetKeyCallback(Context->Window, ProcessKeyboardEvent);
                glfwSetMouseButtonCallback(Context->Window, ProcessMouseButtonEvent);
                glfwSetScrollCallback(Context->Window, ProcessMouseWheelEvent);
                glfwSetWindowSizeCallback(Context->Window, ProcessWindowSizeEvent);

                CursorNormal = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
                CursorHResize = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
                CursorVResize = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);

                GLEWValid = (GLEW_OK == glewInit());
                if(GLEWValid)
                {
                    GLubyte const *GLVendor = glGetString(GL_VENDOR);
                    GLubyte const *GLRenderer = glGetString(GL_RENDERER);
                    GLubyte const *GLVersion = glGetString(GL_VERSION);
                    LogInfo("GL Renderer %s, %s, %s", GLVendor, GLRenderer, GLVersion);
                    LogInfo("GLSL %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

                    int MaxLayers;
                    glGetIntegerv(GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS, &MaxLayers);
                    LogInfo("GL Max Array Layers : %d", MaxLayers);

                    int MaxSize;
                    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &MaxSize);
                    LogInfo("GL Max Texture Width : %d", MaxSize);

					int MaxTessPatchVertices = 0;
					glGetIntegerv(GL_MAX_PATCH_VERTICES, &MaxTessPatchVertices);
					LogInfo("GL Max Tesselation Patch Vertices : %d", MaxTessPatchVertices);

                    ResizeWidth = Desc->WindowWidth;
                    ResizeHeight = Desc->WindowHeight;
                    Context->WindowWidth = Desc->WindowWidth;
                    Context->WindowHeight = Desc->WindowHeight;
                    Context->FOV = Desc->FOV;
                    Context->NearPlane = Desc->NearPlane;
                    Context->FarPlane = Desc->FarPlane;

                    Context->WireframeMode = false;
					Context->EnableCull = true;

                    //Context->ClearColor = vec4f(0.01f, 0.19f, 0.31f, 0.f);
					Context->ClearColor = vec4f(0.9f, 0.9f, 0.9f, 1.0f);

                    glClearColor(Context->ClearColor.x, Context->ClearColor.y, Context->ClearColor.z, Context->ClearColor.w);

                    glEnable(GL_CULL_FACE);
                    glFrontFace(GL_CCW);
                    glCullFace(GL_BACK);

                    glDisable(GL_SCISSOR_TEST);

                    glEnable(GL_DEPTH_TEST);
                    glDepthMask(1);
                    glDepthFunc(GL_LESS);

                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                    glEnable(GL_POINT_SPRITE);
                    glEnable(GL_PROGRAM_POINT_SIZE);

                    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

					vec3i texColor = vec3i(255, 255, 255);
					Context->RenderResources.DefaultDiffuseTexture = (uint32*)PushArenaStruct(Context->SessionArena, uint32);
					*Context->RenderResources.DefaultDiffuseTexture = Make2DTexture((void*)&texColor, 1, 1, 3, false, false, 1,
						GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

					texColor = vec3i(127, 127, 255);
					Context->RenderResources.DefaultNormalTexture = (uint32*)PushArenaStruct(Context->SessionArena, uint32);
					*Context->RenderResources.DefaultNormalTexture = Make2DTexture((void*)&texColor, 1, 1, 3, false, false, 1,
						GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

					texColor = vec3i(0, 0, 0);
					Context->RenderResources.DefaultEmissiveTexture = (uint32*)PushArenaStruct(Context->SessionArena, uint32);
					*Context->RenderResources.DefaultEmissiveTexture = Make2DTexture((void*)&texColor, 1, 1, 3, false, false, 1,
						GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
                    //Context->RenderResources.DefaultDiffuseTexture = 
                        //ResourceLoad2DTexture(Context, "data/default_diffuse.png", false, false, 1);

                    //Context->RenderResources.DefaultNormalTexture= 
                        //ResourceLoad2DTexture(Context, "data/default_normal.png", false, false, 1);
                    
                    //Context->RenderResources.DefaultEmissiveTexture =
                        //ResourceLoad2DTexture(Context, "data/default_emissive.png", false, false, 1);
                }
                else
                {
                    LogInfo("Couldn't initialize GLEW.\n");
                }
            }
            else
            {
                LogInfo("Couldn't create GLFW Window.\n");
            }
        }
        else
        {
            LogInfo("Couldn't init GLFW.\n");
        }

        //SoundValid = sound::Init();

        if(GLFWValid && GLEWValid)// && SoundValid)
        {
            // NOTE - IsRunning might be better elsewhere ?
            Context->IsRunning = true;
            Context->IsValid = true;
        }

        ui::Init(Context);
        
        return Context;
    }

    void GetFrameInput(context *Context, input *Input)
    {
        memset(FrameReleasedKeys, 0, sizeof(FrameReleasedKeys));
        memset(FramePressedKeys, 0, sizeof(FramePressedKeys));
        memset(FrameReleasedMouseButton, 0, sizeof(FrameReleasedMouseButton));
        memset(FramePressedMouseButton, 0, sizeof(FramePressedMouseButton));

        FrameMouseWheel = 0;

        glfwPollEvents();

        // NOTE - The mouse position can go outside the Window bounds in windowed mode
        // See if this can cause problems in the future.
        real64 MX, MY;
        glfwGetCursorPos(Context->Window, &MX, &MY);
        Input->MousePosX = (int)MX;
        Input->MousePosY = (int)MY;
        Input->MouseDZ = FrameMouseWheel;

        if(glfwWindowShouldClose(Context->Window))
        {
            Context->IsRunning = false;
        }

        if(FrameReleasedKeys[KEY_ESCAPE])
        {
            Context->IsRunning = false;
        }

        // Get Player controls
        for(int i = KEY_FIRST; i <= KEY_LAST; ++i)
        {
            Input->Keys[(int)i] = BuildKeyState(i);
        }

        Input->MouseLeft = BuildMouseState(GLFW_MOUSE_BUTTON_LEFT);
        Input->MouseRight = BuildMouseState(GLFW_MOUSE_BUTTON_RIGHT);

        Input->dTimeFixed = 0.1f; // 100FPS
    }

    void Destroy(context *Context)
    {
        glDeleteProgram(Context->ProgramPostProcess);
        //sound::Destroy();
        ResourceFree(&Context->RenderResources);

        if(Context->Window)
        {
            glfwDestroyCursor(CursorNormal);
            glfwDestroyCursor(CursorHResize);
            glfwDestroyCursor(CursorVResize);
            glfwDestroyWindow(Context->Window);
        }
        glfwTerminate();

        log::Destroy();
    }

    path const &GetExePath(context *Context)
    {
        return Context->RenderResources.ExecutablePath;
    }

    void *AllocScratch(context *Context, size_t Size)
    {
        return PushArenaData(Context->ScratchArena, Size);
    }

    void SetCursor(context *Context, cursor_type CursorType)
    {
        GLFWcursor *c = NULL;
        switch(CursorType)
        {
            case CURSOR_NORMAL:
                c = CursorNormal; break;
            case CURSOR_HRESIZE:
                c = CursorHResize; break;
            case CURSOR_VRESIZE:
                c = CursorVResize; break;
            default:
                c = CursorNormal; break;
        }

        glfwSetCursor(Context->Window, c);
    }

    void ShowCursor(context *Context, bool Val)
    {
        if(Val)
            glfwSetInputMode(Context->Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        else
            glfwSetInputMode(Context->Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    GLenum SetWireframeMode(context *Context, GLenum Mode)
    {
        GLenum CurrWireframe = Context->WireframeMode ? GL_LINE : GL_FILL;
        if(0 == Mode)
        { // NOTE - toggle
            Context->WireframeMode = !Context->WireframeMode;
        }
        else
        { // NOTE - set
            Context->WireframeMode = Mode == GL_LINE ? true : false;
        }

        glPolygonMode(GL_FRONT_AND_BACK, Context->WireframeMode ? GL_LINE : GL_FILL);
        return CurrWireframe;
    }

	void SetCullMode(context *Context)
	{
		Context->EnableCull = !Context->EnableCull;

		if (Context->EnableCull)
			glEnable(GL_CULL_FACE);
		else
			glDisable(GL_CULL_FACE);
	}
}
}
