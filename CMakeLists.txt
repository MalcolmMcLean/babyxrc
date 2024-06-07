## #################################################################
## Root CMakeLists.txt
## #################################################################

cmake_minimum_required(VERSION 3.0)

project(babyxrc)

if(CMAKE_SYSTEM_NAME MATCHES Windows)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Build the Baby X resource compiler

file( GLOB BBX_COMMON src/*.c )
file( GLOB BBX_COMMONH src/*.h )
file( GLOB BBX_FREETYPE src/freetype/*.c )
file( GLOB BBX_FREETYPEH src/freetype/*.h )
file( GLOB BBX_SAMPLERATE src/samplerate/*.c )
file( GLOB BBX_SAMPLERATEH src/samplerate/*.h )

set( bbx_includes "")
set( bbx_sources "")
list(APPEND bbx_sources ${BBX_COMMON} )
list(APPEND bbx_sources ${BBX_COMMONH} )
list(APPEND bbx_includes "src")
list(APPEND bbx_sources ${BBX_FREETYPE} )
list(APPEND bbx_sources ${BBX_FREETYPEH} )
list(APPEND bbx_sources ${BBX_SAMPLERATE} )
list(APPEND bbx_sources ${BBX_SAMPLERATEH} )

list(APPEND bbx_includes "src/freetype" )
list(APPEND bbx_includes "src/samplerate" )  

set( libs "")
if(CMAKE_SYSTEM_NAME MATCHES Linux)
        list(APPEND libs m)
endif()

source_group("freetype" FILES ${BBX_FREETYPE} )
source_group("freetype" FILES ${BBX_FREETYPEH} ) 
source_group("samplerate" FILES ${BBX_SAMPLERATE} )
source_group("samplerate" FILES ${BBX_SAMPLERATEH} )

add_executable( "babyxrc" ${bbx_sources} ${bbx_headers} )
target_link_libraries( "babyxrc" ${libs} )

# Baby X file system programs

file( GLOB BBX_SHELL babyxfs_src/shell/*.c )
file( GLOB BBX_SHELLH babyxfs_src/shell/*.h )
set( bbx_shell_includes "")
set( bbx_shell_sources "")
list(APPEND bbx_shell_includes "babyxfs_src")
list(APPEND bbx_shell_sources ${BBX_SHELL} )
list(APPEND bbx_shell_sources ${BBX_SHELLH} )

list(APPEND bbx_shell_includes "babyxfs_src" )

add_executable("testbabyxfilesystem"
    "babyxfs_src/bbx_write_source_archive.c"
    "babyxfs_src/bbx_write_source_archive.h"
    "babyxfs_src/bbx_filesystem.c"
    "babyxfs_src/bbx_filesystem.h"
    "babyxfs_src/xmlparser2.c"
    "babyxfs_src/xmlparser2.h"
   
    "babyxfs_src/testbabyxfilesystem.c")
target_link_libraries( "testbabyxfilesystem" ${libs} )

add_executable("rot13"
    "babyxfs_src/rot13.c")
target_link_libraries( "rot13" ${libs} )

add_executable("bbx_ls"
    "babyxfs_src/strnatcmp.c"
    "babyxfs_src/strnatcmp.h"
    "babyxfs_src/readdirectory_posix.c"
    "babyxfs_src/bbx_options.c"
    "babyxfs_src/bbx_options.h"
    "babyxfs_src/bbx_ls.c")
target_link_libraries( "rot13" ${libs} )


add_executable("babyxfs_escapexml"
    "babyxfs_src/babyxfs_escapexml.c")
target_link_libraries( "babyxfs_escapexml" ${libs} )


add_executable("babyxfs_extract"
    "babyxfs_src/xmlparser2.c"
    "babyxfs_src/xmlparser2.h"
    "babyxfs_src/bbx_filesystem.c"
    "babyxfs_src/bbx_filesystem.h"
    "babyxfs_src/bbx_write_source_archive.c"
    "babyxfs_src/bbx_write_source_archive.h"
    "babyxfs_src/babyxfs_extract.c")
target_link_libraries( "babyxfs_extract" ${libs} )

add_executable("babyxfs_ls"
    "babyxfs_src/xmlparser2.c"
    "babyxfs_src/xmlparser2.h"
    "babyxfs_src/bbx_write_source_archive.c"
    "babyxfs_src/bbx_write_source_archive.h"
    "babyxfs_src/bbx_filesystem.c"
    "babyxfs_src/bbx_filesystem.h"
    "babyxfs_src/bbx_options.c"
    "babyxfs_src/bbx_options.h"
    "babyxfs_src/babyxfs_ls.c")
target_link_libraries( "babyxfs_ls" ${libs} )

add_executable("babyxfs_rm"
    "babyxfs_src/xmlparser2.c"
    "babyxfs_src/xmlparser2.h"
    "babyxfs_src/bbx_write_source_archive.c"
    "babyxfs_src/bbx_write_source_archive.h"
    "babyxfs_src/bbx_filesystem.c"
    "babyxfs_src/bbx_filesystem.h"
    "babyxfs_src/babyxfs_rm.c")
target_link_libraries( "babyxfs_rm" ${libs} )

add_executable("babyxfs_cp"
    "babyxfs_src/xmlparser2.c"
    "babyxfs_src/xmlparser2.h"
    "babyxfs_src/bbx_write_source_archive.c"
    "babyxfs_src/bbx_write_source_archive.h"
    "babyxfs_src/bbx_filesystem.c"
    "babyxfs_src/bbx_filesystem.h"
    "babyxfs_src/babyxfs_cp.c")
target_link_libraries( "babyxfs_cp" ${libs} )

add_executable("babyxfs_shell"
    "babyxfs_src/xmlparser2.c"
    "babyxfs_src/xmlparser2.h"
    "babyxfs_src/bbx_write_source_archive.c"
    "babyxfs_src/bbx_write_source_archive.h"
    "babyxfs_src/bbx_filesystem.c"
    "babyxfs_src/bbx_filesystem.h"
    "babyxfs_src/bbx_options.c"
    "babyxfs_src/bbx_options.h"
    ${bbx_shell_sources})
    target_include_directories(babyxfs_shell PRIVATE ${bbx_shell_includes})
target_link_libraries( "babyxfs_shell" ${libs} )

add_executable("babyxfs_test"
    "babyxfs_src/xmlparser2.c"
    "babyxfs_src/xmlparser2.h"
    "babyxfs_src/bbx_write_source_archive.c"
    "babyxfs_src/bbx_write_source_archive.h"
    "babyxfs_src/babyxfs_test.c")
target_link_libraries( "babyxfs_test" ${libs} )

add_executable("babyxfs_dirtoxml"
    "babyxfs_src/babyxfs_dirtoxml.c")
target_link_libraries( "babyxfs_dirtoxml" ${libs} )

add_executable("babyxfs_xmltodir"
 "babyxfs_src/asciitostring.c"
 "babyxfs_src/asciitostring.h"
 "babyxfs_src/xmlparser2.c"
 "babyxfs_src/xmlparser2.h"
 "babyxfs_src/bbx_write_source.c"
 "babyxfs_src/bbx_write_source.h"
 "babyxfs_src/babyxfs_xmltodir.c")
target_link_libraries( "babyxfs_xmltodir" ${libs} )


