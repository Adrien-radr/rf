#include "render.h"
#include "context.h"
#include "utils.h"

#include "stb_image.h"
#include "stb_truetype.h"
#include "fontawesome.h"

namespace rf {

static resource_store *GetStore(render_resources *RenderResources, render_resource_type Type)
{
	switch (Type)
	{
	case RESOURCE_IMAGE:
		return &RenderResources->Images;
	case RESOURCE_FONT:
		return &RenderResources->Fonts;
	case RESOURCE_TEXTURE:
		return &RenderResources->Textures;
	default:
		return NULL;
	}
}

static char const *GetResourceTypeName(render_resource_type Type)
{
	Assert(Type < RESOURCE_COUNT);
	static char const *ResourceTypeName[] =
	{
		"Image",
		"Texture",
		"Font",
		"Unknown"
	};

	return ResourceTypeName[Type];
}

void *ResourceCheckExist(render_resources *RenderResources, render_resource_type Type, path const Filename)
{
	Assert(Type < RESOURCE_COUNT);
	LogDebug("Checking for %s resource %s", GetResourceTypeName(Type), (char*)Filename);

	resource_store *Store = GetStore(RenderResources, Type);
	if (Store)
	{
		return MapStoreGet(Store, Filename);
	}

	return NULL;
}

void ResourceStore(render_resources *RenderResources, render_resource_type Type, path const Filename, void *Resource)
{
	Assert(Type < RESOURCE_COUNT);

	resource_store *Store = GetStore(RenderResources, Type);
	if (Store)
	{
		LogDebug("Storing %s [%llu]", Filename, (Type == RESOURCE_IMAGE || Type == RESOURCE_TEXTURE) ? (uint64)*((uint32*)Resource) : (uint64)Resource);
		MapStoreAdd(Store, Filename, Resource);
	}
}

void DestroyImage(image *Image);
void ResourceFree(render_resources *RenderResources)
{
	resource_store *Store;
	{
		Store = &RenderResources->Images;
		for (uint64 i = 0; i < Store->HMap.Capacity; ++i)
		{
			const char *KeyStr = MapStoreGetKey(Store, i);
			if (KeyStr)
			{
				LogDebug("Destroying image %s", KeyStr);
				DestroyImage((image*)MapStoreGet(Store, KeyStr));
			}
		}
		MapStoreFree(Store);
		
	}
	{
		Store = &RenderResources->Fonts;
		for (uint64 i = 0; i < Store->HMap.Capacity; ++i)
		{
			const char *KeyStr = MapStoreGetKey(Store, i);
			if (KeyStr)
			{
				LogDebug("Destroying font %s", KeyStr);
			}
		}
		MapStoreFree(Store);

	}
	{
		Store = &RenderResources->Textures;
		for (uint64 i = 0; i < Store->HMap.Capacity; ++i)
		{
			const char *KeyStr = MapStoreGetKey(Store, i);
			if (KeyStr)
			{
				LogDebug("Destroying texture %s", KeyStr);
				glDeleteTextures(1, (uint32*)MapStoreGet(Store, KeyStr));
			}
		}
		MapStoreFree(Store);
	}
}

void CheckGLError(char const *Mark)
{
	D_ONLY(
		uint32 Err = glGetError();
	if (Err != GL_NO_ERROR)
	{
		char ErrName[64];
		switch (Err)
		{
		case GL_INVALID_ENUM:
			snprintf(ErrName, 64, "GL_INVALID_ENUM");
			break;
		case GL_INVALID_VALUE:
			snprintf(ErrName, 64, "GL_INVALID_VALUE");
			break;
		case GL_INVALID_OPERATION:
			snprintf(ErrName, 64, "GL_INVALID_OPERATION");
			break;
		case GL_STACK_OVERFLOW:
			snprintf(ErrName, 64, "GL_STACK_OVERFLOW");
			break;
		case GL_STACK_UNDERFLOW:
			snprintf(ErrName, 64, "GL_STACK_UNDERFLOW");
			break;
		case GL_OUT_OF_MEMORY:
			snprintf(ErrName, 64, "GL_OUT_OF_MEMORY");
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			snprintf(ErrName, 64, "GL_INVALID_FRAMEBUFFER_OPERATION");
			break;
		default:
			snprintf(ErrName, 64, "UNKNOWN [%u]", Err);
			break;
		}
		LogError("[%s] GL Error %s", Mark, ErrName);
	}
	)
}

void CheckFramebufferError(char const *Mark)
{
	D_ONLY(
		uint32 Err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (Err != GL_FRAMEBUFFER_COMPLETE)
	{
		char ErrName[64];
		switch (Err)
		{
		case GL_FRAMEBUFFER_UNDEFINED:                     snprintf(ErrName, 64, "Undefined."); break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:         snprintf(ErrName, 64, "Incomplete Attachment."); break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: snprintf(ErrName, 64, "Incomplete - Missing Attachment."); break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:        snprintf(ErrName, 64, "Incomplete Draw buffer."); break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:        snprintf(ErrName, 64, "Incomplete Read buffer."); break;
		case GL_FRAMEBUFFER_UNSUPPORTED:                   snprintf(ErrName, 64, "Unsupported."); break;
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:        snprintf(ErrName, 64, "Incomplete Multisample."); break;
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:      snprintf(ErrName, 64, "Incomplete Layer Targets."); break;
		default:                                           snprintf(ErrName, 64, "Unknown Error %u", Err); break;
		}
		LogError("[%s] Framebuffer Error : %s", Mark, ErrName);
	}
	)
}

image *ResourceLoadImage(context *Context, path const Filename, bool IsFloat, bool FlipY, int32 ForceNumChannel)
{
	path ResourceName;
	ConcatStrings(ResourceName, ctx::GetExePath(Context), Filename);
	void *LoadedResource = ResourceCheckExist(&Context->RenderResources, RESOURCE_IMAGE, Filename);

	if (LoadedResource)
	{
		return (image*)LoadedResource;
	}

	image *Image = rf::PoolAlloc<image>(Context->SessionPool, 1);
	stbi_set_flip_vertically_on_load(FlipY ? 1 : 0); // NOTE - Flip Y so textures are Y-descending

	if (IsFloat)
		Image->Buffer = stbi_loadf(ResourceName, &Image->Width, &Image->Height, &Image->Channels, ForceNumChannel);
	else
		Image->Buffer = stbi_load(ResourceName, &Image->Width, &Image->Height, &Image->Channels, ForceNumChannel);

	if (!Image->Buffer)
	{
		LogError("Error loading Image from %s. Aborting..", ResourceName);
		return NULL;
	}

	ResourceStore(&Context->RenderResources, RESOURCE_IMAGE, Filename, Image);

	return Image;
}

void DestroyImage(image *Image)
{
	stbi_image_free(Image->Buffer);
	Image->Width = Image->Height = Image->Channels = 0;
}

void FormatFromChannels(uint32 Channels, bool IsFloat, bool FloatHalfPrecision, GLint *BaseFormat, GLint *Format)
{
	if (IsFloat)
	{
		switch (Channels)
		{
		case 1:
			*BaseFormat = FloatHalfPrecision ? GL_R16F : GL_R32F;
			*Format = GL_RED;
			break;
		case 2:
			*BaseFormat = FloatHalfPrecision ? GL_RG16F : GL_RG32F;
			*Format = GL_RG;
			break;
		case 3:
			*BaseFormat = FloatHalfPrecision ? GL_RGB16F : GL_RGB32F;
			*Format = GL_RGB;
			break;
		case 4:
			*BaseFormat = FloatHalfPrecision ? GL_RGBA16F : GL_RGBA32F;
			*Format = GL_RGBA;
			break;
		}
	}
	else
	{
		switch (Channels)
		{
		case 1:
			*BaseFormat = *Format = GL_RED; break;
		case 2:
			*BaseFormat = *Format = GL_RG; break;
		case 3:
			*BaseFormat = *Format = GL_RGB; break;
		case 4:
			*BaseFormat = *Format = GL_RGBA; break;
		}
	}
}

uint32 Make2DTexture(void *ImageBuffer, uint32 Width, uint32 Height, uint32 Channels, bool IsFloat,
	bool FloatHalfPrecision, real32 AnisotropicLevel, int MagFilter, int MinFilter, int WrapS, int WrapT)
{
	uint32 Texture;
	glGenTextures(1, &Texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Texture);

	GLint CurrentAlignment;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &CurrentAlignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, MinFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, MagFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, WrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, WrapT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, AnisotropicLevel);

