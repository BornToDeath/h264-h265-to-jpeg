//
// Created by lixiaoqing on 2021/4/27.
//

#include "com_autonavi_socol_occtiltedserver_service_H265DecodeService.h"
#include "IDecoder.h"
#include <memory>


JNIEXPORT jboolean JNICALL Java_com_autonavi_socol_occtiltedserver_service_H265DecodeService_decode
        (JNIEnv *env, jclass clazz, jstring inputPath, jstring outputPath) {

    const char *input = env->GetStringUTFChars(inputPath, NULL);
    const char *output = env->GetStringUTFChars(outputPath, NULL);

    /**
     * H265 转 Jpeg
     */

    // 创建解码器示例
    auto decoder = IDecoder::getInstance();
    if (!decoder) {
        return false;
    }

    // 进行解码
    bool isOk = decoder->H265ToJpeg(input, output);

    return isOk;
}