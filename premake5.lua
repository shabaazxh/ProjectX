workspace "ProjectX"
	language "C++"
	cppdialect "C++20"

	platforms { "x64" }
	configurations { "debug", "release" }

	flags "NoPCH"
	flags "MultiProcessorCompile"

	startproject "ProjectX"

	debugdir "%{wks.location}"
	objdir "_build_/%{cfg.buildcfg}-%{cfg.platform}-%{cfg.toolset}"
	targetsuffix "-%{cfg.buildcfg}-%{cfg.platform}-%{cfg.toolset}"
	
	-- Default toolset options
	filter "toolset:gcc or toolset:clang"
		linkoptions { "-pthread" }
		buildoptions { "-march=native", "-Wall", "-pthread" }

	filter "toolset:msc-*"
		defines { "_CRT_SECURE_NO_WARNINGS=1" }
		defines { "_SCL_SECURE_NO_WARNINGS=1" }
		buildoptions { "/utf-8" }
	
	filter "*"

	-- default options for GLSLC
	glslcOptions = "-O --target-env=vulkan1.2"

	-- default libraries
	filter "system:linux"
		links "dl"
	
	filter "system:windows"

	filter "*"

	-- default outputs
	filter "kind:StaticLib"
		targetdir "lib/"

	filter "kind:ConsoleApp"
		targetdir "bin/"
		targetextension ".exe"
	
	filter "*"

	--configurations
	filter "debug"
		symbols "On"
		defines { "_DEBUG=1" }

	filter "release"
		optimize "On"
		defines { "NDEBUG=1" }

	filter "*"

-- Third party dependencies
include "third_party" 

-- GLSLC helpers
dofile( "util/glslc.lua" )

-- Projects






project "ProjectX"
	local sources = { 
		"ProjectX/**.cpp",
		"ProjectX/**.hpp",
		"ProjectX/**.hxx",
		"third_party/imgui/*.cpp",
		"third_party/imgui/*.h"
	}

	kind "ConsoleApp"
	location "ProjectX"

	files( sources )

	dependson "ProjectX-shaders"

	links "x-volk"
	links "x-stb"
	links "x-glfw"
	links "x-vma"
	links "x-imgui"

	dependson "x-glm"

project "ProjectX-shaders"
	local shaders = { 
		"ProjectX/shaders/*.vert",
		"ProjectX/shaders/*.frag",
		"ProjectX/shaders/*.comp",
		"ProjectX/shaders/*.geom",
		"ProjectX/shaders/*.tesc",
		"ProjectX/shaders/*.tese"
	}

	kind "Utility"
	location "ProjectX/shaders"

	files( shaders )

	handle_glsl_files( glslcOptions, "assets/shaders", {} )

project()

--EOF
