workspace "rf"
    configurations { "Debug", "Release", "ReleaseDbg" }
    platforms { "Windows", "Unix" }
    language "C++"
    cppdialect "C++11"
    architecture "x86_64"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        optimize "On"

    filter "configurations:ReleaseDbg"
        optimize "On"
        buildoptions { "-fno-omit-frame-pointer" }

    filter "platforms:Windows"
        system "windows"

    filter "platforms:Unix"
        system "linux"

    filter {}

    
project "glfw3"
    kind "StaticLib"
    includedirs { "ext/glfw/include" }
    targetdir "lib/"

    filter "configurations:Debug"
        targetname "glfw3_d"

    filter "configurations:ReleaseDbg"
        targetname "glfw3_p"

    filter "configurations:Release"
        targetname "glfw3"

    filter {}

    files { "ext/glfw/src/osmesa_context.*", "ext/glfw/src/vulkan.c", "ext/glfw/src/egl_context.*",
            "ext/glfw/src/internal.h", "ext/glfw/src/context.c", "ext/glfw/src/init.c", "ext/glfw/src/input.c",
            "ext/glfw/src/monitor.c", "ext/glfw/src/window.c" }

    filter { "platforms:Windows" }
        defines { "_GLFW_WIN32", "_CRT_SECURE_NO_WARNINGS" }
        files { "ext/glfw/src/wgl_context.*", "ext/glfw/src/win32_**", "ext/glfw/src/egl_context.*" }
    filter { "platforms:Unix" }
        defines { "_GLFW_X11" }
        files { "ext/glfw/src/x11_**", "ext/glfw/src/glx**", "ext/glfw/src/linux_joystick.c", "ext/glfw/src/posix*", 
                "ext/glfw/src/xkb_unicode.c" }

    filter {}

project "rf"
    kind "StaticLib"
    targetdir "lib/"

    filter "configurations:Debug"
        targetname "rf_d"

    filter "configurations:ReleaseDbg"
        targetname "rf_p"

    filter "configurations:Release"
        targetname "rf"

    filter {}

    includedirs { "include/rf", "ext", "ext/glew/include", "ext/cjson", "ext/glfw/include" }
    files { "src/**.cpp", "ext/glew/src/glew.c", "ext/stb.cpp", "ext/cjson/cJSON.c", "include/rf/**.h" }
    defines { "GLEW_STATIC", "_CRT_SECURE_NO_WARNINGS" }
    links { "glfw3" }

