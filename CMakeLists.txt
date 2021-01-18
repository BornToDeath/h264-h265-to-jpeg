cmake_minimum_required(VERSION 3.17)
project(H265ToJpeg)

set(CMAKE_CXX_STANDARD 11)

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

# 编为成可执行文件
add_executable(H265ToJpeg main.cpp ${DIR_SRCS})
target_link_libraries(
        H265ToJpeg
        avcodec
        avdevice
        avfilter
        avformat
        avutil
        swresample
        swscale
#        ffmpeg
)