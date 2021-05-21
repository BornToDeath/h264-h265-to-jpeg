//
// Created by lixiaoqing on 2021/5/21.
//

#include "Encoder.h"


extern void LOG(const char *format, ...);

/**
 * 写文件的回调函数
 * @param opaque
 * @param buf
 * @param buf_size
 * @return
 */
int writeCallback(void *opaque, uint8_t *buf, int buf_size) {
    // 从 buf 中拷贝 buf_size 个字节到 opaque 中
    if (buf_size > HEAP_SIZE) {
        return -1;
    }

    Output *data = (Output *) opaque;
    memcpy(data->jpeg_data + data->offset, buf, buf_size);
    data->offset += buf_size;

    return buf_size;
}


Encoder::Encoder() {
    pIOCtx = nullptr;      /* ffmpeg 字节流 IO 上下文 */
    pFormatCtx = nullptr;  /* ffmpeg 的全局上下文，所有 ffmpeg 都需要 */
    pCodeCtx = nullptr;    /* ffmpeg 编解码上下文 */
    pCodec = nullptr;      /* ffmpeg 编解码器 */
    pCodecPars = nullptr;  /* ffmpeg 编解码器参数 */
    pStream = nullptr;     /* ffmpeg 视频流 */
    packet = nullptr;      /* ffmpeg 单帧数据包 */
    ioBuf = nullptr;
}

Encoder::~Encoder() {
    release();
}

void Encoder::release() {
    if (ioBuf) {
        // 释放 IO Buffer
        av_free(ioBuf);
        ioBuf = nullptr;
    }
    if (pIOCtx) {
        // 释放 IO 上下文
        avio_context_free(&pIOCtx);
        pIOCtx = nullptr;
    }
    if (pFormatCtx) {
        // 关闭 ffmpeg 上下文
        avformat_close_input(&pFormatCtx);
        pFormatCtx = nullptr;
    }
    if (pCodeCtx) {
        // 关闭解码器
        avcodec_free_context(&pCodeCtx);
        pCodeCtx = nullptr;
    }
    if (packet) {
        // 释放数据包
        av_packet_free(&packet);
        packet = nullptr;
    }
}

