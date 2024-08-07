# ---- Options ----

macro(find_commonlib_path)
	if(CommonLibName AND NOT ${CommonLibName} STREQUAL "")
		# Check extern
		find_path(CommonLibPath
			include/REL/Relocation.h
			PATHS ${ROOT_DIR}/external/${CommonLibName}/${CommonLibName}/)

		if(${CommonLibPath} STREQUAL "CommonLibPath-NOTFOUND")
			# Check path
			set_from_environment(${CommonLibName}Path)
			set(CommonLibPath ${${CommonLibName}Path})
		endif()
	endif()
endmacro()

set(CommonLibName "CommonLibF4")
set_root_directory()

find_commonlib_path()
message(
	STATUS
	"Building ${PROJECT_NAME} ${PROJECT_VERSION} for ${FalloutVersion} with ${CommonLibName} at ${CommonLibPath}."
)

set(SOURCE_DIR "${ROOT_DIR}/src")
set(SOURCE_FILES
	"${SOURCE_DIR}/Compatibility/Compatibility.cpp"
	"${SOURCE_DIR}/Compatibility/Compatibility.h"
	"${SOURCE_DIR}/Compatibility/F4EE.cpp"
	"${SOURCE_DIR}/Compatibility/F4EE.h"
	"${SOURCE_DIR}/Crash/Introspection/Introspection.cpp"
	"${SOURCE_DIR}/Crash/Introspection/Introspection.h"
	"${SOURCE_DIR}/Crash/Modules/ModuleHandler.cpp"
	"${SOURCE_DIR}/Crash/Modules/ModuleHandler.h"
	"${SOURCE_DIR}/Crash/PDB/PDBHandler.cpp"
	"${SOURCE_DIR}/Crash/PDB/PDBHandler.h"
	"${SOURCE_DIR}/Crash/CrashHandler.cpp"
	"${SOURCE_DIR}/Crash/CrashHandler.h"
	"${SOURCE_DIR}/Fixes/ActorIsHostileToActorFix.h"
	"${SOURCE_DIR}/Fixes/BackportedBA2Support.h"
	"${SOURCE_DIR}/Fixes/BakaMaxPapyrusOps.h"
	"${SOURCE_DIR}/Fixes/BSLightingShaderMaterialGlowmapFix.h"
	"${SOURCE_DIR}/Fixes/CellInitFix.cpp"
	"${SOURCE_DIR}/Fixes/CellInitFix.h"
	"${SOURCE_DIR}/Fixes/CreateD3DAndSwapChainFix.cpp"
	"${SOURCE_DIR}/Fixes/CreateD3DAndSwapChainFix.h"
	"${SOURCE_DIR}/Fixes/EncounterZoneResetFix.h"
	"${SOURCE_DIR}/Fixes/Fixes.cpp"
	"${SOURCE_DIR}/Fixes/Fixes.h"
	"${SOURCE_DIR}/Fixes/GameDataReady.h"
	"${SOURCE_DIR}/Fixes/GreyMoviesFix.h"
	"${SOURCE_DIR}/Fixes/InteriorNavCutFix.h"
	"${SOURCE_DIR}/Fixes/MagicEffectApplyEventFix.h"
	"${SOURCE_DIR}/Fixes/MovementPlannerFix.h"
	"${SOURCE_DIR}/Fixes/PackageAllocateLocationFix.h"
	"${SOURCE_DIR}/Fixes/PipboyLightInvFix.h"
	"${SOURCE_DIR}/Fixes/SafeExitFix.h"
	"${SOURCE_DIR}/Fixes/TESObjectREFRGetEncounterZoneFix.h"
	"${SOURCE_DIR}/Fixes/UnalignedLoadFix.h"
	"${SOURCE_DIR}/Fixes/UtilityShaderFix.cpp"
	"${SOURCE_DIR}/Fixes/UtilityShaderFix.h"
	"${SOURCE_DIR}/Fixes/WorkBenchSwapFix.h"
	"${SOURCE_DIR}/Patches/AchievementsPatch.cpp"
	"${SOURCE_DIR}/Patches/AchievementsPatch.h"
	"${SOURCE_DIR}/Patches/BSMTAManagerPatch.h"
	"${SOURCE_DIR}/Patches/BSPreCulledObjectsPatch.h"
	"${SOURCE_DIR}/Patches/BSTextureStreamerLocalHeapPatch.h"
	"${SOURCE_DIR}/Patches/HavokMemorySystemPatch.h"
	"${SOURCE_DIR}/Patches/INISettingCollectionPatch.h"
	"${SOURCE_DIR}/Patches/InputSwitchPatch.cpp"
	"${SOURCE_DIR}/Patches/InputSwitchPatch.h"
	"${SOURCE_DIR}/Patches/MaxStdIOPatch.h"
	"${SOURCE_DIR}/Patches/MemoryManagerPatch.cpp"
	"${SOURCE_DIR}/Patches/MemoryManagerPatch.h"
	"${SOURCE_DIR}/Patches/Patches.cpp"
	"${SOURCE_DIR}/Patches/Patches.h"
	"${SOURCE_DIR}/Patches/ScaleformAllocatorPatch.h"
	"${SOURCE_DIR}/Patches/SmallBlockAllocatorPatch.cpp"
	"${SOURCE_DIR}/Patches/SmallBlockAllocatorPatch.h"
	"${SOURCE_DIR}/Patches/WorkshopMenuPatch.h"
	"${SOURCE_DIR}/Warnings/CreateTexture2DWarning.cpp"
	"${SOURCE_DIR}/Warnings/CreateTexture2DWarning.h"
	"${SOURCE_DIR}/Warnings/ImageSpaceAdapterWarning.cpp"
	"${SOURCE_DIR}/Warnings/ImageSpaceAdapterWarning.h"
	"${SOURCE_DIR}/Warnings/Warnings.cpp"
	"${SOURCE_DIR}/Warnings/Warnings.h"
	"${SOURCE_DIR}/Allocator.cpp"
	"${SOURCE_DIR}/Allocator.h"
	"${SOURCE_DIR}/Hash.cpp"
	"${SOURCE_DIR}/Hash.h"
	"${SOURCE_DIR}/main.cpp"
	"${SOURCE_DIR}/PCH.cpp"
	"${SOURCE_DIR}/PCH.h"
	"${SOURCE_DIR}/Settings.h"
)

