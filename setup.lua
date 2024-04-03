outputdir = "%{cfg.buildcfg}-%{cfg.architecture}"

workspace "XScrape"
	architecture "x64"
	configurations {"Debug","Release"}

    filter "system:windows"
      buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }
      systemversion "latest"

    filter "configurations:Debug"
      defines { "_DEBUG" }
      runtime "Debug"
      symbols "On"

    filter "configurations:Release"
      defines { "NDEBUG" }
      runtime "Release"
      optimize "On"
      symbols "Off"

project "XScrape"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("XScrape/bin/" .. outputdir .. "/")
	objdir ("XScrape/bin-int/" .. outputdir .. "/")
	
	defines { "BOOST_ASIO_DISABLE_BOOST_REGEX","BOOST_ASIO_DISABLE_BOOST_DATE_TIME","_CRT_SECURE_NO_WARNINGS","WIN32_LEAN_AND_MEAN","BOOST_NETWORK_ENABLE_HTTPS","BOOST_IOSTREAMS_NO_LIB" }

	files {
		"XScrape/src/**.cpp",
		"XScrape/src/**.h"
	}

	includedirs {
		"XScrape/src/",
		"vendor/OpenSSL/3.0.10/include/",
		"vendor/zlib-1.3/",
		"vendor/boost_1_82_0/boost_1_82_0/"
	}

	libdirs {
		"vendor/OpenSSL/3.0.10/lib/"
	}
	
	links {
		"libssl_static.lib",
		"libcrypto_static.lib",
		"zlibstat.lib"
	}

	filter "configurations:Debug"
		libdirs {
			"vendor/zlib-1.3/contrib/vstudio/vc14/x64/ZlibStatDebug/"
		}

	filter "configurations:Release"
      		libdirs {
			"vendor/zlib-1.3/contrib/vstudio/vc14/x64/ZlibStatRelease/"
		}

project "PolyRun"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"

	targetdir ("PolyRun/bin/" .. outputdir .. "/")
	objdir ("PolyRun/bin-int/" .. outputdir .. "/")

	files {
		"PolyRun/src/**.cpp",
		"PolyRun/src/**.h"
	}

	includedirs {
		"PolyRun/src/"
	}