	GLint BaseFormat, Format;
	FormatFromChannels(Channels, IsFloat, FloatHalfPrecision, &BaseFormat, &Format);
	GLenum Type = IsFloat ? GL_FLOAT : GL_UNSIGNED_BYTE;

	glTexImage2D(GL_TEXTURE_2D, 0, BaseFormat, Width, Height, 0, Format, Type, ImageBuffer);
	CheckGLError("glTexImage2D");
	if (MinFilter >= GL_NEAREST_MIPMAP_NEAREST && MinFilter <= GL_LINEAR_MIPMAP_LINEAR)
	{ // NOTE - Generate mipmaps if mag filter ask it
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, CurrentAlignment);

	glBindTexture(GL_TEXTURE_2D, 0);

	return Texture;
}

uint32 Make3DTexture(uint32 Width, uint32 Height, uint32 Depth, uint32 Channels, bool IsFloat, bool FloatHalfPrecision,
	int MagFilter, int MinFilter, int WrapS, int WrapT, int WrapR)
{
	uint32 Texture;
	glGenTextures(1, &Texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, Texture);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, MinFilter);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, MagFilter);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, WrapS);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, WrapT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, WrapR);

	GLint BaseFormat, Format;
	FormatFromChannels(Channels, IsFloat, FloatHalfPrecision, &BaseFormat, &Format);
	GLenum Type = IsFloat ? GL_FLOAT : GL_UNSIGNED_BYTE;

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	glTexImage3D(GL_TEXTURE_3D, 0, BaseFormat, Width, Height, Depth, 0, Format, Type, NULL);
	CheckGLError("glTexImage3D");

	glBindTexture(GL_TEXTURE_3D, 0);

	return Texture;
}

static uint32 Make2DTexture(image *Image, bool IsFloat, bool FloatHalfPrecision, uint32 AnisotropicLevel, int MagFilter, int MinFilter, int WrapS, int WrapT)
{
	return Make2DTexture(Image->Buffer, Image->Width, Image->Height, Image->Channels, IsFloat, FloatHalfPrecision, (real32)AnisotropicLevel,
		MagFilter, MinFilter, WrapS, WrapT);
}

void BindTexture2D(uint32 TextureID, uint32 TextureUnit)
{
	glActiveTexture(GL_TEXTURE0 + TextureUnit);
	glBindTexture(GL_TEXTURE_2D, TextureID);
}

void BindTexture3D(uint32 TextureID, uint32 TextureUnit)
{
	glActiveTexture(GL_TEXTURE0 + TextureUnit);
	glBindTexture(GL_TEXTURE_3D, TextureID);
}

void BindCubemap(uint32 TextureID, uint32 TextureUnit)
{
	glActiveTexture(GL_TEXTURE0 + TextureUnit);
	glBindTexture(GL_TEXTURE_CUBE_MAP, TextureID);
}

uint32 *ResourceLoad2DTexture(context *Context, path const Filename, bool IsFloat, bool FloatHalfPrecision,
	uint32 AnisotropicLevel, int MagFilter, int MinFilter, int WrapS, int WrapT, int32 ForceNumChannel)
{
	void *LoadedResource = ResourceCheckExist(&Context->RenderResources, RESOURCE_TEXTURE, Filename);
	if (LoadedResource)
	{
		return (uint32*)LoadedResource;
	}

	uint32 *Tex = rf::PoolAlloc<uint32>(Context->SessionPool, 1);
	*Tex = Make2DTexture(ResourceLoadImage(Context, Filename, IsFloat, true, ForceNumChannel),
		IsFloat, FloatHalfPrecision, AnisotropicLevel, MagFilter, MinFilter, WrapS, WrapT);

	ResourceStore(&Context->RenderResources, RESOURCE_TEXTURE, Filename, Tex);

	return Tex;
}

uint32 MakeCubemap(context *Context, path *Paths, bool IsFloat, bool FloatHalfPrecision, uint32 Width, uint32 Height, bool MakeMipmap)
{
	uint32 Cubemap = 0;

	glGenTextures(1, &Cubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, Cubemap);
	CheckGLError("SkyboxGen");

	for (int i = 0; i < 6; ++i)
	{ // Load each face
		if (Paths)
		{ // For loading from texture files
			image *Face = ResourceLoadImage(Context, Paths[i], IsFloat);

			GLint BaseFormat, Format;
			FormatFromChannels(Face->Channels, IsFloat, FloatHalfPrecision, &BaseFormat, &Format);
			GLenum Type = IsFloat ? GL_FLOAT : GL_UNSIGNED_BYTE;

			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, BaseFormat, Face->Width, Face->Height,
				0, Format, Type, Face->Buffer);
			CheckGLError("SkyboxFace");
		}
		else
		{ // Empty Cubemap
			GLint BaseFormat = IsFloat ? (FloatHalfPrecision ? GL_RGB16F : GL_RGB32F) : GL_RGB16;
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, BaseFormat,
				Width, Height, 0, GL_RGB, IsFloat ? GL_FLOAT : GL_UNSIGNED_BYTE, NULL);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, MakeMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	if (MakeMipmap)
	{ // allocate mipmap memory. if Paths exist, generate mipmaps as well
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	}
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	CheckGLError("SkyboxParams");

	return Cubemap;
}

static GLuint FBOAttachments[MAX_FBO_ATTACHMENTS] =
{
	GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
	GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4
};

void DestroyFramebuffer(frame_buffer *FB)
{
	if (FB->FBO > 0)
	{
		glDeleteTextures(FB->NumAttachments, FB->BufferIDs);
		glDeleteRenderbuffers(1, &FB->DepthBufferID);
		FB->DepthBufferID = 0;
		glDeleteFramebuffers(1, &FB->FBO);
		FB->FBO = 0;
		FB->Size = vec2i(0);
		FB->NumAttachments = 0;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

frame_buffer MakeFramebuffer(uint32 NumAttachments, vec2i Size, bool AddDepthBuffer)
{
	Assert(NumAttachments < MAX_FBO_ATTACHMENTS);

	frame_buffer FB = {};
	FB.Size = Size;
	FB.NumAttachments = NumAttachments;

	glGenFramebuffers(1, &FB.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FB.FBO);
	glDrawBuffers((GLsizei)NumAttachments, FBOAttachments);

	if (AddDepthBuffer)
	{
		glGenRenderbuffers(1, &FB.DepthBufferID);
		glBindRenderbuffer(GL_RENDERBUFFER, FB.DepthBufferID);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, Size.x, Size.y);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, FB.DepthBufferID);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			LogError("Framebuffer creation error : not complete.");
			DestroyFramebuffer(&FB);
		}
	}

	return FB;
}

void FramebufferSetAttachmentCount(frame_buffer *FB, int Count)
{
	Assert(Count < MAX_FBO_ATTACHMENTS);
	FB->NumAttachments = Count;
	glDrawBuffers((GLsizei)Count, FBOAttachments);
}

void FramebufferAttachBuffer(frame_buffer *FB, uint32 Attachment, uint32 TextureID)
{
	Assert(Attachment < MAX_FBO_ATTACHMENTS);
	glFramebufferTexture(GL_FRAMEBUFFER, FBOAttachments[Attachment], TextureID, 0);
}