source_group(TREE "${ROOT_DIR}" FILES ${SOURCE_FILES})

set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/Plugin.h")
configure_file(
	"${ROOT_DIR}/cmake/Plugin.h.in"
	"${VERSION_HEADER}"
	@ONLY
)

source_group("src" FILES "${VERSION_HEADER}")

configure_file(
	"${ROOT_DIR}/cmake/version.rc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	@ONLY
)

add_library(
	"${PROJECT_NAME}"
	SHARED
	${SOURCE_FILES}
	"${VERSION_HEADER}"
	"${CMAKE_CURRENT_BINARY_DIR}/version.rc"
	"${ROOT_DIR}/.clang-format"
	"${ROOT_DIR}/.editorconfig"
)

target_compile_features(
	"${PROJECT_NAME}"
	PRIVATE
	cxx_std_23
)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(
		"${PROJECT_NAME}"
		PRIVATE
		"/EHa" # Exception handling model
		"/sdl" # Enable Additional Security Checks
		"/utf-8" # Set Source and Executable character sets to UTF-8
		"/Zi" # Debug Information Format

		"/permissive-" # Standards conformance
		"/Zc:preprocessor" # Enable preprocessor conformance mode

		"/wd4324" # 'struct_name' : structure was padded due to __declspec(align())

		"$<$<CONFIG:DEBUG>:>"
		"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
	)

	target_link_options(
		"${PROJECT_NAME}"
		PRIVATE
		"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
		"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif()

target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
	"${CMAKE_CURRENT_BINARY_DIR}/src"
	"${SOURCE_DIR}"
)

if(DEFINED CommonLibPath AND NOT ${CommonLibPath} STREQUAL "" AND IS_DIRECTORY ${CommonLibPath})
	add_subdirectory(${CommonLibPath} ${CommonLibName})
else()
	message(
		FATAL_ERROR
		"Variable ${CommonLibName}Path is not set or in external/."
	)
endif()

