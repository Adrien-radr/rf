# Radar Rendering Foundation
GLFW/GL4 rendering framework and general useful utilities

- File Operations (Disk Copy, Filepath finding, File reading...)
- Logger utility with different log levels and timestamping
- Linear algebra single header math library (vec2, vec3, vec4, mat3, mat4, general utils)
- Windowing utils : context creation, input handling, window events
- Rendering utils
    - Resource handling and storing (textures, images, fonts)
    - Font rendering (stb_truetype)
    - Texture loading and creation (2D, 3D, Cubemap, Irradiance Prefiltering)
    - Mesh handling (VAO, VBO, GLTF mesh loading)
    - Frambuffer utils (GBuffer, auxilliary fbos)
    - 2D Display text rendering
- User Interface
    - Implementation of an immediate-mode UI (à la imgui)
    - Panel with mouse move and resize
    - Border and titlebar
    - Button
    - Slider
    - Text
    - 2D image display
    - Progressbar

The library is initialized by ctx::Init() and demands 2 memory arenas in input to make its internal allocations with.
One Scratch memory arena for temporary allocs (file buffers, temp mesh vertex data, ...). Those are considered in-place
and ephemeral and the memory pool this data is using can be zero'ed every frame if necessary.
One Session memory arena for more permanent storage (image/texture data, UI internal state, the context itself, ...).
This arena should not be free'd for the program's whole lifetime.


