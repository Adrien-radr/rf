#include "ui.h"
#include "ui_theme.h"
#include "utils.h"
#include "context.h"


/////////////////////////////////////////////////////////////////////////////////////////
// All functions accept positions and sizes in absolute coordinate with the coordinate
// origin at the top left (0,0) of the window, with the bottom right (winwidth, winheight).
/////////////////////////////////////////////////////////////////////////////////////////

namespace rf {
#define UI_STACK_SIZE (8*MB)
#define UI_MAX_PANELS 32
#define UI_PARENT_SIZE 10

namespace ui {

static context *Context;
static input   *Input;

struct input_state
{
	void   *ID;
	uint16 Idx;
	int16  Priority;
};

static mem_pool		*FramePool = nullptr;		// used memory pool for the frame (should be scratch_pool always, kept for reuse ease)

static uint16       PanelCount;                 // Total number of panels ever registered
static void         *ParentID[UI_PARENT_SIZE];  // ID stack of the parent of the current widgets, changes when a panel is begin and ended
static int16        PanelOrder[UI_MAX_PANELS];
static int16        RenderOrder[UI_MAX_PANELS];
static uint16       ParentLayer;                // Current Parent layer widgets are attached to
static input_state  Hover;
static input_state  HoverNext;
static input_state  Focus;
static input_state  FocusNext;
static int16        ForcePanelFocus;

static void         *MouseHold;
static bool         ResizeHold;

static int16        LastRootWidget;             // Address of the last widget not attached to anything

static uint32       Program, ProgramRGBTexture;
static uint32       ColorUniformLoc;
static uint32       VAO;
static uint32       VBO[2];

// NOTE - This is what is stored each frame in scratch Memory
// It stacks draw commands with this layout :
// 1 render_info
// 1 array of vertex
// 1 array of uint16 for the indices
static void         *RenderCmd[UI_MAX_PANELS];
static uint32       RenderCmdCount[UI_MAX_PANELS];
static mem_arena	RenderCmdArena[UI_MAX_PANELS];

enum widget_type {
	WIDGET_PANEL,
	WIDGET_TEXT,
	WIDGET_BUTTON,
	WIDGET_TITLEBAR,
	WIDGET_BORDER,
	WIDGET_SLIDER,
	WIDGET_PROGRESSBAR,
	WIDGET_OTHER,       // e.g., the panel resizing triangle
	WIDGET_COUNT
};

struct render_info
{
	uint32      VertexCount;
	uint32      IndexCount;
	uint32      TextureID;
	col4f       Color;
	void        *ID;
	void        *ParentID;
	widget_type Type;
	vec2i       Position;
	vec2i       Size;
	int32       Flags;
};

struct vertex
{
	vec3f Position;
	vec2f Texcoord;
};

vertex UIVertex(vec3f const &Position, vec2f const &Texcoord)
{
	vertex V = { Position, Texcoord };
	return V;
}

void Init(context *Context)
{
	ui::Context = Context;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(2, VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (GLvoid*)sizeof(vec3f));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	Hover = { NULL, 0, 0 };
	HoverNext = { NULL, 0, 0 };
	Focus = { NULL, 0, 0 };
	FocusNext = { NULL, 0, 0 };

	// 1 panel at the start : the 'backpanel' where floating widgets are stored
	PanelCount = 1;

	ParentLayer = 0;
	memset(ParentID, 0, sizeof(void*) * UI_PARENT_SIZE);

	for (int16 i = 0; i < UI_MAX_PANELS; ++i)
	{
		PanelOrder[i] = i;
	}

	MouseHold = NULL;
	ResizeHold = false;

	path ConfigPath;
	ConcatStrings(ConfigPath, ctx::GetExePath(Context), "ui_config.json");
	ParseUIConfig(Context, ConfigPath);
}

bool HasFocus()
{
	return Hover.ID != NULL;
}

void ReloadShaders(context *Context)
{
	static char const *VSSrc =
		"#version 400\n"

		"layout(location=0) in vec3 position;\n"
		"layout(location=1) in vec2 texcoord;\n"

		"uniform mat4 ProjMatrix;\n"

		"out vec2 v_texcoord;\n"

		"void main(){\n"
		"    v_texcoord = texcoord;\n"
		"    gl_Position = ProjMatrix * vec4(position, 1.0);\n"
		"}";

	static char const *FSSrc =
		"#version 400\n"

		"in vec2 v_texcoord;\n"

		"uniform sampler2D Texture0;\n"
		"uniform vec4 Color;\n"

		"out vec4 frag_color;\n"

		"void main() {\n"
		"    vec4 TexValue = texture(Texture0, v_texcoord);\n"
		"    frag_color = Color;\n"
		"    frag_color.a *= TexValue.r;\n"
		"}";

	static char const *FSTexRGBSrc =
		"#version 400\n"

		"in vec2 v_texcoord;\n"

		"uniform sampler2D Texture0;\n"

		"out vec4 frag_color;\n"

		"void main()\n"
		"{\n"
		"    frag_color = texture(Texture0, v_texcoord);\n"
		"}";

	// free programs if they already exist
	if (Program)
		glDeleteProgram(Program);

	if (ProgramRGBTexture)
		glDeleteProgram(ProgramRGBTexture);

	Program = BuildShaderFromSource(Context, VSSrc, FSSrc);
	glUseProgram(Program);
	SendInt(glGetUniformLocation(Program, "Texture0"), 0);
	ColorUniformLoc = glGetUniformLocation(Program, "Color");
	ctx::RegisterShader2D(Context, Program);

	ProgramRGBTexture = BuildShaderFromSource(Context, VSSrc, FSTexRGBSrc);
	glUseProgram(ProgramRGBTexture);
	SendInt(glGetUniformLocation(ProgramRGBTexture, "Texture0"), 0);
	ctx::RegisterShader2D(Context, ProgramRGBTexture);

	CheckGLError("UI Shader");
}

void BeginFrame(input *Input)
{
	FramePool = ui::Context->ScratchPool;
	// null the array of arena ptrs, the memory they point to is erased each frame (only if above is the scratch pool ofc
	memset(RenderCmdArena, 0, sizeof(RenderCmdArena));

	// TODO -  probably this can be done with 1 'alloc' and redirections in the buffer
	//uint8 *RenderCmdBuffer = PoolAlloc<uint8>(ui::Context->ScratchPool, UI_STACK_SIZE);
	uint64 panelStackSize = UI_STACK_SIZE / UI_MAX_PANELS;
	for (uint32 p = 0; p < UI_MAX_PANELS; ++p)
	{
		RenderCmd[p] = ArenaReserve(&RenderCmdArena[p], FramePool, panelStackSize);
	}
	memset(RenderCmdCount, 0, UI_MAX_PANELS * sizeof(uint32));

	ui::Input = Input;
	ctx::SetCursor(Context, ctx::CURSOR_NORMAL);

	LastRootWidget = 0;
	ForcePanelFocus = 0;
	ParentLayer = 0;
	Hover = HoverNext;
	HoverNext = { NULL, 0, 0 };
	Focus = FocusNext;

	// Reset the FocusID to None if we have a mouse click, future frame panels will change that
	if (MOUSE_HIT(Input->MouseLeft))
	{
		FocusNext = { NULL, 0, PanelOrder[0] };
	}
	if (MOUSE_RELEASED(Input->MouseLeft))
	{
		MouseHold = NULL;
		ResizeHold = false;
	}
}

static bool IsRootWidget()
{
	return ParentLayer == 0;
}

render_info *GetParentRenderInfo(int16 ParentIdx)
{
	return (render_info*)ArenaStart(&RenderCmdArena[ParentIdx]);
}

static bool PointInRectangle(const vec2f &Point, const vec2f &TopLeft, const vec2f &BottomRight)
{
	if (Point.x >= TopLeft.x && Point.x <= BottomRight.x)
		if (Point.y >= TopLeft.y && Point.y <= BottomRight.y)
			return true;
	return false;
}

static float SideSign(vec2f const &P, vec2f const &A, vec2f const &B)
{
	vec2f BP = P - B;
	vec2f BA = A - B;
	return BP.x * BA.y - BA.x * BP.y;
}

static bool PointInTriangle(const vec2f &Point, vec2f const &A, vec2f const &B, vec2f const &C)
{
	bool a = SideSign(Point, A, B) < 0.f;
	bool b = SideSign(Point, B, C) < 0.f;
	bool c = SideSign(Point, C, A) < 0.f;
	if ((a == b) && (b == c))
		return true;
	return false;
}

/// This accepts squares defined in the Top Left coordinate system (Top left of window is (0,0), Bottom right is (WinWidth, WinHeight))
static void FillSquare(vertex *VertData, uint16 *IdxData, int V1, int I1, vec2f const &TL, vec2f const &BR,
	vec2f const &TexOffset = vec2f(0, 0), real32 TexScale = 1.f, bool FlipY = false)
{
	int const Y = Context->WindowHeight;
	IdxData[I1 + 0] = V1 + 0; IdxData[I1 + 1] = V1 + 1; IdxData[I1 + 2] = V1 + 2;
	IdxData[I1 + 3] = V1 + 0; IdxData[I1 + 4] = V1 + 2; IdxData[I1 + 5] = V1 + 3;

	VertData[V1 + 0] = UIVertex(vec3f(TL.x, Y - TL.y, 0), TexOffset + vec2f(0.f, FlipY ? TexScale : 0.f));
	VertData[V1 + 1] = UIVertex(vec3f(TL.x, Y - BR.y, 0), TexOffset + vec2f(0.f, FlipY ? 0.f : TexScale));
	VertData[V1 + 2] = UIVertex(vec3f(BR.x, Y - BR.y, 0), TexOffset + vec2f(TexScale, FlipY ? 0.f : TexScale));
	VertData[V1 + 3] = UIVertex(vec3f(BR.x, Y - TL.y, 0), TexOffset + vec2f(TexScale, FlipY ? TexScale : 0.f));
}

void MakeBorder(vec2f const &OrigTL, vec2f const &OrigBR)
{
	int16 const ParentPanelIdx = LastRootWidget;
	render_info *RenderInfo = ArenaAlloc<render_info>(&RenderCmdArena[ParentPanelIdx], FramePool, 1);
	vertex *VertData = ArenaAlloc<vertex>(&RenderCmdArena[ParentPanelIdx], FramePool, 16);
	uint16 *IdxData = ArenaAlloc<uint16>(&RenderCmdArena[ParentPanelIdx], FramePool, 24);

	RenderInfo->Type = WIDGET_BORDER;
	RenderInfo->VertexCount = 16;
	RenderInfo->IndexCount = 24;
	RenderInfo->TextureID = *Context->RenderResources.DefaultDiffuseTexture;
	RenderInfo->Color = Theme.BorderBG;
	RenderInfo->ID = NULL;
	RenderInfo->ParentID = ParentID[ParentLayer];

	vec2f TL(OrigTL);
	vec2f BR(OrigBR.x, OrigTL.y + UI_BORDER_WIDTH);
	FillSquare(VertData, IdxData, 0, 0, TL, BR);

	TL = vec2f(OrigTL.x, OrigBR.y - UI_BORDER_WIDTH);
	BR = vec2f(OrigBR);
	FillSquare(VertData, IdxData, 4, 6, TL, BR);

	TL = vec2f(OrigTL.x, OrigTL.y + UI_BORDER_WIDTH);
	BR = vec2f(OrigTL.x + UI_BORDER_WIDTH, OrigBR.y - UI_BORDER_WIDTH);
	FillSquare(VertData, IdxData, 8, 12, TL, BR);

	TL = vec2f(OrigBR.x - UI_BORDER_WIDTH, OrigTL.y + UI_BORDER_WIDTH);
	BR = vec2f(OrigBR.x, OrigBR.y - UI_BORDER_WIDTH);
	FillSquare(VertData, IdxData, 12, 18, TL, BR);

	++(RenderCmdCount[ParentPanelIdx]);
}

void MakeText(void *ID, char const *Text, theme_font FontStyle, vec2i PositionOffset, col4f const &Color, real32 FontScale, int MaxWidth)
{
	uint32 MsgLength, VertexCount, IndexCount;

	font *Font = GetFont(FontStyle);
	if (!Font)
		return;

	// Test UTF8 String
	int UTFLen = UTF8CharCount(Text);
	if (UTFLen > 1)
	{
		MsgLength = UTF8Len(Text);
		VertexCount = MsgLength * 4;
		IndexCount = MsgLength * 6;
	}
	else
	{
		MsgLength = (uint32)strlen(Text);
		VertexCount = (MsgLength + 1) * 4;
		IndexCount = (MsgLength + 1) * 6;
	}

	if (UTFLen <= 0) return;

	bool const NoParent = IsRootWidget();
	uint16 const ParentPanelIdx = LastRootWidget;

	int const Y = Context->WindowHeight;
	render_info const *ParentRI = GetParentRenderInfo(ParentPanelIdx);
	vec2i ParentPos(0, 0), ParentSize(Context->WindowWidth, Y);;
	if (!NoParent)
	{
		ParentPos = ParentRI->Position;
		ParentSize = ParentRI->Size;
	}

	int const TitlebarOffset = ParentRI->Flags & DECORATION_TITLEBAR ? UI_TITLEBAR_HEIGHT : 0;
	int const BorderOffset = NoParent ? 0 : UI_BORDER_WIDTH;
	int const MarginOffset = NoParent ? 0 : UI_MARGIN_WIDTH;
	MaxWidth = Min(ParentSize.x - 2 * BorderOffset - 2 * MarginOffset, MaxWidth);
	vec3i const DisplayPos = vec3i(ParentPos.x + PositionOffset.x + BorderOffset + MarginOffset,
		Y - ParentPos.y - TitlebarOffset - MarginOffset - PositionOffset.y - BorderOffset, 0);

	// Check if the panel is too small vertically to display the text
	if ((DisplayPos.y - FontScale * Font->LineGap) <= (Y - ParentPos.y - ParentSize.y + MarginOffset + BorderOffset))
		return;

	render_info *RenderInfo = ArenaAlloc<render_info>(&RenderCmdArena[ParentPanelIdx], FramePool, 1);
	vertex *VertData = ArenaAlloc<vertex>(&RenderCmdArena[ParentPanelIdx], FramePool, VertexCount);
	uint16 *IdxData = ArenaAlloc<uint16>(&RenderCmdArena[ParentPanelIdx], FramePool, IndexCount);


	RenderInfo->Type = WIDGET_TEXT;
	RenderInfo->VertexCount = VertexCount;
	RenderInfo->IndexCount = IndexCount;
	RenderInfo->TextureID = Font->AtlasTextureID;
	RenderInfo->Color = Color;
	RenderInfo->ID = ID;
	RenderInfo->ParentID = NoParent ? NULL : ParentID[ParentLayer];

	if (UTFLen > 1)
		FillDisplayTextInterleavedUTF8(Text, MsgLength, Font, DisplayPos, MaxWidth, (real32*)VertData, IdxData, FontScale);
	else
		FillDisplayTextInterleaved(Text, MsgLength, Font, DisplayPos, MaxWidth, (real32*)VertData, IdxData, FontScale);
	++(RenderCmdCount[ParentPanelIdx]);

#if 0
	vec2f TL(DisplayPos.x, Y - DisplayPos.y);
	vec2f BR(TL.x + GetDisplayTextWidth(Text, Font, FontScale), TL.y + FontScale * Font->LineGap);
	MakeBorder(TL, BR);
#endif
}

void MakeText(void *ID, char const *Text, theme_font FontStyle, vec2i PositionOffset, theme_color Color, real32 FontScale, int MaxWidth)
{
	MakeText(ID, Text, FontStyle, PositionOffset, GetColor(Color), FontScale, MaxWidth);
}

void MakeTitlebar(void *ID, char const *PanelTitle, vec3i Position, vec2i Size, col4f Color)
{
	int16 const ParentPanelIdx = LastRootWidget;
	render_info *RenderInfo = ArenaAlloc<render_info>(&RenderCmdArena[ParentPanelIdx], FramePool, 1);
	vertex *VertData = ArenaAlloc<vertex>(&RenderCmdArena[ParentPanelIdx], FramePool, 4);
	uint16 *IdxData = ArenaAlloc<uint16>(&RenderCmdArena[ParentPanelIdx], FramePool, 6);

	RenderInfo->Type = WIDGET_TITLEBAR;
	RenderInfo->VertexCount = 4;
	RenderInfo->IndexCount = 6;
	RenderInfo->TextureID = *Context->RenderResources.DefaultDiffuseTexture;
	RenderInfo->Color = Color;
	RenderInfo->ID = ID;
	RenderInfo->ParentID = ParentID[ParentLayer];

	vec2f TL((real32)Position.x, (real32)Position.y);
	vec2f BR((real32)Position.x + Size.x, (real32)Position.y + Size.y);

	FillSquare(VertData, IdxData, 0, 0, TL, BR);
	++(RenderCmdCount[ParentPanelIdx]);

	// Add panel title as text
	MakeText(NULL, PanelTitle, FONT_DEFAULT, vec2i(0, -UI_TITLEBAR_HEIGHT), Theme.PanelFG, 1.f, Size.x);
}

void MakeSlider(real32 *ID, real32 MinVal, real32 MaxVal)
{
	int16 const ParentPanelIdx = LastRootWidget;
	if (ParentPanelIdx == 0) return;

	// TODO - this is pretty redundant, all this work for 2 squares...
	// Maybe allow color attribute to vertices instead of having it uniform

	// NOTE - Background square
	render_info *RenderInfo = ArenaAlloc<render_info>(&RenderCmdArena[ParentPanelIdx], FramePool, 1);
	vertex *VertData = ArenaAlloc<vertex>(&RenderCmdArena[ParentPanelIdx], FramePool, 4);
	uint16 *IdxData = ArenaAlloc<uint16>(&RenderCmdArena[ParentPanelIdx], FramePool, 6);

	RenderInfo->Type = WIDGET_SLIDER;
	RenderInfo->VertexCount = 4;
	RenderInfo->IndexCount = 6;
	RenderInfo->TextureID = *Context->RenderResources.DefaultDiffuseTexture;
	RenderInfo->Color = Theme.SliderBG;
	RenderInfo->ID = ID;
	RenderInfo->ParentID = ParentID[ParentLayer];

	render_info *ParentRI = GetParentRenderInfo(ParentPanelIdx);
	int const TitlebarOffset = ParentRI->Flags & DECORATION_TITLEBAR ? UI_TITLEBAR_HEIGHT : 0;
	vec2i Size = vec2i(5, ParentRI->Size.y - TitlebarOffset - 2 * UI_BORDER_WIDTH);
	vec2i Pos(ParentRI->Position.x + ParentRI->Size.x - Size.x - UI_BORDER_WIDTH,
		ParentRI->Position.y + TitlebarOffset + UI_BORDER_WIDTH);

	vec2f TL(Pos);
	vec2f BR((real32)Pos.x + Size.x, (real32)Pos.y + Size.y);

	FillSquare(VertData, IdxData, 0, 0, TL, BR);
	++(RenderCmdCount[ParentPanelIdx]);

	// NOTE - Foreground slider
	RenderInfo = ArenaAlloc<render_info>(&RenderCmdArena[ParentPanelIdx], FramePool, 1);
	VertData = ArenaAlloc<vertex>(&RenderCmdArena[ParentPanelIdx], FramePool, 4);
	IdxData = ArenaAlloc<uint16>(&RenderCmdArena[ParentPanelIdx], FramePool, 6);

	RenderInfo->Type = WIDGET_SLIDER;
	RenderInfo->VertexCount = 4;
	RenderInfo->IndexCount = 6;
	RenderInfo->TextureID = *Context->RenderResources.DefaultDiffuseTexture;
	RenderInfo->Color = Theme.SliderFG;
	RenderInfo->ID = ID;
	RenderInfo->ParentID = ParentID[ParentLayer];

	real32 const HalfSliderHeight = 10.f;
	real32 const Ratio = 1.f - ((*ID - MinVal) / (MaxVal - MinVal));
	real32 const PXHeight = Size.y - 2.f * HalfSliderHeight;
	TL = vec2f((real32)Pos.x, (real32)Pos.y + HalfSliderHeight + Ratio * PXHeight - HalfSliderHeight);
	BR = vec2f((real32)Pos.x + Size.x, (real32)Pos.y + HalfSliderHeight + Ratio * PXHeight + HalfSliderHeight);

	FillSquare(VertData, IdxData, 0, 0, TL, BR);
	++(RenderCmdCount[ParentPanelIdx]);

	//vec2i TL(ParentRI->Position.x, Y - ParentRI->Position.y);
	//vec2i BR(ParentRI->Position.x + ParentRI->Size.x, Y - ParentRI->Position.y - ParentRI->Size.y);
	if (Hover.ID == ParentRI->ID)
	{
		//vec2f MousePos(Input->MousePosX, Y - Input->MousePosY);
		if (Input->MouseDZ != 0)// && PointInRectangle(MousePos, TL, BR))
		{
			*ID += 1.0f * Input->MouseDZ;
			*ID = Min(MaxVal, Max(MinVal, *ID));
		}
	}
}

void MakeProgressbar(real32 *ID, real32 MaxVal, vec2i const &PositionOffset, vec2i const &Size)
{
	int16 const ParentPanelIdx = LastRootWidget;
	if (ParentPanelIdx == 0) return;

	// NOTE - Background Square
	render_info *RenderInfo = ArenaAlloc<render_info>(&RenderCmdArena[ParentPanelIdx], FramePool, 1);
	vertex *VertData = ArenaAlloc<vertex>(&RenderCmdArena[ParentPanelIdx], FramePool, 4);
	uint16 *IdxData = ArenaAlloc<uint16>(&RenderCmdArena[ParentPanelIdx], FramePool, 6);

	RenderInfo->Type = WIDGET_PROGRESSBAR;
	RenderInfo->VertexCount = 4;
	RenderInfo->IndexCount = 6;
	RenderInfo->TextureID = *Context->RenderResources.DefaultDiffuseTexture;
	RenderInfo->Color = Theme.ProgressbarBG;
	RenderInfo->ID = ID;
	RenderInfo->ParentID = ParentID[ParentLayer];

	render_info *ParentRI = GetParentRenderInfo(ParentPanelIdx);
	int const TitlebarOffset = ParentRI->Flags & DECORATION_TITLEBAR ? UI_TITLEBAR_HEIGHT : 0;
	int const BorderOffset = UI_BORDER_WIDTH;
	int const MarginOffset = UI_MARGIN_WIDTH;
	int const MaxWidth = Min(ParentRI->Size.x - 2 * BorderOffset - 2 * MarginOffset, Size.x);

	vec2i TL(ParentRI->Position.x + PositionOffset.x + BorderOffset + MarginOffset, ParentRI->Position.y + PositionOffset.y + TitlebarOffset + BorderOffset + MarginOffset);
	vec2i BR(TL.x + MaxWidth, TL.y + Size.y);

	FillSquare(VertData, IdxData, 0, 0, TL + vec2i(BorderOffset), BR - vec2i(BorderOffset));
	++(RenderCmdCount[ParentPanelIdx]);

#if 1
	real32 const ProgressWidth = (*ID / MaxVal) * MaxWidth;
	if (ProgressWidth > 0.f)
	{
		// NOTE - Forground Square
		RenderInfo = ArenaAlloc<render_info>(&RenderCmdArena[ParentPanelIdx], FramePool, 1);
		VertData = ArenaAlloc<vertex>(&RenderCmdArena[ParentPanelIdx], FramePool, 4);
		IdxData = ArenaAlloc<uint16>(&RenderCmdArena[ParentPanelIdx], FramePool, 6);

		RenderInfo->Type = WIDGET_PROGRESSBAR;
		RenderInfo->VertexCount = 4;
		RenderInfo->IndexCount = 6;
		RenderInfo->TextureID = *Context->RenderResources.DefaultDiffuseTexture;
		RenderInfo->Color = Theme.ProgressbarFG;
		RenderInfo->ID = ID;
		RenderInfo->ParentID = ParentID[ParentLayer];

		vec2i BRP(TL.x + (int)ceil(ProgressWidth) - BorderOffset, TL.y + Size.y - BorderOffset);

		FillSquare(VertData, IdxData, 0, 0, TL + vec2i(BorderOffset), BRP);
		++(RenderCmdCount[ParentPanelIdx]);
	}

#endif
	MakeBorder(TL, BR);
}

bool MakeButton(uint32 *ID, char const *ButtonText, theme_font FontStyle, vec2i const &PositionOffset, vec2i const &Size, real32 FontScale, int32 DecorationFlags)
{
	int16 const ParentPanelIdx = LastRootWidget;
	if (ParentPanelIdx == 0) return false;

	font *Font = GetFont(FontStyle);

	render_info *RenderInfo = ArenaAlloc<render_info>(&RenderCmdArena[ParentPanelIdx], FramePool, 1);
	vertex *VertData = ArenaAlloc<vertex>(&RenderCmdArena[ParentPanelIdx], FramePool, 4);
	uint16 *IdxData = ArenaAlloc<uint16>(&RenderCmdArena[ParentPanelIdx], FramePool, 6);

	RenderInfo->Type = WIDGET_BUTTON;
	RenderInfo->VertexCount = 4;
	RenderInfo->IndexCount = 6;
	RenderInfo->TextureID = *Context->RenderResources.DefaultDiffuseTexture;
	RenderInfo->Color = *ID > 0 ? Theme.ButtonPressedBG : Theme.ButtonBG;
	RenderInfo->ID = ID;
	RenderInfo->ParentID = ParentID[ParentLayer];

	render_info *ParentRI = GetParentRenderInfo(ParentPanelIdx);
	int const TitlebarOffset = ParentRI->Flags & DECORATION_TITLEBAR ? UI_TITLEBAR_HEIGHT : 0;
	int const MarginOffset = (DecorationFlags & DECORATION_MARGIN) ? UI_MARGIN_WIDTH : 0;
	int const BorderOffset = (DecorationFlags & DECORATION_BORDER) ? UI_BORDER_WIDTH : 0;
	int const TextHeight = Font ? Font->LineGap : 0;
	vec2f const MaxBR((real32)ParentRI->Position.x + ParentRI->Size.x - BorderOffset - MarginOffset, (real32)ParentRI->Position.y + ParentRI->Size.y - BorderOffset - MarginOffset);
	vec2f const OffsetPos((real32)ParentRI->Position.x + PositionOffset.x + MarginOffset + BorderOffset,
		(real32)ParentRI->Position.y + PositionOffset.y + TitlebarOffset + MarginOffset + BorderOffset);
	vec2f const TL(OffsetPos.x, OffsetPos.y);
	vec2f BR(TL.x + Size.x, TL.y + Max(2 * MarginOffset + 2 * BorderOffset + TextHeight, Size.y));
	BR.x = Min(BR.x, MaxBR.x);
	BR.y = Min(BR.y, MaxBR.y);

	FillSquare(VertData, IdxData, 0, 0, TL, BR);
	++(RenderCmdCount[ParentPanelIdx]);

	if (BorderOffset > 0)
	{
		MakeBorder(TL, BR);
	}

	if (Font)
	{
		real32 const MaxButtonTextWidth = BR.x - TL.x - 2 * UI_BORDER_WIDTH - 2 * UI_MARGIN_WIDTH;
		real32 const TextWidth = GetDisplayTextWidth(ButtonText, Font, FontScale);
		real32 const TextMargin = (MaxButtonTextWidth - TextWidth) * 0.5f;//BR.x-TL.x - TextWidth;//(MaxButtonTextWidth-TextWidth) * 0.5f;
		MakeText(NULL, ButtonText, FontStyle, vec2i(PositionOffset.x + (int)ceil(TextMargin) + BorderOffset + MarginOffset, PositionOffset.y + MarginOffset + BorderOffset),
			Theme.PanelFG, FontScale, (int)MaxButtonTextWidth);
	}

	vec2f const MousePos((real32)Input->MousePosX, (real32)Input->MousePosY);
	if (Hover.ID == ParentRI->ID)
	{
		if (MOUSE_HIT(Input->MouseLeft) && PointInRectangle(MousePos, TL, BR))
		{
			*ID = 1;
		}
		else if (MOUSE_RELEASED(Input->MouseLeft) && PointInRectangle(MousePos, TL, BR))
		{
			*ID = 0;
			return true;
		}
	}

	// Release button (without activation) if it was pressed but not released within bounds
	if (*ID > 0 && MOUSE_RELEASED(Input->MouseLeft) && !PointInRectangle(MousePos, TL, BR))
	{
		*ID = 0;
	}

	return false;
}

void MakeImage(real32 *ID, uint32 TextureID, vec2f *TexOffset, vec2i const &Size, bool FlipY)
{
	int16 const ParentPanelIdx = LastRootWidget;
	if (ParentPanelIdx == 0) return;

	render_info *RenderInfo = ArenaAlloc<render_info>(&RenderCmdArena[ParentPanelIdx], FramePool, 1);
	vertex *VertData = ArenaAlloc<vertex>(&RenderCmdArena[ParentPanelIdx], FramePool, 4);
	uint16 *IdxData = ArenaAlloc<uint16>(&RenderCmdArena[ParentPanelIdx], FramePool, 6);

	RenderInfo->Type = WIDGET_BUTTON;
	RenderInfo->VertexCount = 4;
	RenderInfo->IndexCount = 6;
	RenderInfo->TextureID = TextureID;
	RenderInfo->Color = Theme.White;
	RenderInfo->ID = ID;
	RenderInfo->ParentID = ParentID[ParentLayer];
	RenderInfo->Flags = DECORATION_RGBTEXTURE;

	render_info *ParentRI = GetParentRenderInfo(ParentPanelIdx);
	int const TitlebarOffset = ParentRI->Flags & DECORATION_TITLEBAR ? UI_TITLEBAR_HEIGHT : 0;
	vec2i MaxBR(ParentRI->Position.x + ParentRI->Size.x - UI_BORDER_WIDTH - UI_MARGIN_WIDTH, ParentRI->Position.y + ParentRI->Size.y - UI_BORDER_WIDTH - UI_MARGIN_WIDTH);
	vec2f TL((real32)ParentRI->Position.x + UI_BORDER_WIDTH + UI_MARGIN_WIDTH + UI_BORDER_WIDTH, (real32)ParentRI->Position.y + TitlebarOffset + UI_BORDER_WIDTH + UI_MARGIN_WIDTH + UI_BORDER_WIDTH);
	vec2f BR((real32)TL.x + Size.x - UI_BORDER_WIDTH, (real32)TL.y + Size.y - UI_BORDER_WIDTH);
	BR.x = Min(BR.x, MaxBR.x);
	BR.y = Min(BR.y, MaxBR.y);

	real32 TexScale = 1.f / *ID;
	FillSquare(VertData, IdxData, 0, 0, TL, BR, *TexOffset, TexScale, FlipY);
	++(RenderCmdCount[ParentPanelIdx]);

	MakeBorder(TL + vec2f(-UI_BORDER_WIDTH, -UI_BORDER_WIDTH), BR + vec2f(UI_BORDER_WIDTH, UI_BORDER_WIDTH));

	if (Hover.ID == ParentRI->ID)
	{
		if (Input->MouseDZ != 0 && PointInRectangle(vec2f((real32)Input->MousePosX, (real32)Input->MousePosY), TL, BR))
		{
			*ID *= (1.f + 0.1f * Input->MouseDZ);
			*ID = Max(0.0001f, *ID);
		}

		if (MOUSE_HIT(Input->MouseLeft) && PointInRectangle(vec2f((real32)Input->MousePosX, (real32)Input->MousePosY), TL, BR))
		{
			MouseHold = ID;
		}
	}

	if (MouseHold == ID)
	{
		real32 SpanSpeed = 1.f / (Max(Size.x, Size.y) * *ID);
		TexOffset->x -= Input->MouseDX * SpanSpeed;
		TexOffset->y -= Input->MouseDY * SpanSpeed;
	}
}

// Position is in TopLeft coordinate system (Top left corner of window is (0,0))
void MakeResizingTriangle(vec2f const &BR)
{
	int16 const ParentPanelIdx = LastRootWidget;
	if (ParentPanelIdx == 0) return;

	render_info *RenderInfo = ArenaAlloc<render_info>(&RenderCmdArena[ParentPanelIdx], FramePool, 1);
	vertex *VertData = ArenaAlloc<vertex>(&RenderCmdArena[ParentPanelIdx], FramePool, 3);
	uint16 *IdxData = ArenaAlloc<uint16>(&RenderCmdArena[ParentPanelIdx], FramePool, 3);

	RenderInfo->Type = WIDGET_OTHER;
	RenderInfo->VertexCount = 3;
	RenderInfo->IndexCount = 3;
	RenderInfo->TextureID = *Context->RenderResources.DefaultDiffuseTexture;
	RenderInfo->Color = Theme.BorderBG;
	RenderInfo->ID = NULL;
	RenderInfo->ParentID = ParentID[ParentLayer];

	// Add resizing triangle
	int const Y = Context->WindowHeight;
	IdxData[0] = 0; IdxData[1] = 1; IdxData[2] = 2;
	VertData[0] = UIVertex(vec3f(BR.x, Y - BR.y, 0), vec2f(1.f, 1.f));
	VertData[1] = UIVertex(vec3f(BR.x, Y - BR.y + 8.f, 0), vec2f(1.f, 1.f));
	VertData[2] = UIVertex(vec3f(BR.x - 8.f, Y - BR.y, 0), vec2f(0.f, 1.f));
	++(RenderCmdCount[ParentPanelIdx]);
}

void BeginPanel(uint32 *ID, char const *PanelTitle, vec3i *Position, vec2i *Size, theme_color Color, uint32 DecorationFlags)
{
	Assert(PanelCount < UI_MAX_PANELS);
	Assert(Size->x > 0 && Size->y > 0);
	uint32 PanelIdx;

	// NOTE - Init stage of the panel, storing the first time Panel Idx, as well as forcing Focus on it
	if (*ID == 0)
	{
		PanelIdx = PanelCount++;
		*ID = PanelIdx;
		ForcePanelFocus = PanelIdx;
	}
	else
	{
		PanelIdx = *ID;
	}

	if (DecorationFlags & DECORATION_FOCUS)
	{
		ForcePanelFocus = PanelIdx;
	}

	ParentID[ParentLayer++] = ID;

	int VCount = 4, ICount = 6;
	if (DecorationFlags & DECORATION_INVISIBLE)
	{
		VCount = ICount = 0;
	}

	render_info *RenderInfo = ArenaAlloc<render_info>(&RenderCmdArena[PanelIdx], FramePool, 1);
	vertex *VertData = ArenaAlloc<vertex>(&RenderCmdArena[PanelIdx], FramePool, VCount);
	uint16 *IdxData = ArenaAlloc<uint16>(&RenderCmdArena[PanelIdx], FramePool, ICount);

	RenderInfo->Type = WIDGET_PANEL;
	RenderInfo->VertexCount = VCount;
	RenderInfo->IndexCount = ICount;
	RenderInfo->TextureID = *Context->RenderResources.DefaultDiffuseTexture;
	RenderInfo->Color = GetColor(Color);
	RenderInfo->ID = ID;
	RenderInfo->ParentID = NULL;
	RenderInfo->Position = vec2i(Position->x, Position->y);
	RenderInfo->Size = *Size;
	RenderInfo->Flags = DecorationFlags;
	LastRootWidget = *ID;

	vec2f TL((real32)Position->x, (real32)Position->y);
	vec2f BR((real32)Position->x + Size->x, (real32)Position->y + Size->y);

	++(RenderCmdCount[PanelIdx]);

	if (!(DecorationFlags & DECORATION_INVISIBLE))
	{
		FillSquare(VertData, IdxData, 0, 0, TL, BR);

		if (DecorationFlags & DECORATION_BORDER)
		{
			MakeBorder(TL, BR);
		}

		if (DecorationFlags & DECORATION_RESIZE)
		{
			MakeResizingTriangle(BR);
		}

		if (DecorationFlags & DECORATION_TITLEBAR)
		{
			MakeTitlebar(NULL, PanelTitle, *Position + vec3f(UI_BORDER_WIDTH, UI_BORDER_WIDTH, 0), vec2i(Size->x - 2 * UI_BORDER_WIDTH, UI_TITLEBAR_HEIGHT), Theme.TitlebarBG);
		}
	}

	if (HoverNext.Priority <= PanelOrder[PanelIdx])
	{
		vec2f MousePos((real32)Input->MousePosX, (real32)Input->MousePosY);
		if (PointInRectangle(MousePos, TL, BR))
		{
			HoverNext.ID = ID;
			HoverNext.Priority = PanelOrder[PanelIdx];
			HoverNext.Idx = PanelIdx;

			// Test for Titlebar click
			vec2f TB_TL = TL;
			vec2f TB_BR((real32)BR.x, (real32)Position->y + UI_TITLEBAR_HEIGHT + UI_BORDER_WIDTH);
			if (MOUSE_HIT(Input->MouseLeft))
			{
				if (DecorationFlags & DECORATION_TITLEBAR && ID == Hover.ID && PointInRectangle(MousePos, TB_TL, TB_BR))
				{
					MouseHold = ID;
				}

				// Test for resize triangle click
				vec2f A(BR), B(BR + vec2f(0, -8.f)), C(BR + vec2f(-8.f, 0));
				if (DecorationFlags & DECORATION_RESIZE && ID == Hover.ID && PointInTriangle(MousePos, A, B, C))
				{
					ResizeHold = true;
				}
			}
		}
	}

	if (Focus.ID == ID)
	{
		if (MouseHold == ID)
		{
			Position->x += Input->MouseDX;
			Position->y += Input->MouseDY;
		}
		if (ResizeHold == true)
		{
			Size->x += Input->MouseDX;
			Size->y += Input->MouseDY;

			Size->x = Max(Size->x, 50);
			Size->y = Max(Size->y, 50);
		}
	}
}

void EndPanel()
{
	--ParentLayer;
}

// Reorders the panel focus and render orders by pushing panel Idx to the front
static void FocusReorder(int16 Idx)
{
	int16 OldPriority = PanelOrder[Idx];

	for (uint32 p = 1; p < PanelCount; ++p)
		if (PanelOrder[p] > OldPriority)
			--PanelOrder[p];
	PanelOrder[Idx] = PanelCount - 1;

	for (uint32 i = 0; i < PanelCount; ++i)
	{
		RenderOrder[PanelOrder[i]] = i;
	}
}

static void Update()
{
	if (ForcePanelFocus > 0)
	{ // NOTE - push focus on a specific panel if needed (usually when it first appear or if the caller asks it)
		FocusReorder(ForcePanelFocus);
	}

	if (MOUSE_HIT(Input->MouseLeft))
	{
		FocusNext = Hover;

		// Reorder panel rendering order if focus has changed
		if (Hover.Priority > 0 && Hover.Priority < (PanelCount - 1))
		{
			FocusReorder(Hover.Idx);
		}
	}
}

static void *RenderCmdOffset(uint8 *CmdList, size_t *OffsetAccum, size_t Size)
{
	void* Ptr = (void*)(CmdList + *OffsetAccum);
	*OffsetAccum += Size;
	return Ptr;
}

void Draw()
{
	Update();
	
	uint32 CurrProgram = 0;

	glDisable(GL_DEPTH_TEST);
	glBindVertexArray(VAO);
	for (int p = 0; p < PanelCount; ++p)
	{
		// we should have only one big allocated block for each panel, if we have more we are above UI_STACK_SIZE/UI_MAX_PANELS limit
		Assert(BufSize(RenderCmdArena[p].Blocks) == 1);

		int16 OrdPanelIdx = RenderOrder[p];
		uint8 *Cmd = (uint8*)RenderCmd[OrdPanelIdx];
		for (uint32 i = 0; i < RenderCmdCount[OrdPanelIdx]; ++i)
		{
			size_t Offset = 0;
			render_info *RenderInfo = (render_info*)RenderCmdOffset(Cmd, &Offset, sizeof(render_info));
			vertex *VertData = (vertex*)RenderCmdOffset(Cmd, &Offset, RenderInfo->VertexCount * sizeof(vertex));
			uint16 *IdxData = (uint16*)RenderCmdOffset(Cmd, &Offset, RenderInfo->IndexCount * sizeof(uint16));

			if (!(RenderInfo->Flags & DECORATION_INVISIBLE))
			{
				// Switch program depending on rgb texture or normal UI widget
				if (RenderInfo->Flags & DECORATION_RGBTEXTURE)
				{
					if (CurrProgram != ProgramRGBTexture)
					{
						CurrProgram = ProgramRGBTexture;
						glUseProgram(CurrProgram);
					}
				}
				else
				{
					if (CurrProgram != Program)
					{
						CurrProgram = Program;
						glUseProgram(Program);
					}
					SendVec4(ColorUniformLoc, RenderInfo->Color);
				}

				glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
				glBufferData(GL_ARRAY_BUFFER, RenderInfo->VertexCount * sizeof(vertex), (GLvoid*)VertData, GL_STREAM_DRAW);

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO[0]);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, RenderInfo->IndexCount * sizeof(uint16), (GLvoid*)IdxData, GL_STREAM_DRAW);

				glBindTexture(GL_TEXTURE_2D, RenderInfo->TextureID);

				glDrawElements(GL_TRIANGLES, RenderInfo->IndexCount, GL_UNSIGNED_SHORT, 0);
			}

			Cmd += Offset;
		}
	}
	glEnable(GL_DEPTH_TEST);
	glBindVertexArray(0);
}
}
}