find_package(Boost
	MODULE
	REQUIRED
	COMPONENTS
	nowide
	stacktrace_windbg
)
find_package(binary_io REQUIRED CONFIG)
find_package(fmt REQUIRED CONFIG)
find_package(frozen REQUIRED CONFIG)
find_package(infoware REQUIRED CONFIG)
find_package(robin_hood REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)
find_package(mmio REQUIRED CONFIG)
find_package(TBB
	REQUIRED
	COMPONENTS
	tbb
	tbbmalloc
	CONFIG
)
find_package(tomlplusplus REQUIRED CONFIG)
find_package(xbyak REQUIRED CONFIG)
find_package(magic_enum CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
find_package(OpenVR REQUIRED)
find_package(zycore CONFIG REQUIRED)
find_package(zydis CONFIG REQUIRED)

# https://github.com/microsoft/DirectXShaderCompiler/blob/main/cmake/modules/FindDiaSDK.cmake
# Find the DIA SDK path.
# It will typically look something like this:
# C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\DIA SDK\include

# CMAKE_GENERATOR_INSTANCE has the location of Visual Studio used
# i.e. C:/Program Files (x86)/Microsoft Visual Studio/2019/Community
set(VS_PATH ${CMAKE_GENERATOR_INSTANCE})
get_filename_component(VS_DIA_INC_PATH "${VS_PATH}/DIA SDK/include" ABSOLUTE CACHE)

# Starting in VS 15.2, vswhere is included.
# Unclear what the right component to search for is, might be Microsoft.VisualStudio.Component.VC.DiagnosticTools
# (although the friendly name of that is C++ profiling tools).  The toolset is the most likely target.
set(PROGRAMFILES_X86 "ProgramFiles(x86)")
execute_process(
	COMMAND "$ENV{${PROGRAMFILES_X86}}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
	OUTPUT_VARIABLE VSWHERE_LATEST
	ERROR_QUIET
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

find_path(DIASDK_INCLUDE_DIR # Set variable DIASDK_INCLUDE_DIR
	dia2.h # Find a path with dia2.h
	HINTS "${VS_DIA_INC_PATH}"
	HINTS "${VSWHERE_LATEST}/DIA SDK/include"
	DOC "path to DIA SDK header files"
)

# VS 2017
if(CMAKE_GENERATOR MATCHES "Visual Studio 15 2017.*")
	if(CMAKE_GENERATOR MATCHES "Visual Studio.*Win64")
		find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib/amd64)
	elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM")
		find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib/arm)
	else(CMAKE_GENERATOR MATCHES "Visual Studio.*Win64")
		find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib)
	endif(CMAKE_GENERATOR MATCHES "Visual Studio.*Win64")
else(CMAKE_GENERATOR MATCHES "Visual Studio 15 2017.*")
	if((CMAKE_GENERATOR_PLATFORM STREQUAL "x64") OR("${DXC_BUILD_ARCH}" STREQUAL "x64"))
		find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib/amd64)
	elseif((CMAKE_GENERATOR_PLATFORM STREQUAL "ARM") OR("${DXC_BUILD_ARCH}" STREQUAL "ARM"))
		find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib/arm)
	elseif((CMAKE_GENERATOR_PLATFORM MATCHES "ARM64.*") OR("${DXC_BUILD_ARCH}" MATCHES "ARM64.*"))
		find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib/arm64)
	else((CMAKE_GENERATOR_PLATFORM STREQUAL "x64") OR("${DXC_BUILD_ARCH}" STREQUAL "x64"))
		find_library(DIASDK_GUIDS_LIBRARY NAMES diaguids.lib HINTS ${DIASDK_INCLUDE_DIR}/../lib)
	endif((CMAKE_GENERATOR_PLATFORM STREQUAL "x64") OR("${DXC_BUILD_ARCH}" STREQUAL "x64"))
endif(CMAKE_GENERATOR MATCHES "Visual Studio 15 2017.*")

set(DIASDK_LIBRARIES ${DIASDK_GUIDS_LIBRARY})
set(DIASDK_INCLUDE_DIRS ${DIASDK_INCLUDE_DIR})
set(DIA_DLL "${DIASDK_INCLUDE_DIRS}/../bin/amd64/msdia140.dll")

include(FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set DIASDK_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(DiaSDK DEFAULT_MSG
	DIASDK_LIBRARIES DIASDK_INCLUDE_DIR)

mark_as_advanced(DIASDK_INCLUDE_DIRS DIASDK_LIBRARIES)

if(DIASDK_LIBRARIES)
	target_include_directories(${PROJECT_NAME}
		PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		${DIASDK_INCLUDE_DIRS}
		${DIASDK_INCLUDE_DIRS}/../lib
	)
	target_link_libraries(${PROJECT_NAME}
		PUBLIC
		${DIASDK_INCLUDE_DIRS}/../lib/amd64/diaguids.lib
	)
endif()

add_compile_definitions(_CRT_SECURE_NO_WARNINGS) # silence warnings for mbstowcs
add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING) # silence warning STL4043: stdext::checked_array_iterator, stdext::unchecked_array_iterator, and related factory functions are non-Standard extensions and will be removed in the future. std::span (since C++20) and gsl::span can be used instead. You can define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING or _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS to suppress this warning.

