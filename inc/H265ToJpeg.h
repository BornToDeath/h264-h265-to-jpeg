//
// Created by lixiaoqing on 2021/1/13.
//

#ifndef H265TOJPEG_H265TOJPEG_H
#define H265TOJPEG_H265TOJPEG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * H265 帧转 Jpeg
 * @param inputFilePath  输入的 H265 文件路径
 * @param outputFilePath 输出的 Jpeg 文件路径
 * @return 0: 成功
 *        -1: 失败
 */
int H265ToJpeg(const char * const inputFilePath, const char * const outputFilePath);

#ifdef __cplusplus
}
#endif

#endif //H265TOJPEG_H265TOJPEG_H