void FramebufferAttachBuffer(frame_buffer *FB, uint32 Attachment, uint32 Channels, bool IsFloat, bool FloatHalfPrecision, bool Mipmap)
{
	Assert(Attachment < MAX_FBO_ATTACHMENTS);

	uint32 *BufferID = &FB->BufferIDs[Attachment];
	glGenTextures(1, BufferID);
	glBindTexture(GL_TEXTURE_2D, *BufferID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, Mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLint BaseFormat, Format;
	FormatFromChannels(Channels, IsFloat, FloatHalfPrecision, &BaseFormat, &Format);
	GLenum Type = IsFloat ? GL_FLOAT : GL_UNSIGNED_BYTE;
	glTexImage2D(GL_TEXTURE_2D, 0, BaseFormat, FB->Size.x, FB->Size.y, 0, Format, Type, NULL);

	if (Mipmap)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glFramebufferTexture2D(GL_FRAMEBUFFER, (GLenum)(GL_COLOR_ATTACHMENT0 + Attachment), GL_TEXTURE_2D, *BufferID, 0);
}

// TODO - This method isn't perfect. Some letters have KERN advance between them when in sentences.
// This doesnt take it into account since we bake each letter separately for future use by texture lookup
font *ResourceLoadFont(context *Context, path const Filename, uint32 FontHeight, int Char0, int CharN)
{
	if (FontHeight > 256) FontHeight = 256; // upper bound on font height
	if ((CharN - Char0) <= 0) return nullptr;
	real32 PixelHeight = (real32)FontHeight;

	path ResourceName;
	uint32 FilenameLen = (uint32)strlen(Filename);
	Assert(FilenameLen < (MAX_PATH - 4));
	strncpy(ResourceName, Filename, FilenameLen); // to add the 3 size characters + \0
	sprintf(ResourceName + FilenameLen, "%d", (int32)FontHeight);

	void *LoadedResource = ResourceCheckExist(&Context->RenderResources, RESOURCE_FONT, ResourceName);
	if (LoadedResource)
	{
		return (font*)LoadedResource;
	}

	font *Font = rf::PoolAlloc<font>(Context->SessionPool, 1);

	void *Contents = ReadFileContents(Context, Filename, 0);
	if (Contents)
	{
		stbtt_fontinfo STBFont;
		stbtt_InitFont(&STBFont, (uint8*)Contents, 0);

		real32 PixelScale = stbtt_ScaleForPixelHeight(&STBFont, PixelHeight);
		int Ascent, Descent, LineGap;
		stbtt_GetFontVMetrics(&STBFont, &Ascent, &Descent, &LineGap);
		Ascent = (int)floor(Ascent * PixelScale);
		Descent = (int)floor(Descent * PixelScale);

		Font->Width = 1024;
		Font->Height = 1024;
		Font->NumGlyphs = STBFont.numGlyphs;
		Font->Char0 = Char0;
		Font->CharN = CharN;
		Font->Buffer = rf::PoolAlloc<uint8>(Context->SessionPool, (uint32)(Font->Width*Font->Height));
		Font->Glyphs = rf::PoolAlloc<glyph>(Context->SessionPool, (uint32)(Font->CharN - Font->Char0));
		Font->LineGap = Ascent - Descent;
		Font->Ascent = Ascent;
		Font->MaxGlyphWidth = 0;
		Font->GlyphHeight = 0;

		real32 X = 0, Y = 0;

		for (int Codepoint = Font->Char0; Codepoint < Font->CharN; ++Codepoint)
		{
			int Glyph = stbtt_FindGlyphIndex(&STBFont, Codepoint);

			//real32 ShiftX = X - (real32)floor(X);
			int AdvX, Lsb;
			int X0, X1, Y0, Y1;
			stbtt_GetGlyphBitmapBox(&STBFont, Glyph, PixelScale, PixelScale, &X0, &Y0, &X1, &Y1);
			stbtt_GetGlyphHMetrics(&STBFont, Glyph, &AdvX, &Lsb);
			//int AdvKern = stbtt_GetCodepointKernAdvance(&STBFont, Codepoint, Codepoint+1);

			int CW = X1 - X0;
			int CH = Y1 - Y0;
			real32 AdvanceX = AdvX * PixelScale;

			if (X + AdvanceX >= Font->Width)
			{
				X = 0;
				Y += Font->LineGap;
				Assert((Y + Font->LineGap) < Font->Height);
			}

			int CharX = (int)ceil(X);
			int CharY = (int)ceil(Max(0.f, Y + Ascent + Y0));

			glyph &DstGlyph = Font->Glyphs[Codepoint - Font->Char0];
			DstGlyph.X = X0; DstGlyph.Y = Y0;
			DstGlyph.TexX0 = (X + 0) / (real32)Font->Width;            DstGlyph.TexX1 = (X + CW) / (real32)Font->Width;
			DstGlyph.TexY0 = (Y + Ascent + Y0) / (real32)Font->Height; DstGlyph.TexY1 = (Y + Ascent + Y1) / (real32)Font->Height;
			DstGlyph.CW = CW; DstGlyph.CH = CH; DstGlyph.AdvX = AdvanceX;

			Font->MaxGlyphWidth += AdvanceX;
			Font->GlyphHeight = Max(Font->GlyphHeight, CH);

			uint8 *BitmapPtr = Font->Buffer + (CharY * Font->Width + CharX);
			stbtt_MakeGlyphBitmap(&STBFont, BitmapPtr, CW, CH, Font->Width, PixelScale, PixelScale, Glyph);

			X += CW;
		}

		Font->MaxGlyphWidth /= real32(Font->CharN - Font->Char0);

		// Make Texture out of the Bitmap
		Font->AtlasTextureID = Make2DTexture(Font->Buffer, Font->Width, Font->Height, 1, false, false, 1.0f,
			GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		ResourceStore(&Context->RenderResources, RESOURCE_FONT, ResourceName, Font);
	}

	return Font;
}

uint32 _CompileShader(context *Context, char const *Src, int Type)
{
	GLuint Shader = glCreateShader(Type);

	glShaderSource(Shader, 1, &Src, NULL);
	glCompileShader(Shader);

	GLint Status;
	glGetShaderiv(Shader, GL_COMPILE_STATUS, &Status);

	if (!Status)
	{
		GLint Len;
		glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &Len);

		GLchar *Log = PoolAlloc<GLchar>(Context->ScratchPool, Len);
		glGetShaderInfoLog(Shader, Len, NULL, Log);

		LogError("Shader Compilation Error\n"
			"------------------------------------------\n"
			"%s"
			"------------------------------------------", Log);

		Shader = 0;
	}
	return Shader;
}

uint32 BuildShaderFromSource(context *Context, char const *VSrc, char const *FSrc, char const *GSrc, char const *TESCSrc, char const *TESESrc)
{
	uint32 ProgramID = glCreateProgram();

	uint32 VShader = _CompileShader(Context, VSrc, GL_VERTEX_SHADER);
	if (!VShader)
	{
		glDeleteProgram(ProgramID);
		return 0;
	}
	uint32 FShader = _CompileShader(Context, FSrc, GL_FRAGMENT_SHADER);
	if (!VShader)
	{
		glDeleteShader(VShader);
		glDeleteProgram(ProgramID);
		return 0;
	}

	uint32 GShader = 0;
	if (GSrc)
	{
		GShader = _CompileShader(Context, GSrc, GL_GEOMETRY_SHADER);
		if (!GShader)
		{
			glDeleteShader(VShader);
			glDeleteShader(FShader);
			glDeleteProgram(ProgramID);
			return 0;
		}
	}

	uint32 TESCShader = 0, TESEShader = 0;
	if (TESESrc)
	{
		TESEShader = _CompileShader(Context, TESESrc, GL_TESS_EVALUATION_SHADER);
		if (!TESEShader)
		{
			glDeleteShader(VShader); glDeleteShader(FShader);
			if (GShader) glDeleteShader(GShader);
			glDeleteProgram(ProgramID);
			return 0;
		}
	}
	if (TESCSrc)
	{
		TESCShader = _CompileShader(Context, TESCSrc, GL_TESS_CONTROL_SHADER);
		if (!TESCShader || !TESEShader) // cant have just control shader without evaluation shader
		{
			glDeleteShader(VShader); glDeleteShader(FShader);
			if (GShader) glDeleteShader(GShader);
			if (TESEShader) glDeleteShader(TESEShader);
			glDeleteProgram(ProgramID);
			return 0;
		}
	}

	glAttachShader(ProgramID, VShader);
	glAttachShader(ProgramID, FShader);
	if (GSrc)
	{
		glAttachShader(ProgramID, GShader);
		glDeleteShader(GShader);
	}
	if (TESCShader)
	{
		glAttachShader(ProgramID, TESCShader);
		glDeleteShader(TESCShader);
	}
	if (TESEShader)
	{
		glAttachShader(ProgramID, TESEShader);
		glDeleteShader(TESEShader);
	}

	glDeleteShader(VShader);
	glDeleteShader(FShader);

	glLinkProgram(ProgramID);

	GLint Status;
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Status);

	if (!Status)
	{
		GLint Len;
		glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &Len);

		GLchar *Log = PoolAlloc<GLchar>(Context->ScratchPool, Len);
		glGetProgramInfoLog(ProgramID, Len, NULL, Log);

		LogError("Shader Program link error : \n"
			"-----------------------------------------------------\n"
			"%s"
			"-----------------------------------------------------", Log);

		glDeleteProgram(ProgramID);
		return 0;
	}

	return ProgramID;
}

