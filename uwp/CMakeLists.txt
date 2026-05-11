cmake_minimum_required(VERSION 3.15)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(AssetDeploy)
include(FetchContent)

# Setup UWP platform
set(CMAKE_SYSTEM_NAME WindowsStore)
set(CMAKE_SYSTEM_VERSION 10.0.19041.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# todo: does this still support vs2022?
# > main dusk build seemed to use v145 and 143 throws cryptic errors
# > so currently it requires a manual switch
#set(CMAKE_GENERATOR_TOOLSET "v145" CACHE STRING "Platform Toolset" FORCE)

# Fetch shared UWP deps
FetchContent_Declare(UwpLibs
    GIT_REPOSITORY https://github.com/worleydl/uwp-dep.git
    GIT_TAG 6d4136ce5935990d60488305b41b74af4b1790d8
)
FetchContent_MakeAvailable(UwpLibs)


# Define project
project(dusk-uwp LANGUAGES CXX)
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT ${PROJECT_NAME})

# Config for main recomp build
set(DUSK_DIR CACHE PATH "")
if(NOT DUSK_DIR)
	message(FATAL_ERROR "DUSK_DIR must point to the Dusk build folder containting the main static lib")
endif()

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


	${DUSK_DIR}/_deps/tracy-build/TracyClient.lib
	${DUSK_DIR}/_deps/xxhash-build/xxhash.lib
	${DUSK_DIR}/_deps/zstd-build/lib/zstd_static.lib

	# todo: put this is another file or figure out a cleaner way, dll wrap?
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/base/absl_base.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/base/absl_log_severity.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/base/absl_malloc_internal.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/base/absl_raw_logging_internal.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/base/absl_spinlock_wait.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/base/absl_throw_delegate.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/container/absl_hashtablez_sampler.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/container/absl_raw_hash_set.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/crc/absl_crc32c.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/crc/absl_crc_cord_state.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/crc/absl_crc_cpu_detect.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/crc/absl_crc_internal.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/hash/absl_city.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/hash/absl_hash.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/hash/absl_low_level_hash.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/numeric/absl_int128.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/profiling/absl_exponential_biased.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/strings/absl_cord.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/strings/absl_cordz_functions.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/strings/absl_cordz_handle.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/strings/absl_cordz_info.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/strings/absl_cord_internal.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/strings/absl_strings.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/strings/absl_strings_internal.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/strings/absl_string_view.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/strings/absl_str_format_internal.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/synchronization/absl_graphcycles_internal.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/synchronization/absl_kernel_timeout_internal.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/synchronization/absl_synchronization.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/time/absl_civil_time.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/time/absl_time.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/time/absl_time_zone.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/types/absl_bad_optional_access.lib
	${DUSK_DIR}/_deps/abseil-cpp-build/absl/types/absl_bad_variant_access.lib

	${DUSK_DIR}/libjpeg-turbo-install/lib/jpeg-static.lib
	${DUSK_DIR}/libjpeg-turbo-install/lib/turbojpeg-static.lib # MT conflict
	${DUSK_DIR}/_deps/nod_prebuilt-src/lib/nod.lib

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
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${BinLibs})

# Data deployment
uwp_pkg(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/../res "res")
