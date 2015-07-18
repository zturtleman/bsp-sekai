cmake_minimum_required (VERSION 2.8)
project ( "BSP sekai" C )

if (MINGW)
	set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
endif()

#include_directories( "code" )

set( BSP_SRCS
	code/bsp.c
	code/bsp_ef2.c
	code/bsp_fakk.c
	code/bsp_mohaa.c
	code/bsp_q3.c
	code/bsp_q3test106.c
	code/bsp_sof2.c
	code/main.c
	code/md4.c
)

add_executable(bspsekai ${BSP_SRCS})

