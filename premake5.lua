workspace "rf"
    configurations { "Debug", "Release" }
    language "C++"
    cppdialect "C++11"
    architecture "x86_64"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        optimize "On"

    filter "platforms:Windows"
        system "windows"

    filter "platforms:Unix"
        system "linux"

    filter {}

    
project "glfw3"
    kind "StaticLib"
    includedirs { "ext/glfw/include" }
    files { "ext/glfw/src/omesa_context.*", "ext/glfw/src/vulkan.c", 
            "ext/glfw/src/internal.h", "ext/glfw/src/context.c", "ext/glfw/src/init.c", "ext/glfw/src/input.c",
            "ext/glfw/src/monitor.c", "ext/glfw/src/window.c" }

    -- This filter thing doesnt work on unix it seems...
    --filter { "platforms:Windows" }
        --defines { "_GLFW_WIN32", "_CRT_SECURE_NO_WARNINGS" }
        --files { "ext/glfw/src/wgl_context.*", "ext/glfw/src/win32_**", "ext/glfw/src/egl_context.*" }
    --filter { "system:unix" }
        defines { "_GLFW_X11" }
        files { "ext/glfw/src/x11_**", "ext/glfw/src/glx**" }

project "rf"
    kind "StaticLib"
    targetdir "lib/"
    includedirs { "include/rf", "ext", "ext/glew/include", "ext/cjson" }
    files { "src/**.cpp", "ext/glew/src/glew.c", "ext/stb.cpp", "ext/cjson/cJSON.c", "include/rf/**.h" }
    defines { "GLEW_STATIC", "_CRT_SECURE_NO_WARNINGS" }
    links { "glfw3" }

