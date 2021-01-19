#include <iostream>

#include "H265ToJpeg.h"

int main() {
    std::cout << "--- 开始转换 ---" << std::endl;

    int isOk = H265ToJpeg();
    if (isOk == 0) {
        std::cout << "转换成功！" << std::endl;
    } else {
        std::cout << "转换失败！" << std::endl;
    }

    std::cout << "--- 转换结束 ---" << std::endl;

    return 0;
}