uint32 BuildShader(context *Context, char *VSPath, char *FSPath, char *GSPath, char *TESCPath, char *TESEPath)
{
	char *VSrc = NULL, *FSrc = NULL, *GSrc = NULL, *TESESrc = NULL, *TESCSrc = NULL;

	VSrc = (char*)ReadFileContents(Context, VSPath, 0);
	FSrc = (char*)ReadFileContents(Context, FSPath, 0);

	if (GSPath)
	{
		GSrc = (char*)ReadFileContents(Context, GSPath, 0);
	}

	if (TESCPath)
	{
		TESCSrc = (char*)ReadFileContents(Context, TESCPath, 0);
	}

	if (TESEPath)
	{
		TESESrc = (char*)ReadFileContents(Context, TESEPath, 0);
	}

	uint32 ProgramID = 0;
	bool IsValid = VSrc && FSrc && (!GSPath || GSrc) && (!TESEPath || TESESrc) && (!TESCPath || (TESESrc && TESCSrc));

	if (IsValid)
	{
		ProgramID = BuildShaderFromSource(Context, VSrc, FSrc, GSrc, TESCSrc, TESESrc);
	}

	return ProgramID;
}

void SendVec2(uint32 Loc, vec2f value)
{
	glUniform2fv(Loc, 1, (GLfloat const *)value);
}

void SendVec3(uint32 Loc, vec3f value)
{
	glUniform3fv(Loc, 1, (GLfloat const *)value);
}

void SendVec4(uint32 Loc, vec4f value)
{
	glUniform4fv(Loc, 1, (GLfloat const *)value);
}

void SendMat4(uint32 Loc, mat4f value)
{
	glUniformMatrix4fv(Loc, 1, GL_FALSE, (GLfloat const *)value);
}

void SendMat3(uint32 Loc, mat3f value)
{
	glUniformMatrix3fv(Loc, 1, GL_FALSE, (GLfloat const *)value);
}

void SendInt(uint32 Loc, int value)
{
	glUniform1i(Loc, value);
}

void SendFloat(uint32 Loc, real32 value)
{
	glUniform1f(Loc, value);
}


uint32 MakeVertexArrayObject()
{
	uint32 VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	return VAO;
}

uint32 AddEmptyVBO(size_t Size, uint32 Usage)
{
	uint32 Buffer;
	glGenBuffers(1, &Buffer);
	glBindBuffer(GL_ARRAY_BUFFER, Buffer);
	glBufferData(GL_ARRAY_BUFFER, Size, NULL, Usage);

	return Buffer;
}

void FillVBO(uint32 Attrib, uint32 AttribStride, uint32 Type,
	size_t ByteOffset, size_t Size, void const *Data)
{
	glEnableVertexAttribArray(Attrib);
	CheckGLError("VA");
	glBufferSubData(GL_ARRAY_BUFFER, ByteOffset, Size, Data);
	CheckGLError("SB");
	glVertexAttribPointer(Attrib, AttribStride, Type, GL_FALSE, 0, (GLvoid*)ByteOffset);
	CheckGLError("VAP");
}

uint32 AddVBO(uint32 Attrib, uint32 AttribStride, uint32 Type,
	uint32 Usage, uint32 Size, void const *Data)
{
	glEnableVertexAttribArray(Attrib);

	uint32 Buffer;
	glGenBuffers(1, &Buffer);
	glBindBuffer(GL_ARRAY_BUFFER, Buffer);
	glBufferData(GL_ARRAY_BUFFER, Size, Data, Usage);

	glVertexAttribPointer(Attrib, AttribStride, Type, GL_FALSE, 0, (GLvoid*)NULL);

	return Buffer;
}

void UpdateVBO(uint32 VBO, size_t ByteOffset, size_t Size, void *Data)
{
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferSubData(GL_ARRAY_BUFFER, ByteOffset, Size, Data);
}

uint32 AddIBO(uint32 Usage, size_t Size, void const *Data)
{
	uint32 Buffer;
	glGenBuffers(1, &Buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, Size, Data, Usage);

	return Buffer;
}

void DestroyMesh(mesh *Mesh)
{
	glDeleteBuffers(2, Mesh->VBO);
	glDeleteVertexArrays(1, &Mesh->VAO);
	Mesh->IndexCount = 0;
}

void RenderMesh(mesh *Mesh, uint32 GLDrawType)
{
	glDrawElements(GLDrawType, Mesh->IndexCount, Mesh->IndexType, 0);
}

static void FillCharInterleaved(real32 *VertData, uint16 *IdxData, uint32 i, uint16 c, font *Font, int *X, int *Y, vec3i const &Pos, real32 Scale)
{
	uint32 const Stride = 5 * 4;
	glyph &Glyph = Font->Glyphs[c];

#if 0
	// Modify DisplayWidth to always be at least the length of each character
	if (MaxPixelWidth < Glyph.CW)
	{
		MaxPixelWidth = Glyph.CW;
	}

	if ((X + Glyph.CW) >= MaxPixelWidth)
	{
		X = 0;
		Y -= Font->LineGap;
	}
#endif

	// Position (TL, BL, BR, TR)
	real32 BaseX = (real32)(*X + Glyph.X);
	real32 BaseY = (real32)(*Y - Font->Ascent - Glyph.Y);
	vec3f TL = Pos + vec3f(BaseX, BaseY, 0);
	vec3f BR = TL + vec3f(Scale * Glyph.CW, Scale * -Glyph.CH, 0);
	VertData[i*Stride + 0 + 0] = TL.x; VertData[i*Stride + 0 + 1] = TL.y;   VertData[i*Stride + 0 + 2] = TL.z;
	VertData[i*Stride + 3 + 0] = Glyph.TexX0;  VertData[i*Stride + 3 + 1] = Glyph.TexY0;
	VertData[i*Stride + 5 + 0] = TL.x; VertData[i*Stride + 5 + 1] = BR.y;   VertData[i*Stride + 5 + 2] = TL.z;
	VertData[i*Stride + 8 + 0] = Glyph.TexX0;  VertData[i*Stride + 8 + 1] = Glyph.TexY1;
	VertData[i*Stride + 10 + 0] = BR.x; VertData[i*Stride + 10 + 1] = BR.y;  VertData[i*Stride + 10 + 2] = TL.z;
	VertData[i*Stride + 13 + 0] = Glyph.TexX1; VertData[i*Stride + 13 + 1] = Glyph.TexY1;
	VertData[i*Stride + 15 + 0] = BR.x; VertData[i*Stride + 15 + 1] = TL.y;  VertData[i*Stride + 15 + 2] = TL.z;
	VertData[i*Stride + 18 + 0] = Glyph.TexX1; VertData[i*Stride + 18 + 1] = Glyph.TexY0;

	IdxData[i * 6 + 0] = i * 4 + 0; IdxData[i * 6 + 1] = i * 4 + 1; IdxData[i * 6 + 2] = i * 4 + 2;
	IdxData[i * 6 + 3] = i * 4 + 0; IdxData[i * 6 + 4] = i * 4 + 2; IdxData[i * 6 + 5] = i * 4 + 3;

	*X += (int)ceil(Glyph.AdvX);
}

void FillDisplayTextInterleaved(char const *Text, int32 TextLength, font *Font, vec3i Pos, int MaxPixelWidth,
	real32 *VertData, uint16 *IdxData, real32 Scale)
{
	// Precalculate if the text is larger than the authorized width
	real32 TextWidth = Scale * TextLength * Font->MaxGlyphWidth;
	uint32 AdditionalLen = 0;
	if (TextWidth >= MaxPixelWidth)
	{ // remove necessary charcount + 1
		TextLength -= (int)ceil((TextWidth - MaxPixelWidth) / Font->MaxGlyphWidth) + 1;
		TextLength = Max(0, TextLength);
		AdditionalLen = 2; // 2 dots
	}

	uint32 VertexCount = (TextLength + AdditionalLen) * 4;
	uint32 IndexCount = (TextLength + AdditionalLen) * 6;

	int X = 0, Y = 0;
	for (int32 i = 0; i < TextLength; ++i)
	{
		uint8 AsciiIdx = Text[i] - Font->Char0;

		if (Text[i] == '\n')
		{
			X = 0;
			Y -= (int)ceil(Scale * Font->LineGap);
			AsciiIdx = Text[++i] - Font->Char0;
			IndexCount -= 6;
		}

		FillCharInterleaved(VertData, IdxData, i, AsciiIdx, Font, &X, &Y, Pos, Scale);
	}

	// Add '..' to the string if it's too long and clamped
	if (AdditionalLen > 0)
	{
		uint8 AsciiIdx = '.' - Font->Char0;
		FillCharInterleaved(VertData, IdxData, TextLength++, AsciiIdx, Font, &X, &Y, Pos, Scale);
		FillCharInterleaved(VertData, IdxData, TextLength, AsciiIdx, Font, &X, &Y, Pos, Scale);
	}

}
void FillDisplayTextInterleavedUTF8(char const *Text, int32 TextLength, font *Font, vec3i Pos, int MaxPixelWidth,
	real32 *VertData, uint16 *IdxData, real32 Scale)
{
	uint32 VertexCount = TextLength * 4;
	uint32 IndexCount = TextLength * 6;

	int X = 0, Y = 0;
	size_t TextIdx = 0;
	real32 TextWidth = 0.f;
	for (int32 i = 0; i < TextLength; ++i)
	{
		size_t CharAdvance;
		uint16 UTFIdx = UTF8CharToInt(&Text[TextIdx], &CharAdvance) - Font->Char0;

		TextWidth += Scale * Font->Glyphs[UTFIdx].AdvX;
		if (TextWidth >= MaxPixelWidth)
			break;

		FillCharInterleaved(VertData, IdxData, i, UTFIdx, Font, &X, &Y, Pos, Scale);

		TextIdx += CharAdvance;
	}
}

