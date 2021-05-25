#include <iostream>

#include "IDecoder.h"


int main() {
    std::cout << "--- 开始解码 ---" << std::endl;

    const char *inputFilePath = "/Users/lixiaoqing/Desktop/socol/H265ToJpeg/H265ToJpeg/test/img/img01.h265";
//    const char *inputFilePath = "/home/lxq271332/H265ToJpeg/test/img/img01.h265";
    std::cout << "源文件：" << inputFilePath << std::endl;

    const char *outputFilePath = "/Users/lixiaoqing/Desktop/socol/H265ToJpeg/H265ToJpeg/test/img/img01.h265.jpeg";
//    const char *outputFilePath = "/home/lxq271332/H265ToJpeg/test/img/img01.h265.jpeg";
    std::cout << "目标文件：" << outputFilePath << std::endl;

    for (int i=-0;i<1000000; ++i) {

        std::cout << ">>> i=" << i << std::endl;

        {
            /**
             * H265 转换为 Jpeg
             */

            auto decoder = IDecoder::getInstance();
            if (!decoder) {
                std::cout << "单例创建解码器失败！" << std::endl;
                return -1;
            }
            // 解码
            bool isOk = decoder->H265ToJpeg(inputFilePath, outputFilePath);
            decoder->release();
            if (isOk) {
                std::cout << "解码成功！" << std::endl;
            } else {
                std::cout << "解码失败！" << std::endl;
            }
            std::cout << "--- 解码结束 ---" << std::endl;
        }

    }
    return 0;
}
