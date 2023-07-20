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

set( bbx_includes "")
set( bbx_sources "")
list(APPEND bbx_sources ${BBX_COMMON} )
list(APPEND bbx_sources ${BBX_COMMONH} )
list(APPEND bbx_includes "src")
list(APPEND bbx_sources ${BBX_FREETYPE} )
list(APPEND bbx_sources ${BBX_FREETYPEH} )
list(APPEND bbx_includes "src/freetype" )

if(CMAKE_SYSTEM_NAME MATCHES Linux)
        list(APPEND libs m)
endif()

source_group("freetype" FILES ${BBX_FREETYPE} )
source_group("freetype" FILES ${BBX_FREETYPEH} ) 

add_executable( "babyxrc" ${bbx_sources} ${bbx_headers} )



