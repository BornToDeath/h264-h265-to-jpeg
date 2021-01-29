#include <iostream>

#include "H265ToJpeg.h"

int main() {
    std::cout << "--- 开始解码 ---" << std::endl;

    // H265 转换为 Jpeg
    int isOk = H265ToJpeg();
    if (isOk == 0) {
        std::cout << "解码成功！" << std::endl;
    } else {
        std::cout << "解码失败！" << std::endl;
    }

    std::cout << "--- 解码结束 ---" << std::endl;

    return 0;
}
