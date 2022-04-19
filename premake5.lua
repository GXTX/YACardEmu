workspace "YACardEmu"
	configurations { "Debug", "Release" }
	platforms { "x86" }

	systemversion "latest"

	buildoptions { "/std:c++17" }

externalproject "libserialport"
	location "3rdparty/libserialport"
	uuid "1C8EAAF2-133E-4CEE-8981-4A903A8B3935"
	kind "SharedLib"
	language "C"

project "YACardEmu"
	kind "ConsoleApp"
	language "C++"
	targetdir "bin/%{cfg.buildcfg}"
	
	files { "main.cpp", "Includes/*.cpp", "Includes/*.h" }
	
	includedirs { "Includes/", "3rdParty/spdlog/include/", "3rdParty/mINI/src/", "3rdParty/cpp-httplib/", "3rdParty/libserialport/"}

	links { "libserialport" }

	postbuildcommands {
	  "if exist $(TargetDir)..\\..\\config.ini.sample copy /y $(TargetDir)..\\..\\config.ini.sample $(TargetDir)config.ini",	  
	  "if exist $(TargetDir)..\\..\\%{cfg.buildcfg}\\libserialport.dll copy /y $(TargetDir)..\\..\\%{cfg.buildcfg}\\libserialport.dll $(TargetDir)libserialport.dll", 
	  "if exist $(TargetDir)..\\..\\site xcopy /y $(TargetDir)..\\..\\site $(TargetDir)site\\"
	}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
