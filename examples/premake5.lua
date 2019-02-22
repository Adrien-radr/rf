-- Copy ext/rf/premake5.lua script as rf.lua to be able to include it
os.copyfile("ext/rf/premake5.lua", "ext/rf/rf.lua")

require "ext/rf"

workspace "rf_example"
    language "C++"
    cppdialect "C++11"
    architecture "x86_64"

    platforms { "Windows", "Unix" }
    configurations { "Debug", "Release", "ReleaseDbg" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        optimize "On"

    filter "configurations:ReleaseDbg"
        optimize "Debug"
        buildoptions { "-fno-omit-frame-pointer" }

    filter "platforms:Windows"
        buildoptions { "-W4" }

    filter "platforms:Unix"
        buildoptions { "-Wall" }

    filter {}

externalproject "glfw3"
    kind "StaticLib"
    location "ext/rf"
    targetdir "ext/rf/lib"

externalproject "rf"
    kind "StaticLib"    
    location "ext/rf"
    targetdir "ext/rf/lib"
    includedirs { "ext/rf/ext/glfw/include" }

project "rf_example"
    kind "ConsoleApp"
    targetdir "bin/"
    debugdir "bin/"
    defines { "GLEW_STATIC" }
    defines { "HAVE_SSE2=1" }

    files { "src/**.cpp", "src/**.h" }
    includedirs { "src", "ext/rf/include", "ext/rf/ext/cjson",
                  "ext/rf/ext/glew/include", "ext/rf/ext/glfw/include" }

    libdirs { "ext/rf/lib" }

    filter "configurations:Debug"
        links { "rf_d", "glfw3_d" }

    filter "configurations:ReleaseDbg"
        links { "rf_p", "glfw3_p" }

    filter { "configurations:Release" }
        links { "rf", "glfw3" }

    filter "platforms:Windows"
        links { "opengl32", "PowrProf" }

    filter "platforms:Unix"
        links { "GL", "X11", "dl", "pthread" }

    filter {}