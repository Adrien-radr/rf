#ifndef RF_RENDER_H
#define RF_RENDER_H

#include "rf_defs.h"
#include "GL/glew.h"
#include <map>

namespace rf {
#define MAX_FBO_ATTACHMENTS 5

struct image
{
    void *Buffer;
    int32 Width;
    int32 Height;
    int32 Channels;
};

struct glyph
{
    // NOTE - Schema of behavior
    //   X,Y 0---------o   x
    //       |         |   |
    //       |         |   |
    //       |         |   | CH
    //       |         |   |
    //       0---------o   v
    //       x---------> CW
    //       x-----------> AdvX
    int X, Y;
    real32 TexX0, TexY0, TexX1, TexY1; // Absolute texcoords in Font Bitmap where the char is
    int CW, CH;
    real32 AdvX;
};

// NOTE - Ascii-only
// TODO - UTF
struct font
{
    int Width;
    int Height;
    int LineGap;
    int Ascent;
    int NumGlyphs;
    int Char0, CharN;
    real32 MaxGlyphWidth;
    real32 GlyphHeight;
    uint32 AtlasTextureID;
    uint8 *Buffer;
    glyph *Glyphs;
};

struct display_text
{
    uint32 VAO;
    uint32 VBO[2]; // 0 : positions+texcoords batched, 1 : indices
    uint32 IndexCount;
    uint32 Texture;
    vec4f  Color;
};

struct mesh
{
    uint32 VAO;
    uint32 VBO[5]; // 0: indices, 1-5 : position, normal, texcoords, tangent, bitangent
    uint32 IndexCount;
    uint32 IndexType;
    mat4f  ModelMatrix;
};

struct material
{
    uint32 AlbedoTexture;
    uint32 RoughnessMetallicTexture;
    uint32 NormalTexture;
    uint32 EmissiveTexture;

    vec3f AlbedoMult;
    vec3f EmissiveMult;
    float RoughnessMult;
    float MetallicMult;
};

struct model
{
	mesh	 *Meshes;
	int		 *MaterialIdx;
	material *Materials;
};

struct frame_buffer
{
    vec2i Size;
    uint32 NumAttachments;
    uint32 FBO;
    uint32 DepthBufferID;
    uint32 BufferIDs[MAX_FBO_ATTACHMENTS];
};

enum render_resource_type
{
    RESOURCE_IMAGE,
    RESOURCE_TEXTURE,
    RESOURCE_FONT,
    RESOURCE_COUNT
};

typedef map_store resource_store;

struct render_resources
{
    path ExecutablePath;

    uint32 *DefaultDiffuseTexture;
    uint32 *DefaultNormalTexture;
    uint32 *DefaultEmissiveTexture;

