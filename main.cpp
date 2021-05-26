#include <chrono>
#include <iostream>
#include <memory>
#include "IDecoder.h"

/**
 * 获取系统时间戳（毫秒级）
 * @return
 */
unsigned long long getCurrentUnixMills() {
    std::chrono::milliseconds ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch());
    return ms.count();
}

/**
 * 获取系统时间戳（秒级）
 * @return
 */
unsigned long long getCurrentUnixSeconds() {
    std::chrono::seconds secs =
            std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch());
    return secs.count();
}

int main() {
//    const char *inputFilePath = "/Users/lixiaoqing/Desktop/socol/H265ToJpeg/H265ToJpeg/test/img/img01.h265";
    const char *inputFilePath = "/home/lxq271332/H265ToJpeg/test/img/img01.h265";
    std::cout << "源文件：" << inputFilePath << std::endl;

//    const char *outputFilePath = "/Users/lixiaoqing/Desktop/socol/H265ToJpeg/H265ToJpeg/test/img/img01.h265.jpeg";
    const char *outputFilePath = "/home/lxq271332/H265ToJpeg/test/img/img01.h265.jpeg";
    std::cout << "目标文件：" << outputFilePath << std::endl;

    for (int i = 0; i < 1000000; ++i) {
        std::cout << ">>> i=" << i << std::endl;

        {
            unsigned long long t1 = getCurrentUnixMills();
            std::cout << "--- 开始解码 ---" << std::endl;

            /**
             * H265 转换为 Jpeg
             */

            auto decoder = IDecoder::getInstance();
            if (!decoder) {
                std::cout << "单例解码器失败！" << std::endl;
                return -1;
            }
            // 解码
            bool isOk = decoder->H265ToJpeg(inputFilePath, outputFilePath);
            if (isOk) {
                std::cout << "解码成功！" << std::endl;
            } else {
                std::cout << "解码失败！" << std::endl;
            }

            std::cout << "--- 解码结束 ---" << std::endl;
            unsigned long long t2 = getCurrentUnixMills();
            std::cout << ">>> 耗时: " << (t2 - t1) << " 毫秒" << std::endl;
        }
    }
    return 0;
}
