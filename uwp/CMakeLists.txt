cmake_minimum_required(VERSION 3.25)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(AssetDeploy)
include(FetchContent)

# Setup UWP platform
set(CMAKE_SYSTEM_NAME WindowsStore)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Config for main build
set(DUSK_DIR CACHE PATH "")
set(DUSK_UWP_TOOLSET "" CACHE STRING "MSVC platform toolset to use for UWP build (e.g. v143, v145). Leave empty to auto-detect from DUSK_DIR/CMakeCache.txt")

# Match UWP toolset with the main Dusk build when possible to avoid ABI/link issues.
if(MSVC AND NOT CMAKE_GENERATOR_TOOLSET)
	if(NOT DUSK_UWP_TOOLSET AND DUSK_DIR AND EXISTS "${DUSK_DIR}/CMakeCache.txt")
		file(STRINGS "${DUSK_DIR}/CMakeCache.txt" _dusk_toolset_line REGEX "^CMAKE_VS_PLATFORM_TOOLSET:STRING=")
		if(_dusk_toolset_line)
			string(REGEX REPLACE "^CMAKE_VS_PLATFORM_TOOLSET:STRING=(.+)$" "\\1" DUSK_UWP_TOOLSET "${_dusk_toolset_line}")
		endif()
	endif()
	if(DUSK_UWP_TOOLSET)
		set(CMAKE_GENERATOR_TOOLSET "${DUSK_UWP_TOOLSET}" CACHE STRING "Platform Toolset" FORCE)
		message(STATUS "Using MSVC toolset ${CMAKE_GENERATOR_TOOLSET} for UWP build")
	endif()
endif()

if(DUSK_DIR AND EXISTS "${DUSK_DIR}/CMakeCache.txt")
	file(STRINGS "${DUSK_DIR}/CMakeCache.txt" _dusk_crt_line REGEX "^CMAKE_MSVC_RUNTIME_LIBRARY:")
	if(_dusk_crt_line)
		list(GET _dusk_crt_line 0 _dusk_crt_line0)
		if(_dusk_crt_line0 MATCHES "^CMAKE_MSVC_RUNTIME_LIBRARY:[A-Za-z0-9_]+=(.+)$")
			set(_dusk_rt "${CMAKE_MATCH_1}")
			if(NOT _dusk_rt MATCHES "DLL")
				message(FATAL_ERROR
					"DUSK_DIR must be built with /MD (DLL) CRT for UWP; found CMAKE_MSVC_RUNTIME_LIBRARY=${_dusk_rt}. "
					"Configure the main Dusk tree with preset windows-msvc-relwithdebinfo-uwp and set DUSK_DIR to that build directory.")
			endif()
		endif()
	endif()
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)
message(STATUS "MSVC runtime library (UWP): ${CMAKE_MSVC_RUNTIME_LIBRARY}")

# Fetch shared UWP deps
FetchContent_Declare(UwpLibs
    GIT_REPOSITORY https://github.com/worleydl/uwp-dep.git
    GIT_TAG 33914f994ab6a3663a24279bd7a76c190eb4b4e4
)
FetchContent_MakeAvailable(UwpLibs)


# Define project
project(dusk-uwp LANGUAGES CXX)
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT ${PROJECT_NAME})

if(NOT DUSK_DIR)
	message(FATAL_ERROR "DUSK_DIR must point to the Dusk build folder containting the main static lib")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmake/UwpAbseilLibs.cmake)

add_executable(${PROJECT_NAME} WIN32
	main.cpp
)

set(BinDeps
	${CMAKE_BINARY_DIR}/_deps/uwplibs-src/x64/bin/dxcompiler.dll
	${CMAKE_BINARY_DIR}/_deps/uwplibs-src/x64/bin/dxil.dll
	${CMAKE_BINARY_DIR}/_deps/uwplibs-src/x64/bin/libuwp.dll
	${CMAKE_BINARY_DIR}/_deps/uwplibs-src/x64/bin/nod.dll
	${CMAKE_BINARY_DIR}/_deps/uwplibs-src/x64/bin/SDL3.dll
	${CMAKE_BINARY_DIR}/_deps/uwplibs-src/x64/bin/webgpu_dawn.dll

	Package.appxmanifest
)
source_group("BinDeps" FILES ${BinDeps})
set_source_files_properties(${BinDeps} PROPERTIES
	VS_COPY_TO_OUT_DIR Always
	VS_DEPLOYMENT_CONTENT TRUE
	VS_DEPLOYMENT_LOCATION "."
)

set(PkgAssets
	Assets/LockScreenLogo.scale-200.png
	Assets/Square44x44Logo.targetsize-24_altform-unplated.png
	Assets/Square44x44Logo.scale-200.png
	Assets/Square150x150Logo.scale-200.png
	Assets/StoreLogo.png
)
source_group("PkgAssets" FILES ${PkgAssets})
set_source_files_properties(${PkgAssets} PROPERTIES
	#VS_COPY_TO_OUT_DIR Always
	VS_DEPLOYMENT_CONTENT TRUE
	VS_DEPLOYMENT_LOCATION "Assets"
)

