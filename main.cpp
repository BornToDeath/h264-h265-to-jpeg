#include <iostream>

#include "H265ToJpeg.h"

int main() {
    std::cout << "--- 开始解码 ---" << std::endl;

//    const char *inputFilePath = "/Users/lixiaoqing/Desktop/H265ToJpeg/H265ToJpeg/imgs/img02.h265";
    const char *inputFilePath = "/Users/lixiaoqing/Desktop/socol/H264ToJpeg/test/test09.h264";
    std::cout << "源文件：" << inputFilePath << std::endl;

//    const char *outputFilePath = "/Users/lixiaoqing/Desktop/H265ToJpeg/H265ToJpeg/imgs/output.jpeg";
    const char *outputFilePath = "/Users/lixiaoqing/Desktop/socol/H264ToJpeg/test/test09.h264.jpeg";
    std::cout << "目标文件：" << outputFilePath << std::endl;

    /**
     * H265 转换为 Jpeg
     */

    int isOk = H265ToJpeg(inputFilePath, outputFilePath);
    if (isOk == 0) {
        std::cout << "解码成功！" << std::endl;
    } else {
        std::cout << "解码失败！" << std::endl;
    }

    std::cout << "--- 解码结束 ---" << std::endl;

    return 0;
}