real32 GetDisplayTextWidth(char const *Text, font *Font, real32 Scale)
{
	real32 TextWidth = 0.f;

	int UTFLen = UTF8CharCount(Text);
	if (UTFLen > 1)
	{
		uint32 TextLength = UTF8Len(Text);
		size_t TextIdx = 0;
		for (uint32 i = 0; i < TextLength; ++i)
		{
			size_t CharAdvance;
			uint16 UTFIdx = UTF8CharToInt(&Text[TextIdx], &CharAdvance) - Font->Char0;

			TextWidth += Scale * Font->Glyphs[UTFIdx].AdvX;
			TextIdx += CharAdvance;
		}
	}
	else
	{
		uint32 TextLength = (uint32)strlen(Text);
		for (uint32 i = 0; i < TextLength; ++i)
		{
			uint8 AsciiIdx = Text[i] - Font->Char0;
			TextWidth += Scale * Font->Glyphs[AsciiIdx].AdvX;
		}
	}

	return TextWidth;
}

display_text MakeDisplayText(context *Context, font *Font, char const *Msg, int MaxPixelWidth, vec4f Color, real32 Scale = 1.0f)
{
	display_text Text = {};

	uint32 MsgLength = (uint32)strlen(Msg);
	uint32 VertexCount = MsgLength * 4;
	uint32 IndexCount = MsgLength * 6;

	uint32 PS = 4 * 3;
	uint32 TS = 4 * 2;

	real32 *Positions = PoolAlloc<real32>(Context->ScratchPool, 3 * VertexCount);
	real32 *Texcoords = PoolAlloc<real32>(Context->ScratchPool, 2 * VertexCount);
	uint32 *Indices = PoolAlloc<uint32>(Context->ScratchPool, IndexCount);

	int X = 0, Y = 0;
	for (uint32 i = 0; i < MsgLength; ++i)
	{
		uint8 AsciiIdx = Msg[i] - Font->Char0;
		glyph &Glyph = Font->Glyphs[AsciiIdx];

		// Modify DisplayWidth to always be at least the length of each character
		if (MaxPixelWidth < Glyph.CW)
		{
			MaxPixelWidth = Glyph.CW;
		}

		if (Msg[i] == '\n')
		{
			X = 0;
			Y -= Font->LineGap;
			AsciiIdx = Msg[++i] - Font->Char0;
			Glyph = Font->Glyphs[AsciiIdx];
			IndexCount -= 6;
		}

		if ((X + Glyph.CW) >= MaxPixelWidth)
		{
			X = 0;
			Y -= Font->LineGap;
		}

		// Position (TL, BL, BR, TR)
		real32 BaseX = (real32)(X + Glyph.X);
		real32 BaseY = (real32)(Y - Font->Ascent - Glyph.Y);
		Positions[i*PS + 0 + 0] = Scale * (BaseX);              Positions[i*PS + 0 + 1] = Scale * (BaseY);            Positions[i*PS + 0 + 2] = 0;
		Positions[i*PS + 3 + 0] = Scale * (BaseX);              Positions[i*PS + 3 + 1] = Scale * (BaseY - Glyph.CH); Positions[i*PS + 3 + 2] = 0;
		Positions[i*PS + 6 + 0] = Scale * (BaseX + Glyph.CW);   Positions[i*PS + 6 + 1] = Scale * (BaseY - Glyph.CH); Positions[i*PS + 6 + 2] = 0;
		Positions[i*PS + 9 + 0] = Scale * (BaseX + Glyph.CW);   Positions[i*PS + 9 + 1] = Scale * (BaseY);            Positions[i*PS + 9 + 2] = 0;

		// texcoords
		Texcoords[i*TS + 0 + 0] = Glyph.TexX0; Texcoords[i*TS + 0 + 1] = Glyph.TexY0;
		Texcoords[i*TS + 2 + 0] = Glyph.TexX0; Texcoords[i*TS + 2 + 1] = Glyph.TexY1;
		Texcoords[i*TS + 4 + 0] = Glyph.TexX1; Texcoords[i*TS + 4 + 1] = Glyph.TexY1;
		Texcoords[i*TS + 6 + 0] = Glyph.TexX1; Texcoords[i*TS + 6 + 1] = Glyph.TexY0;

		Indices[i * 6 + 0] = i * 4 + 0; Indices[i * 6 + 1] = i * 4 + 1; Indices[i * 6 + 2] = i * 4 + 2;
		Indices[i * 6 + 3] = i * 4 + 0; Indices[i * 6 + 4] = i * 4 + 2; Indices[i * 6 + 5] = i * 4 + 3;

		X += (int)ceil(Glyph.AdvX);
	}

	Text.VAO = MakeVertexArrayObject();
	Text.VBO[0] = AddIBO(GL_STATIC_DRAW, IndexCount * sizeof(uint32), Indices);
	Text.VBO[1] = AddEmptyVBO(5 * VertexCount * sizeof(real32), GL_STATIC_DRAW);
	FillVBO(0, 3, GL_FLOAT, 0, 3 * VertexCount * sizeof(real32), Positions);
	FillVBO(1, 2, GL_FLOAT, 3 * VertexCount * sizeof(real32), 2 * VertexCount * sizeof(real32), Texcoords);
	Text.IndexCount = IndexCount;
	glBindVertexArray(0);

	Text.Texture = Font->AtlasTextureID;
	Text.Color = Color;

	return Text;
}

void DestroyDisplayText(display_text *Text)
{
	glDeleteBuffers(2, Text->VBO);
	glDeleteVertexArrays(1, &Text->VAO);
	Text->IndexCount = 0;
}

////////////////////////////////////////////////////////////////////////
// NOTE - Primitive builders
////////////////////////////////////////////////////////////////////////

