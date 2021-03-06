#include "context.h"
#include "utils.h"

#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

namespace rf {
static int GetAttribIndex(const char *AttribName)
{
    if(!strcmp(AttribName, "POSITION")) return 0;
    if(!strcmp(AttribName, "TEXCOORD_0")) return 1;
    if(!strcmp(AttribName, "NORMAL")) return 2;
    if(!strcmp(AttribName, "TANGENT")) return 3;
    else
    {
        printf("Undefined GLTF attrib name %s\n", AttribName); 
        return -1;
    }
}

static int GetAttribStride(int AccessorType) {
    switch(AccessorType)
    {
        case TINYGLTF_TYPE_VEC2: return 2;
        case TINYGLTF_TYPE_VEC3: return 3;
        case TINYGLTF_TYPE_VEC4: return 4;
        case TINYGLTF_TYPE_MAT2: return 4;
        case TINYGLTF_TYPE_MAT3: return 9;
        case TINYGLTF_TYPE_MAT4: return 16;
        default: printf("Wrong GLTF TYPE!\n"); return -1;
    }
}

static size_t GetComponentSize(int ComponentType)
{
    switch(ComponentType)
    {
        case GL_BYTE :
        case GL_UNSIGNED_BYTE:
            return sizeof(int8);
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return sizeof(int16);
        case GL_INT:
        case GL_UNSIGNED_INT:
            return sizeof(int32);
        case GL_FLOAT:
            return sizeof(real32);
        case GL_DOUBLE:
            return sizeof(real64);
        default:
            printf("Undefined GL Component %d\n", ComponentType);
            return 0;

    }
}

bool ModelLoadGLTF(context *Context, model *Model, path const Filename, int AnisotropicLevel)
{
    tinygltf::Model Mdl;
    tinygltf::TinyGLTF Loader;

    path Filepath;
    ConcatStrings(Filepath, ctx::GetExePath(Context), Filename);
    
    using namespace tinygltf;

    stbi_set_flip_vertically_on_load(0);
	bool Ret;
	{
		std::string LoadErr;
		Ret = Loader.LoadASCIIFromFile(&Mdl, &LoadErr, Filepath);

		if (!LoadErr.empty())
		{
			printf("Error loading glTF model %s : %s\n", Filepath, LoadErr.c_str());
			return false;
		}
	}

    if(!Ret)
    {
        printf("Failed to parse glTF model %s\n", Filepath);
        return false;
    }

    if(Mdl.buffers.size() > 1)
    {
        printf("UNIMPLEMENTED : glTF model with several buffers %s (%zu)\n", Filepath, Mdl.buffers.size());
        return false;
    }

	Model->Meshes = rf::Buf<mesh>(Context->SessionPool, Mdl.meshes.size());
	Model->MaterialIdx = rf::Buf<int>(Context->SessionPool, Mdl.meshes.size());
	Model->Materials = rf::Buf<material>(Context->SessionPool, Mdl.materials.size());

    // Load Textures 
    // TODO - Load all textures first and register in a resource manager. Query it afterwards during material loading

    render_resources *RenderResources = &Context->RenderResources;

    // Load Material
    for(auto const &SrcMtl : Mdl.materials)
    {
        //material &DstMtl = Model->Material[iter++];
		material DstMtl;

        if(SrcMtl.values.size() == 0)
        { // NOTE - Default Magenta color for error
            DstMtl.AlbedoTexture = *RenderResources->DefaultDiffuseTexture;
            DstMtl.RoughnessMetallicTexture = *RenderResources->DefaultDiffuseTexture;
            DstMtl.NormalTexture = *RenderResources->DefaultNormalTexture;
            DstMtl.AlbedoMult = vec3f(1,0,1); // Magenta error color
            DstMtl.RoughnessMult = 1.f;
            DstMtl.MetallicMult = 0.f;
        }
        else
        {
            auto DiffuseTexIdx = SrcMtl.values.find("baseColorTexture");
            if(DiffuseTexIdx != SrcMtl.values.end())
            {
                int TextureIndex = (int)DiffuseTexIdx->second.json_double_value.at("index");
                const Image &img = Mdl.images[Mdl.textures[TextureIndex].source];
                const Sampler &spl = Mdl.samplers[Mdl.textures[TextureIndex].sampler];
                DstMtl.AlbedoTexture = Make2DTexture((void*)&img.image[0], (uint32)img.width, (uint32)img.height, (uint32)img.component, 
                        false, false, (real32)AnisotropicLevel,
                        spl.magFilter, spl.minFilter, spl.wrapS, spl.wrapT);
            }
            else
            { // NOTE - No diffuse texture : put in the Default
                DstMtl.AlbedoTexture = *RenderResources->DefaultDiffuseTexture;
            }

            auto RoughnessTexIdx = SrcMtl.values.find("metallicRoughnessTexture");
            if(RoughnessTexIdx != SrcMtl.values.end())
            {
                int TextureIndex = (int)RoughnessTexIdx->second.json_double_value.at("index");
                const Image &img = Mdl.images[Mdl.textures[TextureIndex].source];
                const Sampler &spl = Mdl.samplers[Mdl.textures[TextureIndex].sampler];
                DstMtl.RoughnessMetallicTexture = Make2DTexture((void*)&img.image[0], (uint32)img.width, (uint32)img.height, (uint32)img.component, 
                        false, false, (real32)AnisotropicLevel,
                        spl.magFilter, spl.minFilter, spl.wrapS, spl.wrapT);
            }
            else
            { // NOTE - No diffuse texture : put in the Default
                DstMtl.RoughnessMetallicTexture = *RenderResources->DefaultDiffuseTexture;
            }

            auto NormalTexIdx = SrcMtl.values.find("normalTexture");
            if(NormalTexIdx != SrcMtl.values.end())
            {
                int TextureIndex = (int)NormalTexIdx->second.json_double_value.at("index");
                const Image &img = Mdl.images[Mdl.textures[TextureIndex].source];
                const Sampler &spl = Mdl.samplers[Mdl.textures[TextureIndex].sampler];
                DstMtl.NormalTexture = Make2DTexture((void*)&img.image[0], img.width, img.height, img.component,
                        false, false, (real32)AnisotropicLevel, spl.magFilter, spl.minFilter, spl.wrapS, spl.wrapT);
            }
            else
            {
                DstMtl.NormalTexture = *RenderResources->DefaultNormalTexture;
            }

            auto EmissiveTexIdx = SrcMtl.values.find("emissiveTexture");
            if(EmissiveTexIdx != SrcMtl.values.end())
            {
                int TextureIndex = (int)EmissiveTexIdx->second.json_double_value.at("index");
                const Image &img = Mdl.images[Mdl.textures[TextureIndex].source];
                const Sampler &spl = Mdl.samplers[Mdl.textures[TextureIndex].sampler];
                DstMtl.EmissiveTexture = Make2DTexture((void*)&img.image[0], img.width, img.height, img.component,
                        false, false, (real32)AnisotropicLevel, spl.magFilter, spl.minFilter, spl.wrapS, spl.wrapT);
            }
            else
            {
                DstMtl.EmissiveTexture = *RenderResources->DefaultEmissiveTexture;
            }

            auto AlbedoMultIdx = SrcMtl.values.find("albedoFactor");
            if(AlbedoMultIdx != SrcMtl.values.end())
            {
                DstMtl.AlbedoMult = vec3f((real32)AlbedoMultIdx->second.number_array[0],
                                          (real32)AlbedoMultIdx->second.number_array[1],
                                          (real32)AlbedoMultIdx->second.number_array[2]);
            }
            else
            {
                DstMtl.AlbedoMult = vec3f(1.f);
            }

            auto RoughnessMultIdx = SrcMtl.values.find("roughnessFactor");
            if(RoughnessMultIdx != SrcMtl.values.end())
            {
                DstMtl.RoughnessMult = (real32)RoughnessMultIdx->second.number_array[0];
            }
            else
            {
                DstMtl.RoughnessMult = 1.f;
            }

            auto MetallicMultIdx = SrcMtl.values.find("metallicFactor");
            if(MetallicMultIdx != SrcMtl.values.end())
            {
                DstMtl.MetallicMult = (real32)MetallicMultIdx->second.number_array[0];
            }
            else
            {
                DstMtl.MetallicMult = 1.f;
            }

            auto EmissiveMultIdx = SrcMtl.values.find("emissiveFactor");
            if(EmissiveMultIdx != SrcMtl.values.end())
            {
                DstMtl.EmissiveMult = vec3f((real32)EmissiveMultIdx->second.number_array[0],
                                            (real32)EmissiveMultIdx->second.number_array[1],
                                            (real32)EmissiveMultIdx->second.number_array[2]);
            }
            else
            {
                DstMtl.EmissiveMult = vec3f(1.f);
            }
        }
		rf::BufPush(Model->Materials, DstMtl);
    }

    // Load Mesh
    const Buffer &DataBuffer = Mdl.buffers[0];

    for(auto const &SrcMesh : Mdl.meshes)
    {
        if(SrcMesh.primitives.size() > 1)
        {
            printf("UNIMPLEMENTED : glTF model with several primitives %s (%zu)\n", Filepath, SrcMesh.primitives.size());
            return false;
        }

        const Primitive &prim = SrcMesh.primitives[0];
        const Accessor &indices =  Mdl.accessors[prim.indices];
        const BufferView &indicesBV = Mdl.bufferViews[indices.bufferView];

		mesh DstMesh;
		rf::BufPush(Model->MaterialIdx, prim.material);

        DstMesh.IndexCount = (uint32)indices.count;
        DstMesh.IndexType = (uint32)indices.componentType;
        DstMesh.VAO = MakeVertexArrayObject();

        // compute size of index buffer
        size_t IdxBufferSize = indices.count * GetComponentSize(indices.componentType);
        DstMesh.VBO[0] = AddIBO(GL_STATIC_DRAW, IdxBufferSize, &DataBuffer.data[0] + indices.byteOffset + indicesBV.byteOffset);

        // compute size of buffer first by iterating over all attributes, and validate the mesh
        bool ValidMesh[4] = { false, false, false, false };
        size_t VCount = 0;
        size_t BufferSize = 0;
        for(auto const &it : prim.attributes)
        {
            const Accessor &acc = Mdl.accessors[it.second];
            const BufferView & bv = Mdl.bufferViews[acc.bufferView];

            int AttribIdx = GetAttribIndex(it.first.c_str());
            int AttribStride = GetAttribStride(acc.type);

            if(AttribIdx >= 0 && AttribIdx <= 3)
            {
                ValidMesh[AttribIdx] = true;

                if(AttribIdx == 0)
                {
                    VCount = acc.count;
                }
            }

            BufferSize += acc.count * GetComponentSize(acc.componentType) * AttribStride;
        }

        if(!ValidMesh[0])
        {
            printf("Error loading glTF model %s : Positions are not given.\n", Filepath);
            return false;
        }
        if(!ValidMesh[1])
        {
            printf("Error loading glTF model %s : Texcoords are not given.\n", Filepath);
            return false;
        }
        if(!ValidMesh[2])
        {
            printf("Error loading glTF model %s : Normals are not given.\n", Filepath);
            return false;
        }
        if(!ValidMesh[3])
        { // Add buffer space for the tangents and bitangents
            BufferSize += VCount * sizeof(vec4f);
        }

        DstMesh.VBO[1] = AddEmptyVBO(BufferSize, GL_STATIC_DRAW);

        int Attrib = 0;
        size_t AttribOffset = 0;

        for(auto &it : prim.attributes)
        {
            const Accessor &acc = Mdl.accessors[it.second];
            const BufferView & bv = Mdl.bufferViews[acc.bufferView];

            int AttribIdx = GetAttribIndex(it.first.c_str());
            int AttribStride = GetAttribStride(acc.type);
            size_t AttribSize = acc.count * GetComponentSize(acc.componentType) * AttribStride;

            //printf("%s : Idx %d, Stride %d, Size %d\n", it.first.c_str(), AttribIdx, AttribStride, bv.byteLength);

            if(AttribStride < 0 || AttribIdx < 0)
            {
                printf("Error loading glTF model %s : Attrib %d stride or size invalid.\n", Filepath, Attrib);
                return false;
            }

            if(AttribIdx >= 0 && AttribIdx <= 3)
            {
                FillVBO(AttribIdx, AttribStride, acc.componentType, AttribOffset, AttribSize, 
                        &DataBuffer.data[0] + acc.byteOffset + bv.byteOffset);
                AttribOffset += AttribSize;

                if(AttribIdx == 2 && !ValidMesh[3])
                { // We have normals but no tangents, programatically compute them
                    printf("No tangents, generating..\n");
                    real32 *NormalPtr = (real32*)(&DataBuffer.data[0] + acc.byteOffset + bv.byteOffset);

                    // Generate tangents ourselves
                    size_t TangentSize = sizeof(vec4f) * acc.count;
                    vec4f *Tangent = (vec4f*)alloca(TangentSize);
                    vec3f Dummy;

                    for(uint32 i = 0; i < VCount; ++i)
                    {
                        vec3f *N = (vec3f*)(NormalPtr + i * 3);
                        printf("%f %f %f\n", N->x, N->y, N->z);
                        BasisFrisvad(*N, *((vec3f*)&Tangent[i]), Dummy);
                        Tangent[i].w = 1.f;
                    }
                    FillVBO(3, 4, GL_FLOAT, AttribOffset, TangentSize, Tangent);
                    AttribOffset += TangentSize;
                }
            }

            Attrib++;
        }
		rf::BufPush(Model->Meshes, DstMesh);
    }

    // Retrieve mesh relative positions
    for(auto const &SrcNode : Mdl.nodes)
    {
        int DstMeshIdx = SrcNode.mesh;
        if(DstMeshIdx < 0) continue;
        // TODO - handle children group

        mesh &DstMesh = Model->Meshes[DstMeshIdx];
        DstMesh.ModelMatrix.Identity();

        vec3f Translation(0), Rotation(0), Scale(1);

        if(SrcNode.rotation.size())
        {
            // TODO - The engine should use quaternions in the future, instead of Euler angles.
            quaternion Q = { (real32)SrcNode.rotation[0], (real32)SrcNode.rotation[1], (real32)SrcNode.rotation[2], (real32)SrcNode.rotation[3] };
            Rotation = QuaternionToEulerAngle(Q);
        }
        if(SrcNode.translation.size())
        {
            Translation = vec3f((real32) SrcNode.translation[0], (real32) SrcNode.translation[1], (real32) SrcNode.translation[2]);
        }
        if(SrcNode.scale.size())
        {
            Scale = vec3f((real32) SrcNode.scale[0], (real32) SrcNode.scale[1], (real32) SrcNode.scale[2]);
        }

        if(SrcNode.matrix.size())
        {
            std::vector<double> const &sm = SrcNode.matrix;
            DstMesh.ModelMatrix = mat4f((real32)sm[0],  (real32)sm[1],  (real32)sm[2],  (real32)sm[3],
                                        (real32)sm[4],  (real32)sm[5],  (real32)sm[6],  (real32)sm[7],
                                        (real32)sm[8],  (real32)sm[9],  (real32)sm[10], (real32)sm[11],
                                        (real32)sm[12], (real32)sm[13], (real32)sm[14], (real32)sm[15]);
        }
        else
        {
            DstMesh.ModelMatrix.FromTRS(Translation, Rotation, Scale);
        }
    }


    glBindVertexArray(0);

    return true;
}

void ModelFree(model *Model)
{
	Assert(Model);

	for (int i = 0; i < BufSize(Model->Meshes); ++i)
	{
		rf::DestroyMesh(&Model->Meshes[i]);
	}
	rf::BufFree(Model->Materials);
	rf::BufFree(Model->MaterialIdx);
	rf::BufFree(Model->Meshes);
}
}