    resource_store Images;
    resource_store Textures;
    resource_store Fonts;
};

/// Error Handling
void            CheckGLError(char const *Mark = "");
void            CheckFramebufferError(char const *Mark = "");

/// Resource loading and storage
void            *ResourceCheckExist(render_resources *RenderResources, render_resource_type Type, path const Filename);
void            ResourceStore(render_resources *RenderResources, render_resource_type Type, path const Filename, void *Resource);
void            ResourceFree(render_resources *RenderResources);
image           *ResourceLoadImage(context *Context, path const Filename, bool IsFloat, bool FlipY = true,
                    int32 ForceNumChannel = 0);
font            *ResourceLoadFont(context *Context, path const Filename, uint32 PixelHeight, int Char0 = 32, int CharN = 127);
uint32          *ResourceLoad2DTexture(context *Context, path const Filename, bool IsFloat, bool FloatHalfPrecision,
                    uint32 AnisotropicLevel, int MagFilter = GL_LINEAR, int MinFilter = GL_LINEAR_MIPMAP_LINEAR, 
                    int WrapS = GL_CLAMP_TO_EDGE, int WrapT = GL_CLAMP_TO_EDGE, int32 ForceNumChannel = 0);

/// Texture Utilities
void            BindTexture2D(uint32 TextureID, uint32 TextureUnit);
void            BindTexture3D(uint32 TextureID, uint32 TextureUnit);
void            BindCubemap(uint32 TextureID, uint32 TextureUnit);
uint32          Make2DTexture(void *ImageBuffer, uint32 Width, uint32 Height, uint32 Channels, bool IsFloat,
                    bool FloatHalfPrecision, real32 AnisotropicLevel, int MagFilter, int MinFilter, int WrapS, int WrapT);
uint32          Make3DTexture(uint32 Width, uint32 Height, uint32 Depth, uint32 Channels, bool IsFloat, bool FloatHalfPrecision,
                    int MagFilter, int MinFilter, int WrapS, int WrapT, int WrapR);
uint32          MakeCubemap(context *Context, path *Paths, bool IsFloat, bool FloatHalfPrecision, uint32 Width, uint32 Height, bool MakeMipmap);
void            ComputeIrradianceCubemap(context *Context, char const *HDREnvmapFilename,
                    uint32 *HDRCubemapEnvmap, uint32 *HDRGlossyEnvmap, uint32 *HDRIrradianceEnvmap);
uint32          PrecomputeGGXLUT(context *Context, uint32 Width);

/// Mesh Utilities
uint32          MakeVertexArrayObject();
uint32          AddIBO(uint32 Usage, size_t Size, void const *Data);
uint32          AddEmptyVBO(size_t Size, uint32 Usage);
void            FillVBO(uint32 Attrib, uint32 AttribStride, uint32 Type, size_t ByteOffset, size_t Size, void const *Data);
void            UpdateVBO(uint32 VBO, size_t ByteOffset, size_t Size, void *Data);
void            DestroyMesh(mesh *Mesh);
void            RenderMesh(mesh *Mesh, uint32 GLDrawType = GL_TRIANGLES);

mesh            MakeUnitCube(bool MakeAdditionalAttribs = true);
mesh            Make2DQuad(context *Context, vec2f Start, vec2f End, int Subdivisions = 0);
mesh			Make2DCircle(context *Context, vec2f Center, real32 Radius, int Segments = 32);
mesh            Make3DPlane(context *Context, vec2i Dimension, uint32 Subdivisions, uint32 TextureRepeatCount, bool Dynamic = false);
mesh            MakeUnitSphere(bool MakeAdditionalAttribs = true, real32 TexScale = 1.f);

/// Uniform Buffer Utilities
uint32			MakeUBO(size_t Size, GLenum DrawType = GL_STATIC_DRAW);
void			BindUBO(uint32 ID, int Target);
void			FillUBO(size_t Offset, size_t Size, void const *Data);
void			DestroyUBO(uint32 ID);

/// Model Utilities
bool            ModelLoadGLTF(context *Context, model *Model, path const Filename, int AnisotropicLevel);
void			ModelFree(model *Model);

/// Shader Utilities
uint32          BuildShader(context *Context, char *VSPath, char *FSPath, char *GSPath = NULL, char *TESCPath = NULL, char *TESEPath = NULL);
uint32          BuildShaderFromSource(context *Context, char const *VSrc, char const *FSrc, char const *GSrc = NULL,
									  char const *TESCSrc = NULL, char const *TESESrc = NULL);
void            SendVec2(uint32 Loc, vec2f value);
void            SendVec3(uint32 Loc, vec3f value);
void            SendVec4(uint32 Loc, vec4f value);
void			SendMat3(uint32 Loc, mat3f value);
void            SendMat4(uint32 Loc, mat4f value);
void            SendInt(uint32 Loc, int value);
void            SendFloat(uint32 Loc, real32 value);

/// Framebuffer Utilities
frame_buffer    MakeFramebuffer(uint32 NumAttachments, vec2i Size, bool AddDepthBuffer = true);
void            DestroyFramebuffer(frame_buffer *FB);
void            FramebufferAttachBuffer(frame_buffer *FB, uint32 Attachment, uint32 Channels, bool IsFloat, bool FloatHalfPrecision, bool Mipmap);
void            FramebufferAttachBuffer(frame_buffer *FB, uint32 Attachment, uint32 TextureID);
void            FramebufferSetAttachmentCount(frame_buffer *FB, int Count);

/// Display Text Utilities
real32          GetDisplayTextWidth(char const *Text, font *Font, real32 Scale);
void            FillDisplayTextInterleaved(char const *Text, int32 TextLength, font *Font, vec3i Pos, int MaxPixelWidth,
                    real32 *VertData, uint16 *Indices, real32 Scale = 1.0f);
void            FillDisplayTextInterleavedUTF8(char const *Text , int32 TextLength, font *Font, vec3i Pos, int MaxPixelWidth,
                    real32 *VertData, uint16 *IdxData, real32 Scale = 1.0f);
}
#endif