target_sources(${PROJECT_NAME} PRIVATE
	${BinDeps}
	${PkgAssets}
)

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_BINARY_DIR}/_deps/uwplibs-src/x64/include
)

set(BinLibs
	${CMAKE_BINARY_DIR}/_deps/uwplibs-src/x64/lib/libuwp.lib
	${CMAKE_BINARY_DIR}/_deps/uwplibs-src/x64/lib/nod.lib
	${CMAKE_BINARY_DIR}/_deps/uwplibs-src/x64/lib/SDL3.lib
	${CMAKE_BINARY_DIR}/_deps/uwplibs-src/x64/lib/webgpu_dawn.lib

	${DUSK_DIR}/extern/aurora/aurora_card.lib
	${DUSK_DIR}/extern/aurora/aurora_core.lib
	${DUSK_DIR}/extern/aurora/aurora_dvd.lib
	${DUSK_DIR}/extern/aurora/aurora_gd.lib
	${DUSK_DIR}/extern/aurora/aurora_gx.lib
	#${DUSK_DIR}/extern/aurora/aurora_main.lib
	${DUSK_DIR}/extern/aurora/aurora_mtx.lib
	${DUSK_DIR}/extern/aurora/aurora_os.lib
	${DUSK_DIR}/extern/aurora/aurora_pad.lib
	${DUSK_DIR}/extern/aurora/aurora_si.lib
	${DUSK_DIR}/extern/aurora/aurora_vi.lib
	${DUSK_DIR}/extern/aurora/extern/imgui.lib	

	${DUSK_DIR}/extern/aurora/extern/imgui_backends.lib	
	${DUSK_DIR}/extern/aurora/extern/rmlui_backends.lib	
	${DUSK_DIR}/extern/aurora/extern/sqlite3.lib	
	${DUSK_DIR}/rmlui.lib
	${DUSK_DIR}/rmlui_debugger.lib
	${DUSK_DIR}/libs/freeverb/freeverb.lib

	# must swap these out for release
	debug ${DUSK_DIR}/_deps/fmt-build/fmtd.lib
	debug ${DUSK_DIR}/_deps/freetype-build/freetyped.lib
	optimized ${DUSK_DIR}/_deps/fmt-build/fmt.lib
	optimized ${DUSK_DIR}/_deps/freetype-build/freetype.lib


	${DUSK_DIR}/_deps/xxhash-build/xxhash.lib
	${DUSK_DIR}/_deps/zstd-build/lib/zstd_static.lib

	${DUSK_ABSEIL_LIBS}

	${DUSK_DIR}/libjpeg-turbo-install/lib/jpeg-static.lib
	${DUSK_DIR}/libjpeg-turbo-install/lib/turbojpeg-static.lib # MT conflict

	${DUSK_DIR}/JSystem_J2DGraph.lib
	${DUSK_DIR}/JSystem_J3DGraphAnimator.lib
	${DUSK_DIR}/JSystem_J3DGraphBase.lib
	${DUSK_DIR}/JSystem_J3DGraphLoader.lib
	${DUSK_DIR}/JSystem_J3DU.lib
	${DUSK_DIR}/JSystem_JAHostIO.lib
	${DUSK_DIR}/JSystem_JAudio2.lib
	${DUSK_DIR}/JSystem_JFramework.lib
	${DUSK_DIR}/JSystem_JGadget.lib
	${DUSK_DIR}/JSystem_JHostIO.lib
	${DUSK_DIR}/JSystem_JKernel.lib
	${DUSK_DIR}/JSystem_JMath.lib
	${DUSK_DIR}/JSystem_JMessage.lib
	${DUSK_DIR}/JSystem_JParticle.lib
	${DUSK_DIR}/JSystem_JStage.lib
	${DUSK_DIR}/JSystem_JStudio.lib
	${DUSK_DIR}/JSystem_JStudio_JAudio2.lib
	${DUSK_DIR}/JSystem_JStudio_JParticle.lib
	${DUSK_DIR}/JSystem_JStudio_JStage.lib
	${DUSK_DIR}/JSystem_JSupport.lib
	${DUSK_DIR}/JSystem_JUtility.lib

	${DUSK_DIR}/dusk.lib	

	WindowsApp.lib
	OneCore.lib
	ntdll.lib
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${BinLibs})

if(MSVC)
	target_link_options(${PROJECT_NAME} PRIVATE
		/NODEFAULTLIB:libcmt
		/NODEFAULTLIB:libcmtd
	)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE AURORA_WINDOWS_STORE)

# Data deployment
uwp_pkg(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/../res "res")
