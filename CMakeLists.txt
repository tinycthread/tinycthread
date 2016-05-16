cmake_minimum_required(VERSION 3.1)

project(TinyCThread C)

set(LIBRARY_NAME tinycthread)

## Define COMPILER_OPTIONS_FORMAT to match compiler we are using
if (CMAKE_COMPILER_IS_GNUCC OR ${CMAKE_C_COMPILER_ID} STREQUAL Clang)
	set(COMPILER_OPTIONS_FORMAT "GNU")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL MSVC)
	set(COMPILER_OPTIONS_FORMAT "MSVC")
endif ()

## Enable extra warnings and make warnings errors
if (${COMPILER_OPTIONS_FORMAT} STREQUAL GNU)
	add_compile_options(-Wall -Wextra -Werror -pedantic)
elseif (${COMPILER_OPTIONS_FORMAT} STREQUAL MSVC)
	add_compile_options(/W4 /WX /EHsc)
endif ()

include_directories(source)

set(SOURCE_FILES source/tinycthread.c source/tinycthread.h)

add_library(${LIBRARY_NAME} STATIC ${SOURCE_FILES})
set_target_properties(${LIBRARY_NAME} PROPERTIES C_STANDARD 11)

install(TARGETS	${LIBRARY_NAME}	ARCHIVE DESTINATION lib)
install(FILES source/tinycthread.h DESTINATION include)