target_link_libraries(
	"${PROJECT_NAME}"
	PRIVATE
	Boost::headers
	Boost::nowide
	Boost::stacktrace_windbg
	binary_io::binary_io
	Bcrypt.lib
	CommonLibF4::CommonLibF4
	Dbghelp.lib
	fmt::fmt
	frozen::frozen
	infoware
	mmio::mmio
	magic_enum::magic_enum
	robin_hood::robin_hood
	spdlog::spdlog
	TBB::tbb
	TBB::tbbmalloc
	tomlplusplus::tomlplusplus
	xbyak::xbyak
	Zydis::Zydis
)

# target_compile_definitions(
# "${PROJECT_NAME}"
# PRIVATE
# BOOST_STACKTRACE_LINK
# BOOST_STACKTRACE_USE_WINDBG
# )
target_compile_definitions(
	CommonLibF4
	PUBLIC
	F4SE_SUPPORT_XBYAK
	_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)

target_precompile_headers(
	"${PROJECT_NAME}"
	PRIVATE
	"${SOURCE_DIR}/PCH.h"
)

handle_data_files(
	FILES
	"$<TARGET_FILE:${PROJECT_NAME}>"
	"${ROOT_DIR}/data/Data/F4SE/Plugins/Buffout4_preload.txt"
	"${ROOT_DIR}/data/Data/F4SE/Plugins/Buffout4.toml"
	DESTINATION "F4SE/Plugins"
)

install(
	FILES
	"$<TARGET_PDB_FILE:${PROJECT_NAME}>"
	DESTINATION "/"
	COMPONENT "pdbs"
)

set(CONFIG_DIR "${ROOT_DIR}/data/Data/F4SE/Plugins/*")
file(GLOB_RECURSE CONFIG_FILES ${CONFIG_DIR})
message("Found for ${CONFIG_FILES} in ${CONFIG_DIR}")

# Automatic deployment to Mod Organizer 2 mod directory.
foreach(DEPLOY_TARGET $ENV{FalloutPluginTargets})
	message("Adding deployment target ${DEPLOY_TARGET}.")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${DEPLOY_TARGET}/F4SE/Plugins/")

	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "${DEPLOY_TARGET}/F4SE/Plugins/")

	if(DIASDK_LIBRARIES)
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy ${DIA_DLL} "${DEPLOY_TARGET}/F4SE/Plugins/")
	endif()
endforeach()

set(ZIP_DIR "${CMAKE_CURRENT_BINARY_DIR}/zip")
add_custom_target(build-time-make-directory ALL
	COMMAND ${CMAKE_COMMAND} -E make_directory "${ZIP_DIR}"
	"${ZIP_DIR}/F4SE/Plugins/"
)

message("Copying mod into ${ZIP_DIR}.")
file(MAKE_DIRECTORY "${ZIP_DIR}/F4SE/Plugins/")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${ZIP_DIR}/F4SE/Plugins/"
)

if(CONFIG_FILES)
	message("Copying default config files ${CONFIG_FILES} into ${ZIP_DIR}.")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${CONFIG_FILES} "${ZIP_DIR}/F4SE/Plugins/")
endif()

if(DIASDK_LIBRARIES)
	message("Copying ${DIA_DLL} into ${ZIP_DIR}.")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${DIA_DLL} "${ZIP_DIR}/F4SE/Plugins/")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL Debug OR ${CMAKE_BUILD_TYPE} STREQUAL RelWithDebInfo OR ${CMAKE_BUILD_TYPE} STREQUAL Release)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "${ZIP_DIR}/F4SE/Plugins/")
endif()

set(TARGET_ZIP "${PROJECT_NAME}_NG-${PROJECT_VERSION}.7z")
message("Zipping ${ZIP_DIR} to ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_ZIP}.")
ADD_CUSTOM_COMMAND(
	TARGET ${PROJECT_NAME}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_ZIP} --format=7zip -- .
	WORKING_DIRECTORY ${ZIP_DIR}
)