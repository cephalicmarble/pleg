project(pleg)

set(TARGET pleg)
add_compile_options(-g -W -Wall -Wextra -pedantic)
cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 14)
find_package(PkgConfig REQUIRED)
set(PKGCONFIG "/usr/bin/pkg-config")

execute_process(COMMAND ${PKGCONFIG} gstreamer-1.0 --cflags OUTPUT_VARIABLE GST_CFLAGS)
add_compile_options(${GST_CFLAGS})
execute_process(COMMAND ${PKGCONFIG} gstreamer-1.0 --libs OUTPUT_VARIABLE GST_LFLAGS)
SEPARATE_ARGUMENTS(GST_LFLAGS)
foreach(FLAG IN LISTS GST_FLAGS)
	target_link_libraries(${TARGET} ${FLAG})
endforeach()
target_link_libraries(${TARGET} "-lunwind")

FILE(STRINGS "pleg.files" PLEG_FILES)
add_executable(${TARGET} main.cpp)
foreach(FILE ${PLEG_FILES})
	target_sources(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/${FILE})
	set(SOURCE_FILES "${SOURCE_FILES} ${CMAKE_SOURCE_DIR}/${FILE}")
endforeach()

FILE(STRINGS "pleg.includes" INCLUDE_DIRS)

foreach(DIR ${INCLUDE_DIRS})
	include_directories(${DIR})
endforeach()

find_package(Boost REQUIRED COMPONENTS system chrono date_time log program_options regex)
include_directories(${Boost_INCLUDE_DIR})
target_link_libraries(${TARGET} ${Boost_LIBRARIES})
