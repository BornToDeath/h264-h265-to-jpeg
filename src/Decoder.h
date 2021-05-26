//
// Created by lixiaoqing on 2021/5/21.
//

#ifndef H265TOJPEG_DECODER_H
#define H265TOJPEG_DECODER_H


#ifdef __cplusplus
extern "C" {
#endif
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#ifdef __cplusplus
}
#endif

#include <iostream>
#include <memory>
#include "IDecoder.h"


/**
 * 解码器
 */
class Decoder : public IDecoder {
public:
    Decoder();

    ~Decoder() override;

    Decoder(const Decoder &obj) = delete;

    Decoder &operator=(const Decoder &obj) = delete;

    /**
     * H265 帧转 Jpeg
     * @param inputFilePath  输入的 H265 文件路径
     * @param outputFilePath 输出的 Jpeg 文件路径
     * @return 0: 成功
     *        -1: 失败
     */
    bool H265ToJpeg(const char *inputFilePath, const char *outputFilePath) override;

private:

    /**
     * 释放资源
     */
    void release();

private:
    AVFormatContext *fmtCtx; /* ffmpeg 的全局上下文，所有 ffmpeg 都需要 */
    AVCodecContext *codecCtx;    /* ffmpeg 编解码上下文 */
    AVFrame *frame;          /* ffmpeg 单帧缓存 */
    AVPacket *packet;        /* ffmpeg 单帧数据包 */
};

#endif  // H265TOJPEG_DECODER_H