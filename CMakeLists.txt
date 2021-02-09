cmake_minimum_required (VERSION 3.5 FATAL_ERROR)

cmake_policy (SET CMP0054 NEW)

include (ExternalProject)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

project (fii-ipmi-oem CXX)

add_definitions (-DBOOST_ERROR_CODE_HEADER_ONLY)
add_definitions (-DBOOST_SYSTEM_NO_DEPRECATED)
add_definitions (-DBOOST_ALL_NO_LIB)
add_definitions (-DBOOST_NO_RTTI)
add_definitions (-DBOOST_NO_TYPEID)
add_definitions (-DBOOST_ASIO_DISABLE_THREADS)
add_definitions (-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)
add_definitions (-Wno-psabi)

find_package (PkgConfig REQUIRED)
pkg_check_modules (LOGGING phosphor-logging REQUIRED)
include_directories (${LOGGING_INCLUDE_DIRS})
link_directories (${LOGGING_LIBRARY_DIRS})

pkg_check_modules (LIBIPMID libipmid REQUIRED)
include_directories (SYSTEM ${LIBIPMID_INCLUDE_DIRS})
link_directories (${LIBIPMID_LIBRARY_DIRS})

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library (fiioemcmd
             SHARED src/systemcommands.cpp src/networkcommands.cpp
             src/fanctrlcommands.cpp src/cpldcommands.cpp
             src/buscommands.cpp src/filefwcommands.cpp
             src/misccommands.cpp src/bioscommands.cpp
	     src/utils.cpp)

set_target_properties (fiioemcmd PROPERTIES VERSION "0.1.0")
set_target_properties (fiioemcmd PROPERTIES SOVERSION "0")
target_link_libraries (fiioemcmd sdbusplus)
target_link_libraries (fiioemcmd ipmid)
target_link_libraries (fiioemcmd -luserlayer)
target_link_libraries (fiioemcmd -lchannellayer)
target_link_libraries (fiioemcmd ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries (fiioemcmd phosphor_logging)

#set (PACKAGE_DIR /usr/share/lcd-debug/)
#set (CONFIG_FILES post_desc.json gpio_desc.json cri_sensors.json)

install (TARGETS fiioemcmd DESTINATION lib/ipmid-providers)
#install (FILES ${CONFIG_FILES} DESTINATION ${PACKAGE_DIR})
