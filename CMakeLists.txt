cmake_minimum_required(VERSION 3.16)
project(H265ToJpeg)

set(CMAKE_CXX_STANDARD 11)

# 是否是 debug 环境
set(DEBUG YES)
set(CMAKE_CXX_FLAGS "-fPIC")

if(DEBUG)
    message(">>> 当前环境: DEBUG")
    ADD_DEFINITIONS(-D_BUILD_TYPE_DEBUG_)
else()
    message(">>> 当前环境: RELEASE")
endif()

# 头文件目录
include_directories(
        export_inc/
        src/
        src/jni/
        src/ffmpeg/x86_64_shared
)

# 链接库目录
link_directories(
#        lib/ffmpeg/x86_64_shared
        lib/ffmpeg/mac_static
)

# 源文件
aux_source_directory(src DIR_SRCS)
aux_source_directory(src/jni DIR_SRCS)

#message("DIR_SRCS=${DIR_SRCS}")

# 编译成库文件
add_library(H265ToJpeg SHARED ${DIR_SRCS})

## 修改库文件的前缀，去掉 "lib"
#set_target_properties(H265ToJpeg PROPERTIES PREFIX "")
#
## 修改库文件的名称
#set_target_properties(H265ToJpeg PROPERTIES OUTPUT_NAME "lixiaoqing")

# 链接库
target_link_libraries(H265ToJpeg
        avdevice
        avfilter
        avformat
        avutil
        swresample
        swscale
        avcodec
)

if(DEBUG)
    # 编译成可执行文件
    add_executable(runH265ToJpeg main.cpp)

    # 链接库
    target_link_libraries(runH265ToJpeg
            H265ToJpeg
    )
endif()