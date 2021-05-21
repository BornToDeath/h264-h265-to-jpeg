//
// Created by lixiaoqing on 2021/5/21.
//

#ifndef H265TOJPEG_ENCODER_H
#define H265TOJPEG_ENCODER_H

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavdevice/avdevice.h"
}

#include "Common.h"


/**
 * 编码器
 */
class Encoder {

public:

    Encoder();

    ~Encoder();

    /**
     * 将 yuv 编码为 Jpeg
     * @param pFrame     YUV 帧数据
     * @param outputData 输出的 Jpeg 数据的结构体
     * @return
     */
    bool yuv2Jpeg(AVFrame *pFrame, Output *outputData);

private:

    /**
     * 释放资源
     */
    void release();

private:

    AVIOContext *pIOCtx;            /* ffmpeg 字节流 IO 上下文 */
    AVFormatContext *pFormatCtx;    /* ffmpeg 的全局上下文，所有 ffmpeg 都需要 */
    AVCodecContext *pCodeCtx;       /* ffmpeg 编解码上下文 */
    AVCodec *pCodec;                /* ffmpeg 编解码器 */
    AVCodecParameters *pCodecPars;  /* ffmpeg 编解码器参数 */
    AVStream *pStream;              /* ffmpeg 视频流 */
    AVPacket *packet;               /* ffmpeg 单帧数据包 */
    unsigned char *ioBuf;           /* IO Buffer */

};

#endif //H265TOJPEG_ENCODER_H