bool Encoder::yuv2Jpeg(AVFrame *pFrame, Output *outputData) {

    // 用于输出错误日志
    char errorBuf[STACK_SIZE];

    // IO Buffer
    ioBuf = (unsigned char *)av_malloc(HEAP_SIZE);
    if (!ioBuf) {
        LOG("%s line=%d | av_malloc failed", __FUNCTION__, __LINE__);
        release();
        return false;
    }

    // 初始化 AVIOContext。将 outputData 中数据写入到 outBuf 中
    pIOCtx = avio_alloc_context(ioBuf, HEAP_SIZE, 1, outputData, NULL, writeCallback, NULL);
    if (!pIOCtx) {
        LOG("%s line=%d | avio_alloc_context failed.", __FUNCTION__, __LINE__);
        release();
        return false;
    }

    /**
     * int avformat_alloc_output_context2(AVFormatContext **ctx, ff_const59 AVOutputFormat *oformat,
                                   const char *format_name, const char *filename);
     * 初始化一个用于输出的 AVFormatContext 结构体
     * 参考：https://blog.csdn.net/leixiaohua1020/article/details/41198929
     */

    // 打开输出文件，初始化输出视频码流的 AVFormatContext
    int ret = avformat_alloc_output_context2(&pFormatCtx, NULL, "mjpeg", NULL);
    if(ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("avformat_alloc_output_context2 failed, ret=%d, error=%s", ret, errorBuf);
        release();
        return false;
    }

    pFormatCtx->pb    = pIOCtx;
    pFormatCtx->flags = AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_FLUSH_PACKETS;

    // 创建输出码流的 AVStream
    pStream = avformat_new_stream(pFormatCtx, NULL);
    if (pStream == NULL) {
        LOG("avformat_new_stream failed");
        release();
        return false;
    }

    pCodecPars             = pStream->codecpar;
    pCodecPars->codec_id   = pFormatCtx->oformat->video_codec;
    pCodecPars->codec_type = AVMEDIA_TYPE_VIDEO;
    pCodecPars->format     = AV_PIX_FMT_YUVJ420P;
    pCodecPars->width      = pFrame->width;
    pCodecPars->height     = pFrame->height;

    if(DEBUG) {
        LOG("解码后原始数据类型：%d", pFrame->format);  // format 是 AVPixelFormat 类型
        LOG("是否是关键帧：%d", pFrame->key_frame);
        LOG("帧类型：%d", pFrame->pict_type);
        LOG("帧时间戳：%lld", pFrame->pts);
        LOG("codec_id=%d", pCodecPars->codec_id);
    }

    // 通过 id 查找一个匹配的已经注册的音视频编码器
    pCodec = avcodec_find_encoder(pCodecPars->codec_id);

    if (!pCodec) {
        LOG("Could not find encoder");
        release();
        return false;
    }

    // 申请 AVCodecContext 空间
    pCodeCtx = avcodec_alloc_context3(pCodec);
    if (!pCodeCtx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        release();
        return false;
    }

    // 替换编码器 AVCodecContext 上下文参数
    ret = avcodec_parameters_to_context(pCodeCtx, pCodecPars);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy %s codec pCodecPars to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        release();
        return false;
    }

    // 设置时基。该字段解码时无需设置，编码时需要用户手动指定
    pCodeCtx->time_base = (AVRational) {1, 25};

    // 打开编码器
    ret = avcodec_open2(pCodeCtx, pCodec, NULL);
    if (ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("Could not open codec, ret=%d, error=%s", ret, errorBuf);
        release();
        return false;
    }

    /**
     * int avformat_write_header(AVFormatContext *s, AVDictionary **options);
     * 把流头信息写入到媒体文件中。成功返回 0
     *   s      : ffmpeg 上下文
     *   options: 额外选项
     */

    // 写视频文件头
    ret = avformat_write_header(pFormatCtx, NULL);
    if (ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("avformat_write_header failed, ret=%d, error=%s", ret, errorBuf);
        release();
        return false;
    }

    /**
     * AVPacket *av_packet_alloc(void);
     * 创建 AVPacket
     */

    packet = av_packet_alloc();
    if (!packet) {
        LOG("av_packet_alloc failed");
        release();
        return false;
    }

    // 给AVPacket分配足够大的空间
    ret = av_new_packet(packet, pFrame->width * pFrame->height * 3);
    if (ret != 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("av_new_packet failed, ret=%d, error=%s", ret, errorBuf);
        release();
        return false;
    }

    // 编码数据
    ret = avcodec_send_frame(pCodeCtx, pFrame);
    if (ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("Could not avcodec_send_frame, ret=%d, error=%s", ret, errorBuf);
        release();
        return false;
    }

    // 得到编码后数据
    ret = avcodec_receive_packet(pCodeCtx, packet);
    if (ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("avcodec_receive_packet failed, ret=%d, error=%s", ret, errorBuf);
        release();
        return false;
    }

//    pkt.pts = av_rescale_q(pkt.pts, pCodeCtx->time_base, pStream->time_base);
//    av_packet_rescale_ts(&pkt, pCodeCtx->time_base, pStream->time_base);

    /**
     * int av_write_frame(AVFormatContext *s, AVPacket *pkt);
     * 输出一帧音视频数据
     *   s  : AVFormatContext 输出上下文
     *   pkt: 待输出的 AVPacket
     */

    // 写视频数据
    ret = av_write_frame(pFormatCtx, packet);
    if (ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("av_write_frame failed, ret=%d, error=%s", ret, errorBuf);
        release();
        return false;
    }

    // 强制清除缓存中的数据
    avio_flush(pIOCtx);

    // 写视频文件尾
    ret = av_write_trailer(pFormatCtx);
    if (ret < 0) {
        LOG("av_write_trailer fail, ret = %d", ret);
        release();
        return false;
    }

    // 此处无需调用 release(), 因为此类在析构时会自动调用 release()
//    release();

    return true;
}