//
// Created by lixiaoqing on 2021/4/27.
//

#include "com_autonavi_socol_occtiltedserver_service_H265DecodeService.h"
#include "H265ToJpeg.h"


JNIEXPORT jboolean JNICALL Java_com_autonavi_socol_occtiltedserver_service_H265DecodeService_decode
        (JNIEnv *env, jclass clazz, jstring inputPath, jstring outputPath) {

    const char *input = env->GetStringUTFChars(inputPath, NULL);
    const char *output = env->GetStringUTFChars(outputPath, NULL);

    int res = H265ToJpeg(input, output);
    return res == 0;
}