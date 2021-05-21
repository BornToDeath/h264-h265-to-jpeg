//
// Created by lixiaoqing on 2021/4/27.
//

#include "com_autonavi_socol_occtiltedserver_service_H265DecodeService.h"
#include "IDecoder.h"


JNIEXPORT jboolean JNICALL Java_com_autonavi_socol_occtiltedserver_service_H265DecodeService_decode
        (JNIEnv *env, jclass clazz, jstring inputPath, jstring outputPath) {

    const char *input = env->GetStringUTFChars(inputPath, NULL);
    const char *output = env->GetStringUTFChars(outputPath, NULL);

    // H265 转 Jpeg
    bool isOk = IDecoder::getInstance()->H265ToJpeg(input, output);
    return isOk;
}