mesh MakeUnitCube(bool MakeAdditionalAttribs)
{
	mesh Cube = {};

	vec3f const Position[24] = {
		vec3f(-1, -1, -1),
		vec3f(-1, -1, 1),
		vec3f(-1, 1, 1),
		vec3f(-1, 1, -1),

		vec3f(1, -1, 1),
		vec3f(1, -1, -1),
		vec3f(1, 1, -1),
		vec3f(1, 1, 1),

		vec3f(-1, -1, 1),
		vec3f(-1, -1, -1),
		vec3f(1, -1, -1),
		vec3f(1, -1, 1),

		vec3f(-1, 1, -1),
		vec3f(-1, 1, 1),
		vec3f(1, 1, 1),
		vec3f(1, 1, -1),

		vec3f(1, 1, -1),
		vec3f(1, -1, -1),
		vec3f(-1, -1, -1),
		vec3f(-1, 1, -1),

		vec3f(-1, 1, 1),
		vec3f(-1, -1, 1),
		vec3f(1, -1, 1),
		vec3f(1, 1, 1)
	};

	vec2f const Texcoord[24] = {
		vec2f(0, 1),
		vec2f(0, 0),
		vec2f(1, 0),
		vec2f(1, 1),

		vec2f(0, 1),
		vec2f(0, 0),
		vec2f(1, 0),
		vec2f(1, 1),

		vec2f(0, 1),
		vec2f(0, 0),
		vec2f(1, 0),
		vec2f(1, 1),

		vec2f(0, 1),
		vec2f(0, 0),
		vec2f(1, 0),
		vec2f(1, 1),

		vec2f(0, 1),
		vec2f(0, 0),
		vec2f(1, 0),
		vec2f(1, 1),

		vec2f(0, 1),
		vec2f(0, 0),
		vec2f(1, 0),
		vec2f(1, 1),
	};

	vec3f const Normal[24] = {
		vec3f(-1, 0, 0),
		vec3f(-1, 0, 0),
		vec3f(-1, 0, 0),
		vec3f(-1, 0, 0),

		vec3f(1, 0, 0),
		vec3f(1, 0, 0),
		vec3f(1, 0, 0),
		vec3f(1, 0, 0),

		vec3f(0, -1, 0),
		vec3f(0, -1, 0),
		vec3f(0, -1, 0),
		vec3f(0, -1, 0),

		vec3f(0, 1, 0),
		vec3f(0, 1, 0),
		vec3f(0, 1, 0),
		vec3f(0, 1, 0),

		vec3f(0, 0, -1),
		vec3f(0, 0, -1),
		vec3f(0, 0, -1),
		vec3f(0, 0, -1),

		vec3f(0, 0, 1),
		vec3f(0, 0, 1),
		vec3f(0, 0, 1),
		vec3f(0, 0, 1)
	};

	vec4f Tangent[24]; vec3f Dummy;
	for (uint32 i = 0; i < 24; ++i)
	{
		BasisFrisvad(Normal[i], *((vec3f*)&Tangent[i]), Dummy);
		Tangent[i].w = 1.f;
	}

	uint32 Indices[36];
	for (uint32 i = 0; i < 6; ++i)
	{
		Indices[i * 6 + 0] = i * 4 + 0;
		Indices[i * 6 + 1] = i * 4 + 1;
		Indices[i * 6 + 2] = i * 4 + 2;

		Indices[i * 6 + 3] = i * 4 + 0;
		Indices[i * 6 + 4] = i * 4 + 2;
		Indices[i * 6 + 5] = i * 4 + 3;
	}

	Cube.IndexCount = 36;
	Cube.IndexType = GL_UNSIGNED_INT;
	Cube.VAO = MakeVertexArrayObject();
	Cube.VBO[0] = AddIBO(GL_STATIC_DRAW, sizeof(Indices), Indices);
	if (MakeAdditionalAttribs)
	{
		Cube.VBO[1] = AddEmptyVBO(sizeof(Position) + sizeof(Texcoord) + sizeof(Normal) + sizeof(Tangent), GL_STATIC_DRAW);
		size_t offset = 0;
		FillVBO(0, 3, GL_FLOAT, (offset += 0), sizeof(Position), Position);
		FillVBO(1, 2, GL_FLOAT, (offset += sizeof(Position)), sizeof(Texcoord), Texcoord);
		FillVBO(2, 3, GL_FLOAT, (offset += sizeof(Texcoord)), sizeof(Normal), Normal);
		FillVBO(3, 4, GL_FLOAT, (offset += sizeof(Normal)), sizeof(Tangent), Tangent);
	}
	else
	{
		Cube.VBO[1] = AddVBO(0, 3, GL_FLOAT, GL_STATIC_DRAW, sizeof(Position), Position);
	}
	glBindVertexArray(0);

	return Cube;
}

// NOTE - Start is Top Left corner. End is Bottom Right corner.
mesh Make2DQuad(context *Context, vec2f Start, vec2f End, int Subdivisions)
{
	mesh Quad = {};

	uint32 QuadCount1D = (int)pow(2, Subdivisions);
	uint32 QuadCount = QuadCount1D * QuadCount1D;
	uint32 TriangleCount = QuadCount * 2;
	Quad.IndexCount = TriangleCount * 3;
	uint32 VCount1D = QuadCount1D + 1;
	uint32 VertexCount = VCount1D * VCount1D;

	vec2f Rect(End.x - Start.x, End.y - Start.y);
	vec2f Stride = Rect / (real32)QuadCount1D;
	vec2f TexStride = vec2f(1, 1) / (real32)QuadCount1D;

	vec2f *Positions = PoolAlloc<vec2f>(Context->ScratchPool, VertexCount);
	vec2f *Texcoords = PoolAlloc<vec2f>(Context->ScratchPool, VertexCount);
	uint16 *Indices = PoolAlloc<uint16>(Context->ScratchPool, Quad.IndexCount);

	// vertices ordered from (top left) to (bottom right), line by line as an array
	for (uint32 j = 0; j < VCount1D; ++j)
	{
		for (uint32 i = 0; i < VCount1D; ++i)
		{
			Positions[j * VCount1D + i] = Start + vec2f(i * Stride.x, j * Stride.y);
			Texcoords[j * VCount1D + i] = vec2f(0.f, 1.f) + vec2f(i * TexStride.x, j * -TexStride.y);
		}
	}

	for (uint32 j = 0; j < (VCount1D - 1); ++j)
	{
		for (uint32 i = 0; i < (VCount1D - 1); ++i)
		{
			uint32 IdxBase = j * (VCount1D - 1) + i;
			Indices[IdxBase * 6 + 0] = (j * VCount1D + i) + 0;
			Indices[IdxBase * 6 + 1] = (j * VCount1D + i) + VCount1D;
			Indices[IdxBase * 6 + 2] = (j * VCount1D + i) + VCount1D + 1;
			Indices[IdxBase * 6 + 3] = (j * VCount1D + i) + 0;
			Indices[IdxBase * 6 + 4] = (j * VCount1D + i) + VCount1D + 1;
			Indices[IdxBase * 6 + 5] = (j * VCount1D + i) + 1;
		}
	}

	Quad.IndexType = GL_UNSIGNED_SHORT;
	Quad.VAO = MakeVertexArrayObject();
	Quad.VBO[0] = AddIBO(GL_STATIC_DRAW, Quad.IndexCount * sizeof(uint16), Indices);
	Quad.VBO[1] = AddEmptyVBO(VertexCount * 2 * sizeof(vec2f), GL_STATIC_DRAW);
	FillVBO(0, 2, GL_FLOAT, 0, VertexCount * sizeof(vec2f), Positions);
	FillVBO(1, 2, GL_FLOAT, VertexCount * sizeof(vec2f), VertexCount * sizeof(vec2f), Texcoords);
	glBindVertexArray(0);

	return Quad;
}

mesh Make2DCircle(context *Context, vec2f Center, real32 Radius, int Segments)
{
	mesh Circle = {};

	uint32 nVerts = Segments + 1;

	return Circle;
}

