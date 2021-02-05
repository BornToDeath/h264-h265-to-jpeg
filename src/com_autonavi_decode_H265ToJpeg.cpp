//
// Created by lixiaoqing on 2021/2/3.
//

#include <iostream>
#include "com_autonavi_decode_H265ToJpeg.h"
#include "H265ToJpeg.h"

JNIEXPORT jboolean JNICALL Java_com_autonavi_decode_H265ToJpeg_decode
        (JNIEnv *env, jclass clazz, jstring inputPath, jstring outputPath) {

    const char *input = env->GetStringUTFChars(inputPath, NULL);
    const char *output = env->GetStringUTFChars(outputPath, NULL);

    int res = H265ToJpeg(input, output);
    return res == 0;
}