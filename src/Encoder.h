//
// Created by lixiaoqing on 2021/5/21.
//

#ifndef H265TOJPEG_ENCODER_H
#define H265TOJPEG_ENCODER_H


#ifdef __cplusplus
extern "C" {
#endif
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavdevice/avdevice.h"
#ifdef __cplusplus
}
#endif

#include "Common.h"


/**
 * 编码器
 */
class Encoder {

public:

    /**
     * 构造函数
     * @param outputFilePath 输出文件的路径
     */
    explicit Encoder(const char * outputFilePath);

    ~Encoder();

    /**
     * 将 yuv 编码为 Jpeg 并保存
     * @param pFrame YUV 帧数据
     * @return
     */
    bool yuv2Jpeg(AVFrame *pFrame);

private:

    /**
     * 释放资源
     */
    void release();

    /**
     * 将 jpeg 数据保存到文件
     * @param filePath Jpeg 文件路径
     * @return
     */
    bool saveJpegtoFile(const char * filePath);

private:

    const char * outputFilePath;    /* 输出文件的路径 */
    unsigned char *ioBuf;           /* IO Buffer */
    AVIOContext *pIOCtx;            /* ffmpeg 字节流 IO 上下文 */
    AVFormatContext *pFormatCtx;    /* ffmpeg 的全局上下文，所有 ffmpeg 都需要 */
    AVCodecContext *pCodeCtx;       /* ffmpeg 编解码上下文 */
    AVCodec *pCodec;                /* ffmpeg 编解码器 */
    AVCodecParameters *pCodecPars;  /* ffmpeg 编解码器参数 */
    AVStream *pStream;              /* ffmpeg 视频流 */
    AVPacket *packet;               /* ffmpeg 单帧数据包 */

};

#endif //H265TOJPEG_ENCODER_H