mesh Make3DPlane(context *Context, vec2i Dimension, uint32 Subdivisions, uint32 TextureRepeatCount, bool Dynamic)
{
	mesh Plane = {};

	uint32 BaseSize = Square(Subdivisions);
	uint32 PositionsSize = 4 * BaseSize * sizeof(vec3f);
	uint32 NormalsSize = 4 * BaseSize * sizeof(vec3f);
	uint32 TexcoordsSize = 4 * BaseSize * sizeof(vec2f);
	uint32 IndicesSize = 6 * BaseSize * sizeof(uint32);
	uint32 TangentsSize = 4 * BaseSize * sizeof(vec4f);

	vec3f *Positions = PoolAlloc<vec3f>(Context->ScratchPool, PositionsSize);
	vec3f *Normals = PoolAlloc<vec3f>(Context->ScratchPool, NormalsSize);
	vec2f *Texcoords = PoolAlloc<vec2f>(Context->ScratchPool, TexcoordsSize);
	vec4f *Tangents = PoolAlloc<vec4f>(Context->ScratchPool, TangentsSize);
	uint32 *Indices = PoolAlloc<uint32>(Context->ScratchPool, IndicesSize);

	vec2i SubdivDim = Dimension / Subdivisions;
	vec2f TexMax = vec2f(Dimension) / (real32)TextureRepeatCount;

	for (uint32 j = 0; j < Subdivisions; ++j)
	{
		for (uint32 i = 0; i < Subdivisions; ++i)
		{
			uint32 Idx = j * Subdivisions + i;

			Positions[Idx * 4 + 0] = vec3f((real32)i*SubdivDim.x, 0.f, (real32)j*SubdivDim.y);
			Positions[Idx * 4 + 1] = vec3f((real32)i*SubdivDim.x, 0.f, real32(j + 1)*SubdivDim.y);
			Positions[Idx * 4 + 2] = vec3f(real32(i + 1)*SubdivDim.x, 0.f, real32(j + 1)*SubdivDim.y);
			Positions[Idx * 4 + 3] = vec3f(real32(i + 1)*SubdivDim.x, 0.f, (real32)j*SubdivDim.y);

			Normals[Idx * 4 + 0] = vec3f(0, 1, 0); Tangents[Idx * 4 + 0] = vec4f(1, 0, 0, 1);
			Normals[Idx * 4 + 1] = vec3f(0, 1, 0); Tangents[Idx * 4 + 1] = vec4f(1, 0, 0, 1);
			Normals[Idx * 4 + 2] = vec3f(0, 1, 0); Tangents[Idx * 4 + 2] = vec4f(1, 0, 0, 1);
			Normals[Idx * 4 + 3] = vec3f(0, 1, 0); Tangents[Idx * 4 + 3] = vec4f(1, 0, 0, 1);

			Texcoords[Idx * 4 + 0] = vec2f(0.f, TexMax.y);
			Texcoords[Idx * 4 + 1] = vec2f(0.f, 0.f);
			Texcoords[Idx * 4 + 2] = vec2f(TexMax.x, 0.f);
			Texcoords[Idx * 4 + 3] = vec2f(TexMax.x, TexMax.y);

			Indices[Idx * 6 + 0] = Idx * 4 + 0;
			Indices[Idx * 6 + 1] = Idx * 4 + 1;
			Indices[Idx * 6 + 2] = Idx * 4 + 2;
			Indices[Idx * 6 + 3] = Idx * 4 + 0;
			Indices[Idx * 6 + 4] = Idx * 4 + 2;
			Indices[Idx * 6 + 5] = Idx * 4 + 3;
		}
	}

	Plane.IndexCount = 6 * BaseSize;
	Plane.IndexType = GL_UNSIGNED_INT;
	Plane.VAO = MakeVertexArrayObject();
	Plane.VBO[0] = AddIBO(GL_STATIC_DRAW, IndicesSize, Indices);
	// Positions and Normals in the 1st VBO
	Plane.VBO[1] = AddEmptyVBO(PositionsSize + NormalsSize + TexcoordsSize + TangentsSize, Dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	size_t offset = 0;
	FillVBO(0, 3, GL_FLOAT, 0, PositionsSize, Positions);
	FillVBO(1, 2, GL_FLOAT, (offset += PositionsSize), TexcoordsSize, Texcoords);
	FillVBO(2, 3, GL_FLOAT, (offset += TexcoordsSize), NormalsSize, Normals);
	FillVBO(3, 4, GL_FLOAT, (offset += NormalsSize), TangentsSize, Tangents);
	glBindVertexArray(0);

	return Plane;
}

mesh MakeUnitSphere(bool MakeAdditionalAttribs, real32 TexScale)
{
	mesh Sphere = {};

	real32 const Radius = 1.f;
	uint32 const nLon = 32, nLat = 24;

	uint32 const nVerts = (nLon + 1) * nLat + 2;
	uint32 const nIndices = (nLat - 1)*nLon * 6 + nLon * 2 * 3;

	// Positions
	vec3f Position[nVerts];
	Position[0] = vec3f(0, 1, 0) * Radius;
	for (uint32 lat = 0; lat < nLat; ++lat)
	{
		real32 a1 = M_PI * (real32)(lat + 1) / (nLat + 1);
		real32 sin1 = sinf(a1);
		real32 cos1 = cosf(a1);

		for (uint32 lon = 0; lon <= nLon; ++lon)
		{
			real32 a2 = M_TWO_PI * (real32)(lon == nLon ? 0 : lon) / nLon;
			real32 sin2 = sinf(a2);
			real32 cos2 = cosf(a2);

			Position[lon + lat * (nLon + 1) + 1] = vec3f(sin1 * cos2, cos1, sin1 * sin2) * Radius;
		}
	}
	Position[nVerts - 1] = vec3f(0, 1, 0) * -Radius;

	// Normals
	vec3f Normal[nVerts];
	vec4f Tangent[nVerts]; vec3f Dummy;
	for (uint32 i = 0; i < nVerts; ++i)
	{
		Normal[i] = Normalize(Position[i]);
		BasisFrisvad(Normal[i], *((vec3f*)&Tangent[i]), Dummy);
		Tangent[i].w = 1.f;
	}

	// UVs
	vec2f Texcoord[nVerts];
	Texcoord[0] = vec2f(0, TexScale);
	Texcoord[nVerts - 1] = vec2f(0.f);
	for (uint32 lat = 0; lat < nLat; ++lat)
	{
		for (uint32 lon = 0; lon <= nLon; ++lon)
		{
			Texcoord[lon + lat * (nLon + 1) + 1] = vec2f(lon / (real32)nLon, 1.f - (lat + 1) / (real32)(nLat + 1)) * TexScale;
		}
	}

	// Triangles/Indices
	uint32 Indices[nIndices];
	{
		// top
		uint32 i = 0;
		for (uint32 lon = 0; lon < nLon; ++lon)
		{
			Indices[i++] = lon + 2;
			Indices[i++] = lon + 1;
			Indices[i++] = 0;
		}

		// middle
		for (uint32 lat = 0; lat < nLat - 1; ++lat)
		{
			for (uint32 lon = 0; lon < nLon; ++lon)
			{
				uint32 curr = lon + lat * (nLon + 1) + 1;
				uint32 next = curr + nLon + 1;

				Indices[i++] = curr;
				Indices[i++] = curr + 1;
				Indices[i++] = next + 1;

				Indices[i++] = curr;
				Indices[i++] = next + 1;
				Indices[i++] = next;
			}
		}

		// bottom
		for (uint32 lon = 0; lon < nLon; ++lon)
		{
			Indices[i++] = nVerts - 1;
			Indices[i++] = nVerts - (lon + 2) - 1;
			Indices[i++] = nVerts - (lon + 1) - 1;
		}
	}

	Sphere.IndexCount = nIndices;
	Sphere.IndexType = GL_UNSIGNED_INT;
	Sphere.VAO = MakeVertexArrayObject();
	Sphere.VBO[0] = AddIBO(GL_STATIC_DRAW, sizeof(Indices), Indices);
	if (MakeAdditionalAttribs)
	{
		Sphere.VBO[1] = AddEmptyVBO(sizeof(Position) + sizeof(Texcoord) + sizeof(Normal) + sizeof(Tangent), GL_STATIC_DRAW);
		size_t offset = 0;
		FillVBO(0, 3, GL_FLOAT, 0, sizeof(Position), Position);
		FillVBO(1, 2, GL_FLOAT, (offset += sizeof(Position)), sizeof(Texcoord), Texcoord);
		FillVBO(2, 3, GL_FLOAT, (offset += sizeof(Texcoord)), sizeof(Normal), Normal);
		FillVBO(3, 4, GL_FLOAT, (offset += sizeof(Normal)), sizeof(Tangent), Tangent);
	}
	else
	{
		Sphere.VBO[1] = AddVBO(0, 3, GL_FLOAT, GL_STATIC_DRAW, sizeof(Position), Position);
	}
	glBindVertexArray(0);

	return Sphere;
}

uint32 MakeUBO(size_t Size, GLenum DrawType)
{
	uint32 UBO = 0;
	glGenBuffers(1, &UBO);
	glBindBuffer(GL_UNIFORM_BUFFER, UBO);
	glBufferData(GL_UNIFORM_BUFFER, Size, NULL, DrawType);
	return UBO;
}

void BindUBO(uint32 ID, int Target)
{
	glBindBufferBase(GL_UNIFORM_BUFFER, Target, ID);
	glBindBuffer(GL_UNIFORM_BUFFER, ID);
}

void FillUBO(size_t Offset, size_t Size, void const *Data)
{
	glBufferSubData(GL_UNIFORM_BUFFER, Offset, Size, Data);
}

void DestroyUBO(uint32 ID)
{
	glDeleteBuffers(1, &ID);
}

void ComputeIrradianceCubemap(context *Context, char const *HDREnvmapFilename,
	uint32 *HDRCubemapEnvmap, uint32 *HDRGlossyEnvmap, uint32 *HDRIrradianceEnvmap)
{
	// TODO - Parameterize this ?
	uint32 CubemapWidth = 512;
	uint32 IrradianceCubemapWidth = 32;
	uint32 GlossyCubemapWidth = 128;

	uint32 ProgramLatlong2Cubemap;
	uint32 ProgramCubemapConvolution;
	uint32 ProgramCubemapPrefilter;

	path VSPath;
	path FSPath;
	ConcatStrings(VSPath, ctx::GetExePath(Context), "data/shaders/skybox_vert.glsl");
	ConcatStrings(FSPath, ctx::GetExePath(Context), "data/shaders/latlong2cubemap_frag.glsl");
	ProgramLatlong2Cubemap = BuildShader(Context, VSPath, FSPath);
	glUseProgram(ProgramLatlong2Cubemap);
	SendInt(glGetUniformLocation(ProgramLatlong2Cubemap, "Envmap"), 0);
	CheckGLError("Latlong Shader");

	ConcatStrings(FSPath, ctx::GetExePath(Context), "data/shaders/cubemapconvolution_frag.glsl");
	ProgramCubemapConvolution = BuildShader(Context, VSPath, FSPath);
	glUseProgram(ProgramCubemapConvolution);
	SendInt(glGetUniformLocation(ProgramCubemapConvolution, "Cubemap"), 0);
	CheckGLError("Convolution Shader");

	ConcatStrings(FSPath, ctx::GetExePath(Context), "data/shaders/cubemapprefilter_frag.glsl");
	ProgramCubemapPrefilter = BuildShader(Context, VSPath, FSPath);
	glUseProgram(ProgramCubemapPrefilter);
	SendInt(glGetUniformLocation(ProgramCubemapPrefilter, "Cubemap"), 0);
	CheckGLError("Prefilter Shader");

	frame_buffer FBOEnvmap = MakeFramebuffer(1, vec2i(CubemapWidth, CubemapWidth));

	image *HDREnvmapImage = ResourceLoadImage(Context, HDREnvmapFilename, true);

	uint32 HDRLatlongEnvmap = Make2DTexture(HDREnvmapImage->Buffer, HDREnvmapImage->Width, HDREnvmapImage->Height,
		HDREnvmapImage->Channels, true, false, 1, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, GL_REPEAT, GL_MIRRORED_REPEAT);

	mesh SkyboxCube = MakeUnitCube(false);

	*HDRCubemapEnvmap = MakeCubemap(NULL, NULL, true, true, CubemapWidth, CubemapWidth, true);
	CheckGLError("Latlong2Cubmap");
	*HDRIrradianceEnvmap = MakeCubemap(NULL, NULL, true, false, IrradianceCubemapWidth, IrradianceCubemapWidth, false);
	CheckGLError("IrradianceCubemap");
	*HDRGlossyEnvmap = MakeCubemap(NULL, NULL, true, true, GlossyCubemapWidth, GlossyCubemapWidth, true);
	CheckGLError("GlossyCubemap");

	glDisable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);

	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(SkyboxCube.VAO);
	// The 6 view matrices for the 6 cubemap directions
	mat4f const ViewDirs[6] = {
		mat4f::LookAt(vec3f(0), vec3f(1, 0, 0), vec3f(0, -1, 0)),
		mat4f::LookAt(vec3f(0), vec3f(-1, 0, 0), vec3f(0, -1, 0)),
		mat4f::LookAt(vec3f(0), vec3f(0, 1, 0), vec3f(0, 0, 1)),
		mat4f::LookAt(vec3f(0), vec3f(0,-1, 0), vec3f(0, 0,-1)),
		mat4f::LookAt(vec3f(0), vec3f(0, 0, 1), vec3f(0, -1, 0)),
		mat4f::LookAt(vec3f(0), vec3f(0, 0,-1), vec3f(0, -1, 0))
	};
	mat4f const EnvmapProjectionMatrix = mat4f::Perspective(90.f, 1.f, 0.1f, 10.f);

	// NOTE - Latlong to Cubemap
	glUseProgram(ProgramLatlong2Cubemap);
	{
		uint32 Loc = glGetUniformLocation(ProgramLatlong2Cubemap, "ProjMatrix");
		SendMat4(Loc, EnvmapProjectionMatrix);
		CheckGLError("ProjMatrix Latlong2Cubemap");
	}

	uint32 ViewLoc = glGetUniformLocation(ProgramLatlong2Cubemap, "ViewMatrix");

	glViewport(0, 0, CubemapWidth, CubemapWidth);
	glClearColor(0, 0, 0, 0);
	glBindTexture(GL_TEXTURE_2D, HDRLatlongEnvmap);
	glBindFramebuffer(GL_FRAMEBUFFER, FBOEnvmap.FBO);
	for (int i = 0; i < 6; ++i)
	{
		SendMat4(ViewLoc, ViewDirs[i]);
		CheckGLError("ViewMatrix Latlong2Cubemap");
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
			*HDRCubemapEnvmap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDrawElements(GL_TRIANGLES, SkyboxCube.IndexCount, GL_UNSIGNED_INT, 0);
	}

	glBindTexture(GL_TEXTURE_CUBE_MAP, *HDRCubemapEnvmap);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// NOTE - Cubemap convolution
	glUseProgram(ProgramCubemapConvolution);
	{
		uint32 Loc = glGetUniformLocation(ProgramCubemapConvolution, "ProjMatrix");
		SendMat4(Loc, EnvmapProjectionMatrix);
		CheckGLError("ProjMatrix CubemapConvolution");
	}

	ViewLoc = glGetUniformLocation(ProgramCubemapConvolution, "ViewMatrix");

	glViewport(0, 0, IrradianceCubemapWidth, IrradianceCubemapWidth);
	glBindRenderbuffer(GL_RENDERBUFFER, FBOEnvmap.DepthBufferID);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, IrradianceCubemapWidth, IrradianceCubemapWidth);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, *HDRCubemapEnvmap);
	for (int i = 0; i < 6; ++i)
	{
		SendMat4(ViewLoc, ViewDirs[i]);
		CheckGLError("ViewMatrix Cubemap Convolution");
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
			*HDRIrradianceEnvmap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDrawElements(GL_TRIANGLES, SkyboxCube.IndexCount, GL_UNSIGNED_INT, 0);
	}

	// NOTE - Cubemap glossy prefilter
	glUseProgram(ProgramCubemapPrefilter);
	{
		uint32 Loc = glGetUniformLocation(ProgramCubemapPrefilter, "ProjMatrix");
		SendMat4(Loc, EnvmapProjectionMatrix);
		CheckGLError("ProjMatrix CubemapPrefilter");
	}

	ViewLoc = glGetUniformLocation(ProgramCubemapPrefilter, "ViewMatrix");
	uint32 RoughnessLoc = glGetUniformLocation(ProgramCubemapPrefilter, "Roughness");

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, *HDRCubemapEnvmap);
	uint32 const MaxMipLevels = 5;
	for (uint32 mip = 0; mip < MaxMipLevels; ++mip)
	{
		rf::CheckGLError("ClearMip");
		uint32 const MipW = (uint32)(GlossyCubemapWidth * std::pow(0.5, mip));
		uint32 const MipH = MipW;
		glBindRenderbuffer(GL_RENDERBUFFER, FBOEnvmap.DepthBufferID);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, MipW, MipH);

		rf::CheckFramebufferError("Renderbufferstorage");
		glViewport(0, 0, MipW, MipH);

		real32 Roughness = (real32)mip / (real32)(MaxMipLevels - 1);
		SendFloat(RoughnessLoc, Roughness);
		for (uint32 i = 0; i < 6; ++i)
		{
			SendMat4(ViewLoc, ViewDirs[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				*HDRGlossyEnvmap, mip);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDrawElements(GL_TRIANGLES, SkyboxCube.IndexCount, GL_UNSIGNED_INT, 0);
		}
	}
	rf::CheckGLError("CubemapEnvEnd");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);

	glDeleteTextures(1, &HDRLatlongEnvmap);
	glDeleteProgram(ProgramLatlong2Cubemap);
	glDeleteProgram(ProgramCubemapConvolution);
	glDeleteProgram(ProgramCubemapPrefilter);
	DestroyFramebuffer(&FBOEnvmap);
	DestroyMesh(&SkyboxCube);
	rf::CheckGLError("CubemapEnvEndDestroy");
}

