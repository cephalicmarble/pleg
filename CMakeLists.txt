project(pleg)

set(TARGET pleg)
add_compile_options(-g -W -Wall -Wextra -pedantic -DBOOST_ASIO_ENABLE_HANDLER_TRACKING)
cmake_minimum_required(VERSION 3.6)
add_executable(${TARGET} main.cpp)
target_compile_definitions(${TARGET} PRIVATE -DDEBUGHEAP)

set(CMAKE_CXX_STANDARD 14)
find_package(PkgConfig REQUIRED)
set(PKGCONFIG "/usr/bin/pkg-config")

execute_process(COMMAND ${PKGCONFIG} gstreamer-1.0 --cflags OUTPUT_VARIABLE GST_CFLAGS)
separate_arguments(GST_CFLAGS)
foreach(FLAG IN LISTS GST_CFLAGS)
	STRING(STRIP ${FLAG} FLAG)
	add_compile_options(${TARGET} PRIVATE ${FLAG})
endforeach()

execute_process(COMMAND ${PKGCONFIG} gstreamer-1.0 --libs OUTPUT_VARIABLE GST_LFLAGS)
separate_arguments(GST_LFLAGS)
foreach(FLAG IN LISTS GST_LFLAGS)
	STRING(STRIP ${FLAG} FLAG)
	target_link_libraries(${TARGET} ${FLAG})
endforeach()
target_link_libraries(${TARGET} "-lunwind -lpthread -lgstbase-1.0 -lgstapp-1.0 -lgstpbutils-1.0 -lgstriff-1.0 -lpng")

FILE(STRINGS "pleg.files" PLEG_FILES)
foreach(FILE ${PLEG_FILES})
	target_sources(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/${FILE})
	set(SOURCE_FILES "${SOURCE_FILES} ${CMAKE_SOURCE_DIR}/${FILE}")
endforeach()

FILE(STRINGS "pleg.includes" INCLUDE_DIRS)

foreach(DIR ${INCLUDE_DIRS})
	include_directories(${DIR})
endforeach()

find_package(Boost REQUIRED COMPONENTS system chrono date_time filesystem log program_options regex)
include_directories(${Boost_INCLUDE_DIR})
target_link_libraries(${TARGET} ${Boost_LIBRARIES})
