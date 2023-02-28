cmake_minimum_required(VERSION 3.23)
set(NAME "Buffout4")
set(VERSION 1.31.1)

project(
	${NAME}
	VERSION ${VERSION}
	LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

include(cmake/common.cmake)

add_subdirectory(src)
include(cmake/packaging.cmake)
