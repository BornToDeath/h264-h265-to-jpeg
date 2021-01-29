cmake_minimum_required(VERSION 3.17)
project(H265ToJpeg)

set(CMAKE_CXX_STANDARD 11)

# 是否是 debug 环境
set(DEBUG YES)

if(DEBUG)
    ADD_DEFINITIONS(-D_BUILD_TYPE_DEBUG_)
endif()

# 头文件目录
include_directories(
        inc
)

# 链接库目录
link_directories(
        lib
)

# 源文件
aux_source_directory(src DIR_SRCS)

# 编译成静态库
add_library(H265ToJpeg SHARED ${DIR_SRCS})

# 编为成可执行文件
#add_executable(H265ToJpeg main.cpp ${DIR_SRCS})
#add_executable(H265ToJpeg main.cpp)

# 链接库
target_link_libraries(
        H265ToJpeg
        avcodec
        avdevice
        avfilter
        avformat
        avutil
        swresample
        swscale
)