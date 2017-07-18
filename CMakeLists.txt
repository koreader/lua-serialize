cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(proxy VERSION 3.0)

add_library(serialize SHARED serialize.c)

include_directories(/usr/local/include/lua5.3 /usr/local/include)

if (APPLE)
	target_link_libraries(serialize lua.5.3)
endif ()

if (UNIX AND NOT APPLE)
	target_link_libraries(serialize lua)
endif ()