uint32 PrecomputeGGXLUT(context *Context, uint32 Width)
{
	uint32 LUTProgram;
	path VSPath;
	path FSPath;
	ConcatStrings(VSPath, ctx::GetExePath(Context), "data/shaders/screenquad_vert.glsl");
	ConcatStrings(FSPath, ctx::GetExePath(Context), "data/shaders/ggxintegrate_frag.glsl");
	LUTProgram = BuildShader(Context, VSPath, FSPath);

	glActiveTexture(GL_TEXTURE0);
	uint32 GGXLUT = Make2DTexture(NULL, Width, Width,
		2, true, true, 1, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, GGXLUT);

	frame_buffer FBO = MakeFramebuffer(1, vec2i(Width, Width));
	mesh ScreenQuad = Make2DQuad(Context, vec2i(-1, 1), vec2i(1, -1));

	glBindFramebuffer(GL_FRAMEBUFFER, FBO.FBO);
	glBindRenderbuffer(GL_RENDERBUFFER, FBO.DepthBufferID);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, Width, Width);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GGXLUT, 0);

	glViewport(0, 0, Width, Width);
	glUseProgram(LUTProgram);
	glBindVertexArray(ScreenQuad.VAO);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDrawElements(GL_TRIANGLES, ScreenQuad.IndexCount, ScreenQuad.IndexType, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteProgram(LUTProgram);
	DestroyFramebuffer(&FBO);
	DestroyMesh(&ScreenQuad);

	CheckGLError("PrecomputeGGXLUT");

	return GGXLUT;
}
}
