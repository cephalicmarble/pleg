project(pleg)

set(TARGET pleg)
add_compile_options(-g -W -Wall -Wextra -pedantic -Wno-literal-suffix -DBOOST_ASIO_ENABLE_HANDLER_TRACKING)
cmake_minimum_required(VERSION 3.6)
add_executable(${TARGET} src/main.cpp)
#target_compile_definitions(${TARGET} PRIVATE -DDEBUGHEAP)

set(CMAKE_CXX_STANDARD 14)
find_package(PkgConfig REQUIRED)
set(PKGCONFIG "/usr/bin/pkg-config")

execute_process(COMMAND ${PKGCONFIG} gstreamer-1.0 --cflags OUTPUT_VARIABLE GST_CFLAGS)
separate_arguments(GST_CFLAGS)
foreach(FLAG IN LISTS GST_CFLAGS)
	STRING(STRIP ${FLAG} FLAG)
	if(FLAG MATCHES -I.*)
		STRING(REPLACE "-I" "" DIR ${FLAG})
		include_directories(${DIR})
	else(FLAG MATCHES -I.*)
		add_compile_options(${TARGET} PRIVATE ${FLAG})
	endif(FLAG MATCHES -I.*)
endforeach()

execute_process(COMMAND ${PKGCONFIG} gstreamer-1.0 --libs OUTPUT_VARIABLE GST_LFLAGS)
separate_arguments(GST_LFLAGS)
foreach(FLAG IN LISTS GST_LFLAGS)
	STRING(STRIP ${FLAG} FLAG)
	target_link_libraries(${TARGET} ${FLAG})
endforeach()
#target_link_libraries(${TARGET} "-lunwind -lpthread -lgstbase-0.10 -lgstapp-0.10 -lgstpbutils-0.10 -lgstriff-0.10 -lpng")
target_link_libraries(${TARGET} "-lunwind -lpthread -lgstbase-1.0 -lgstapp-1.0 -lgstpbutils-1.0 -lgstriff-1.0 -lpng")

FILE(STRINGS "pleg.files" PLEG_FILES)
foreach(FILE ${PLEG_FILES})
	message(${FILE})
	target_sources(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/${FILE})
	set(SOURCE_FILES "${SOURCE_FILES} ${CMAKE_SOURCE_DIR}/${FILE}")
endforeach()

FILE(STRINGS "pleg.includes" INCLUDE_DIRS)

foreach(DIR ${INCLUDE_DIRS})
	if(NOT DIR MATCHES .*gstreamer.*)
		include_directories(${DIR})
	endif(NOT DIR MATCHES .*gstreamer.*)
endforeach()

find_package(Boost REQUIRED COMPONENTS system chrono date_time filesystem log program_options regex)
include_directories(${Boost_INCLUDE_DIR})
target_link_libraries(${TARGET} ${Boost_LIBRARIES})
