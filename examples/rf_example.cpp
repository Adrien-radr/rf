#include "rf/context.h"
#include "rf/ui.h"
#include "rf/log.h"

path ExeName = "Test RF App";
rf::context *Context = nullptr;

uint32 SessionMemSize = 512 * MB, ScratchMemSize = 64 * MB;
void *SessionPool = nullptr, *ScratchPool = nullptr;
rf::memory_arena SessionArena, ScratchArena;

uint32 PanelID = 0;
vec3i PanelPos = vec3i(0, 0, 0);
vec2i PanelSize = vec2i(140, 50);

uint32 Prog = 0;

void Destroy()
{
	rf::ctx::Destroy(Context);
	if (SessionPool) free(SessionPool);
	if (ScratchPool) free(ScratchPool);
}

rf::context_descriptor MakeCtxtDesc()
{
	// init some amount of zeroed mem for the two pools
	SessionPool = calloc(1, SessionMemSize);
	ScratchPool = calloc(1, ScratchMemSize);

	// set the arenas to manage them
	rf::InitArena(&SessionArena, SessionMemSize, SessionPool);
	rf::InitArena(&ScratchArena, ScratchMemSize, ScratchPool);

	// init the context descriptor and return it
	rf::context_descriptor desc = {};
	desc.SessionArena = &SessionArena;
	desc.ScratchArena = &ScratchArena;
	desc.WindowX = 600.f;
	desc.WindowY = 100.f;
	desc.WindowWidth = 800;
	desc.WindowHeight = 600;
	desc.VSync = false;
	desc.FOV = 45.f;
	desc.NearPlane = 0.1f;
	desc.FarPlane = 1000.f;
	memcpy(desc.ExecutableName, ExeName, MAX_PATH);

	return desc;
}

void LoadShaders()
{
	// clear all registered shaders
	rf::ctx::RegisteredShaderClear(Context);

	// (re)loads 2D UI rendering shaders
	rf::ui::ReloadShaders(Context);

	static const char *Vsrc =
		"#version 400\n"
		"layout(location=0) in vec2 position;\n"
		"layout(location=1) in vec2 texcoord;\n"
		"uniform mat4 ProjMatrix;\n"

		"out vec2 v_texcoord;\n"

		"void main(){\n"
		"    v_texcoord = texcoord;\n"
		"    gl_Position = ProjMatrix * vec4(position, 0.0, 1.0);\n"
		"}";

	static const char *Fsrc =
		"#version 400\n"

		"in vec2 v_texcoord;\n"

		"out vec4 frag_color;\n"

		"void main() {\n"
		"    frag_color = vec4(1.0-(v_texcoord.x*v_texcoord.y),v_texcoord.x,v_texcoord.y,1);\n"
		"}";

	Prog = rf::BuildShaderFromSource(Context, Vsrc, Fsrc);
	rf::ctx::RegisterShader2D(Context, Prog);

	// update projection matrices for every 2D/3D shaders registered with the context
	rf::ctx::UpdateShaderProjection(Context);

	rf::CheckGLError("rs");
}

int main()
{
	rf::context_descriptor desc = MakeCtxtDesc();

	Context = rf::ctx::Init(&desc);
	LogInfo("Welcome to %s", ExeName);

	int LastMouseX = 0, LastMouseY = 0;
	real64 CurrentTime, LastTime = glfwGetTime(), UpdateTime = 0.0;
	path FPSStr = "";

	rf::mesh ScreenQuad = rf::Make2DQuad(Context, vec2f(desc.WindowWidth / 2.f - 100.f, desc.WindowHeight / 2.f + 100.f),
		vec2f(desc.WindowWidth / 2.f + 100.f, desc.WindowHeight / 2.f - 100.f));

	// load shaders and update projection matrices shaders for the first time
	LoadShaders();

	while (Context->IsRunning)
	{
		rf::input Input = {};

		CurrentTime = glfwGetTime();
		Input.dTime = CurrentTime - LastTime;
		LastTime = CurrentTime;
		UpdateTime += Input.dTime;

		// clear the scratch arena every frame
		rf::ClearArena(&ScratchArena);

		// get frame inputs from context
		rf::ctx::GetFrameInput(Context, &Input);

		Input.MouseDX = Input.MousePosX - LastMouseX;
		Input.MouseDY = Input.MousePosY - LastMouseY;
		LastMouseX = Input.MousePosX;
		LastMouseY = Input.MousePosY;

		// clear default framebuffer
		glClearColor(Context->ClearColor.x, Context->ClearColor.y, Context->ClearColor.z, Context->ClearColor.w);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// init frame for UI
		rf::ui::BeginFrame(&Input);

		// termination test
		if (KEY_DOWN(Input.Keys[KEY_ESCAPE]))
		{
			Context->IsRunning = false;
		}

		// simple example UI widget displaying FPS
		if (UpdateTime > 0.3)
		{
			snprintf(FPSStr, MAX_PATH, "FPS : %2.4g  %.1fms", 1.0 / Input.dTime, 1000.0 * Input.dTime);
			UpdateTime = 0.0;
		}

		rf::ui::BeginPanel(&PanelID, "Info", &PanelPos, &PanelSize, rf::ui::COLOR_PANELBG);
		rf::ui::MakeText(nullptr, FPSStr, rf::ui::FONT_DEFAULT, vec2i(0, 0), rf::ui::COLOR_WHITE);
		rf::ui::EndPanel();

		// draw UI
		rf::ui::Draw();

		// draw example quad
		glUseProgram(Prog);
		glBindVertexArray(ScreenQuad.VAO);
		rf::RenderMesh(&ScreenQuad);

		glfwSwapBuffers(Context->Window);
	}

	Destroy();
	return 0;
}