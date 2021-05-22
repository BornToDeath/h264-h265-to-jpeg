//
// Created by lixiaoqing on 2021/5/21.
//

#ifndef H265TOJPEG_DECODER_H
#define H265TOJPEG_DECODER_H


#include <iostream>
#include <memory>
#include "Common.h"
#include "IDecoder.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavdevice/avdevice.h"
}


/**
 * 解码器
 */
class Decoder : public IDecoder{

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
    bool H265ToJpeg(const char *const inputFilePath, const char *const outputFilePath) override;

private:

    /**
     * 读取 H265 文件到内存
     * @param filePath
     * @return
     */
    bool readH265File(const char *const filePath);

    /**
     * 构建 OutPut 结构体
     * @return
     */
    bool constructOutputData();

    /**
     * 释放资源
     */
    void release();

    /**
     * 进行 H265 到 Jpeg 的转换
     * @return
     */
    bool convert();

    /**
     * 生成 Jpeg 文件
     * @param filePath
     * @return
     */
    bool writeJpeg2File(const char * const filePath);

private:

    /**
     * 输入的 H265 数据的结构体
     */
    std::shared_ptr<Input> inputData;

    /**
     * 输出的 Jpeg 数据的结构体
     */
    std::shared_ptr<Output> outputData;

    AVFormatContext   *fmtCtx;     /* ffmpeg 的全局上下文，所有 ffmpeg 都需要 */
    AVIOContext       *avioCtx;    /* ffmpeg 字节流 IO 上下文 */
    AVCodec           *codec;      /* ffmpeg 编解码器 */
    AVCodecContext    *codecCtx;   /* ffmpeg 编解码上下文 */
    AVCodecParameters *codecPar;   /* ffmpeg 编解码器参数 */
    AVFrame           *frame;      /* ffmpeg 单帧缓存 */
//    AVStream          *stream;     /* ffmpeg 视频流 */
    AVPacket          *packet;     /* ffmpeg 单帧数据包 */
    unsigned char     *ioBuf;      /* IO Buffer */
    int               streamType;  /* ffmpeg 流类型 */
};

#endif //H265TOJPEG_DECODER_H
