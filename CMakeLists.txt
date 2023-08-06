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


