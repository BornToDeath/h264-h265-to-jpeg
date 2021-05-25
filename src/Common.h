//
// Created by lixiaoqing on 2021/5/21.
//

#ifndef H265TOJPEG_COMMON_H
#define H265TOJPEG_COMMON_H

/* 是否是 debug 环境 */
#ifdef _BUILD_TYPE_DEBUG_
#define DEBUG 1
#else
#define DEBUG 0
#endif

/* 堆缓冲大小（单位：Byte） */
#define HEAP_SIZE (1024 * 1024)  // 1MB

/* 栈缓冲大小（单位：Byte） */
#define STACK_SIZE (1024)  // 1KB


extern void LOG(const char *format, ...);

/**
 * H265 数据的结构体
 */
class Input {
public:
    Input() = default;

    ~Input() {
        LOG("%s", __PRETTY_FUNCTION__);
        if (h265_data) {
            free(h265_data);
            h265_data = nullptr;
        }
        offset = 0;
        size = 0;
    }

public:
    char *h265_data;
    int offset;
    int size;
};


/**
 * Jpeg 数据的结构体
 */
class Output {
public:
    Output() = default;

    ~Output() {
        LOG("%s", __PRETTY_FUNCTION__);
        if (jpeg_data) {
            free(jpeg_data);
            jpeg_data = nullptr;
        }
        offset = 0;
    }

public:
    char *jpeg_data;
    int offset;
};

#endif //H265TOJPEG_COMMON_H
