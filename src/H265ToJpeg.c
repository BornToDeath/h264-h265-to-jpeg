#include <stdio.h>

#include "H265ToJpeg.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavdevice/avdevice.h"


/* 是否是 debug 环境 */
#ifdef _BUILD_TYPE_DEBUG_
#define DEBUG 1
#else
#define DEBUG 0
#endif

/* 堆缓冲大小（单位：Byte） */
#define HEAP_SIZE (1024 * 1024)  // 1MB

/* 栈缓冲大小（单位：Byte） */
#define STACK_SIZE (1024)  // 1KB

/**
 * H265 数据的结构体
 */
struct Input {
    char *h265_data;
    int offset;
    int size;
};

/**
 * Jpeg 数据的结构体
 */
struct Output {
    char *jpeg_data;
    int offset;
};

/**
 * 日志打印
 * @param format
 * @param ...
 */
void LOG(const char * format, ...){
    // 对可变参数进行组合，合成一条完整的日志数据
    char log[STACK_SIZE] = {0};
    va_list arg_list;
    va_start(arg_list, format);
    vsnprintf(log, STACK_SIZE, format, arg_list);
    va_end(arg_list);
    printf("%s\n", log);
}

/**
 * 读取数据的回调函数。AVIOContext使用的回调函数！
 * 手动初始化AVIOContext只需要两个东西：内容来源的buffer，和读取这个Buffer到FFmpeg中的函数
 * 回调函数，功能就是：把buf_size字节数据送入buf即可
 * @param opaque
 * @param buf
 * @param buf_size
 * @return 注意：返回值是读取的字节数
 */
int readCallback(void *opaque, uint8_t *buf, int buf_size) {
    //从 opaque 拷贝 buf_size 个字节到 buf 中
    struct Input *data = (struct Input *) opaque;
    int size = data->size;
    char *h265_data = data->h265_data;

    if (data->offset + buf_size - 1 < size) {
        // 读取未溢出 直接复制
        memcpy(buf, h265_data + data->offset, buf_size);
        data->offset += buf_size;
        if (DEBUG) {
            LOG("Callback|read size: %d", buf_size);
        }
        return buf_size;
    } else {
        // 已经溢出无法读取
        if (data->offset >= size) {
            return -1;
        } else {
            // 还有剩余字节未读取但不到 buf_size ，读取剩余字节
            int real_read = size - data->offset;
            memcpy(buf, h265_data + data->offset, real_read);
            data->offset += buf_size;
            if(DEBUG){
                LOG("readCallback | read offset=%d", data->offset);
            }
            return real_read;
        }
    }
}

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

    struct Output *data = (struct Output *) opaque;
    memcpy(data->jpeg_data + data->offset, buf, buf_size);
    data->offset += buf_size;

    return buf_size;
}

/**
 *
 * @param pFrame
 * @param outputData
 * @return
 */
int yuv2Jpeg(AVFrame *pFrame, struct Output *outputData) {

    AVIOContext       *pIOCtx     = NULL;  /* ffmpeg 字节流 IO 上下文 */
    AVFormatContext   *pFormatCtx = NULL;  /* ffmpeg 的全局上下文，所有 ffmpeg 都需要 */
    AVCodecContext    *pCodeCtx   = NULL;  /* ffmpeg 编解码上下文 */
    AVCodec           *pCodec     = NULL;  /* ffmpeg 编解码器 */
    AVCodecParameters *pCodecPars = NULL;  /* ffmpeg 编解码器参数 */
    AVStream          *pStream    = NULL;  /* ffmpeg 视频流 */
    AVPacket          pkt;                 /* ffmpeg 单帧数据包 */

    unsigned char *outBuf = (unsigned char *)av_malloc(HEAP_SIZE);

    // 初始化 AVIOContext。将 outputData 中数据写入到 outBuf 中
    pIOCtx = avio_alloc_context(outBuf, HEAP_SIZE, 1, outputData, NULL, writeCallback, NULL);

    /**
     * int avformat_alloc_output_context2(AVFormatContext **ctx, ff_const59 AVOutputFormat *oformat,
                                   const char *format_name, const char *filename);
     * 初始化一个用于输出的 AVFormatContext 结构体
     * 参考：https://blog.csdn.net/leixiaohua1020/article/details/41198929
     */

    // 打开输出文件，初始化输出视频码流的 AVFormatContext
    avformat_alloc_output_context2(&pFormatCtx, NULL, "mjpeg", NULL);

    pFormatCtx->pb    = pIOCtx;
    pFormatCtx->flags = AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_FLUSH_PACKETS;

    // 创建输出码流的 AVStream
    pStream = avformat_new_stream(pFormatCtx, NULL);
    if (pStream == NULL) {
        return -1;
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
        return -1;
    }

    // 申请 AVCodecContext 空间
    pCodeCtx = avcodec_alloc_context3(pCodec);
    if (!pCodeCtx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return -1;
    }

    // 替换编码器 AVCodecContext 上下文参数
    int ret = avcodec_parameters_to_context(pCodeCtx, pCodecPars);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy %s codec pCodecPars to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return -1;
    }

    // 设置时基。该字段解码时无需设置，编码时需要用户手动指定
    pCodeCtx->time_base = (AVRational) {1, 25};

    // 打开编码器
    ret = avcodec_open2(pCodeCtx, pCodec, NULL);
    if (ret < 0) {
        LOG("Could not open codec, ret = %d", ret);
        return -1;
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
        LOG("avformat_write_header fail, ret = %d", ret);
        return -1;
    }

    // 给AVPacket分配足够大的空间
    av_new_packet(&pkt, pFrame->width * pFrame->height * 3);

    // 编码数据
    ret = avcodec_send_frame(pCodeCtx, pFrame);
    if (ret < 0) {
        LOG("Could not avcodec_send_frame, ret = %d", ret);
        return -1;
    }

    // 得到编码后数据
    ret = avcodec_receive_packet(pCodeCtx, &pkt);
    if (ret < 0) {
        LOG("Could not avcodec_receive_packet, ret = %d", ret);
        return -1;
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
    ret = av_write_frame(pFormatCtx, &pkt);
    if (ret < 0) {
        LOG("Could not av_write_frame, ret = %d", ret);
        return -1;
    }

    // 强制清除缓存中的数据
    avio_flush(pIOCtx);

    // 写视频文件尾
    ret = av_write_trailer(pFormatCtx);
    if (ret < 0) {
        LOG("av_write_trailer fail, ret = %d", ret);
        return -1;
    }

    // 释放数据包
    av_packet_unref(&pkt);

    if (pFormatCtx->pb) {
        // 关闭和释放 AVIOContext
        avio_context_free(&pFormatCtx->pb);
    }

    /**
     * int avcodec_close(AVCodecContext *avctx);
     * 关闭给定的 AVCodecContext 并释放与之关联的所有数据（但不是 AVCodecContext 本身）
     */

    // 关闭编码器
    avcodec_close(pCodeCtx);

    if (pFormatCtx) {
        /**
         * void avformat_free_context(AVFormatContext *s);
         * 释放 AVFormatContext 上下文
         */

        // 释放 AVFormatContext
        avformat_free_context(pFormatCtx);
    }

    return 0;
}

/**
 * H265 转 Jpeg
 * @param inputData  H265 数据
 * @param outputData Jpeg 数据
 * @return
 */
int convert(struct Input *inputData, struct Output *outputData) {

    AVFormatContext   *fmtCtx   = NULL;  /* ffmpeg 的全局上下文，所有 ffmpeg 都需要 */
    AVIOContext       *avio     = NULL;  /* ffmpeg 字节流 IO 上下文 */
    AVCodec           *codec    = NULL;  /* ffmpeg 编解码器 */
    AVCodecContext    *codecCtx = NULL;  /* ffmpeg 编解码上下文 */
    AVCodecParameters *codecPar = NULL;  /* ffmpeg 编解码器参数 */
    AVFrame           *frame    = NULL;  /* ffmpeg 单帧缓存 */
    AVStream          *stream   = NULL;  /* ffmpeg 视频流 */
    AVPacket          packet;            /* ffmpeg 单帧数据包 */
    int               streamType;        /* ffmpeg 流类型 */

    unsigned char *ioBuf = (unsigned char *)av_malloc(HEAP_SIZE);

    /**
     * AVIOContext *avio_alloc_context(
                  unsigned char *buffer,
                  int buffer_size,
                  int write_flag,
                  void *opaque,
                  int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
                  int (*write_packet)(void *opaque, uint8_t *buf, int buf_size),
                  int64_t (*seek)(void *opaque, int64_t offset, int whence));
     * 为缓冲 I/O 分配并初始化 AVIOContext 。从 opaque 中读取数据，读到 buffer 中
     *   buffer         : 输入/输出缓存内存块，必须是使用 av_malloc() 分配的
     *   buffer_size    : 缓存大小
     *   write_flag     : 如果缓存为可写则设置为 1 ，否则设置为 0
     *   opaque         : 指针，用于回调时使用
     *   (*read_packet) : 读回调
     *   (*write_packet): 写回调
     */

    // 从输入的 H265 文件中读取数据到 ioBuf 中
    avio = avio_alloc_context(ioBuf, HEAP_SIZE, 0, inputData, readCallback, NULL, NULL);

    /**
     * AVFormatContext *avformat_alloc_context(void);
     * 初始化 ffmpeg 上下文
     */

    // 初始化 AVFormatContext
    fmtCtx = avformat_alloc_context();

    // pb: IO 上下文，通过对该变量进行赋值可改变输入源或输出目的
    fmtCtx->pb = avio;

    /**
     * int avformat_open_input(AVFormatContext **ps, const char *url, ff_const59 AVInputFormat *fmt, AVDictionary **options);
     * 打开输入文件，初始化输入视频码流的 AVFormatContext
     * 成功返回值 >=0 ，失败返回负值
     *   ps:      指向用户提供的 AVFormatContext 指针
     *   url:     要打开的流的 url
     *   fmt:     如果非空，则此参数强制使用特定的输入格式，否则将自动检测格式
     *   options: 附加选项，一般可为 NULL
     */

    int ret = avformat_open_input(&fmtCtx, "nothing", NULL, NULL);
    if (ret < 0) {
        LOG("%s line=%d | Error in avformat_open_input(), ret=%d", __FUNCTION__, __LINE__, ret);
        return -1;
    }

    /**
     * int avformat_find_stream_info(AVFormatContext *ic, AVDictionary **options);
     * 探测码流格式。读取检查媒体文件的数据包以获取具体的流信息，如媒体存入的编码格式。失败返回负值
     *   ic:      媒体文件上下文
     *   options: 额外选项，包含一些配置选项
     */

    ret = avformat_find_stream_info(fmtCtx, NULL);
    if (ret < 0) {
        LOG("%s line=%d | Error in find stream, ret=%d", __FUNCTION__, __LINE__, ret);
        return -1;
    }

    // 获取视频流的索引
    streamType = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (streamType < 0) {
        LOG("Error in find best stream type, streamType=%d", streamType);
        return -1;
    }

    // 获取流对应的解码器参数
    codecPar = fmtCtx->streams[streamType]->codecpar;

    if(DEBUG) {
        struct AVRational base = fmtCtx->streams[streamType]->time_base;
        LOG("时基：num=%d, den=%d", base.num, base.den);
    }

    /**
     * AVCodec *avcodec_find_decoder(enum AVCodecID id);
     * 根据解码器 ID 查找一个匹配的已注册解码器。未找到返回 NULL
     */

    // 对找到的视频流解码器
    codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        LOG("%s line=%d | Error in get the codec", __FUNCTION__, __LINE__);
        return -1;
    }

    /**
     * AVCodecContext *avcodec_alloc_context3(const AVCodec *codec);
     * 申请 AVCodecContext 空间
     */

    codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        LOG("Error in allocate the codecCtx");
        return -1;
    }

    // 替换解码器上下文参数。将视频流信息拷贝到 AVCodecContext 中
    ret = avcodec_parameters_to_context(codecCtx, codecPar);
    if (ret < 0) {
        LOG("Error in replace the parameters int the codecCtx, ret=%d", ret);
        return -1;
    }

    if(DEBUG){
        char type[32];
        switch (codecCtx->codec_type) {
            case AVMEDIA_TYPE_UNKNOWN:
                strcpy(type, "AVMEDIA_TYPE_UNKNOWN");
                break;
            case AVMEDIA_TYPE_VIDEO:
                strcpy(type, "AVMEDIA_TYPE_VIDEO");
                break;
            case AVMEDIA_TYPE_AUDIO:
                strcpy(type, "AVMEDIA_TYPE_AUDIO");
                break;
            case AVMEDIA_TYPE_DATA:
                strcpy(type, "AVMEDIA_TYPE_DATA");
                break;
            case AVMEDIA_TYPE_SUBTITLE:
                strcpy(type, "AVMEDIA_TYPE_SUBTITLE");
                break;
            case AVMEDIA_TYPE_ATTACHMENT:
                strcpy(type, "AVMEDIA_TYPE_ATTACHMENT");
                break;
            case AVMEDIA_TYPE_NB:
                strcpy(type, "AVMEDIA_TYPE_NB");
                break;
            default:
                LOG("No stream type!");
        }
        LOG("Stream type is: %s", type);
    }

    /**
     * int avcodec_open2(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
     * 使用给定的 AVCodec 初始化 AVCodecContext
     *   avctx: 需要初始化的 AVCodecContext
     *   codec: 输入的 AVCodec
     */

    // 打开解码器
    avcodec_open2(codecCtx, codec, NULL);

    if(DEBUG) {
        LOG("codecCtx->width=%d, codecCtx->height=%d", codecCtx->width, codecCtx->height);
    }

    // 初始化 AVFrame ，用默认值填充字段
    frame = av_frame_alloc();
    if (!frame) {
        LOG("%s line=%d | Error in allocate the frame", __FUNCTION__, __LINE__);
        return -1;
    }

    /**
     * void av_init_packet(AVPacket *pkt);
     * 初始化数据包
     */

    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    /**
     * int av_read_frame(AVFormatContext *s, AVPacket *pkt);
     *   s  : 输入的 AVFormatContext
     *   pkt: 输出的 AVPacket
     * 返回流的下一帧。此函数返回存储在文件中的内容，不对有效的帧进行验证，不会省略有效帧之间的无效数据，以便给解码器最大可用于解码的信息。
     * 成功返回 >=0 （>0 是文件末尾），失败返回负值
     */

    // 读取码流数据中的一帧视频帧。从输入文件中读取一个 AVPacket 数据包, 存储到 packet 中。一个 packet 是一帧压缩数据（I + P + P + ...）？
    av_read_frame(fmtCtx, &packet);

    if(DEBUG) {
        LOG("packet.pts=%lld", packet.pts);
    }

    if (packet.stream_index == streamType) {                                                 // 读取的数据包类型正确
        /**
         * int avcodec_send_packet(AVCodecContext *avctx, const AVPacket *packet);
         * 将原始分组数据包发送给解码器
         * avctx: 编解码器上下文
         * packet:
         */

        // 将数据包发送到解码器中
        ret = avcodec_send_packet(codecCtx, &packet);
        if (ret < 0) {
            LOG("Error in the send packet, ret=%d", ret);
            return -1;
        }

        /**
         * int avcodec_receive_frame(AVCodecContext *avctx, AVFrame *frame);
         * 从解码器返回解码输出数据
         * avctx: 编解码器上下文
         * frame:
         */

        // 从解码器获取解码后的帧，循环获取数据（一个分组数据包可能存在多帧数据）
        while (avcodec_receive_frame(codecCtx, frame) == 0) {
//            frame->pts = av_rescale_q(av_gettime(), (AVRational){1, 1200000}, fmtCtx->streams[streamType]->time_base);
            if (DEBUG) {
                struct AVRational frameRate = av_guess_frame_rate(fmtCtx, fmtCtx->streams[streamType], frame);
                LOG("帧率：num=%d, den=%d", frameRate.num, frameRate.den);
                LOG("帧时间戳：%lld", frame->pts);
            }
            ret = yuv2Jpeg(frame, outputData);
            if (ret != 0) {
                LOG("Yuv 编码为 Jpeg 失败！ret=%d", ret);
            }
            break;
        }
    }

    // 释放数据包
    av_packet_unref(&packet);

    // 关闭解码器
    avcodec_close(codecCtx);

    // 关闭 ffmpeg 上下文
    avformat_close_input(&fmtCtx);

    return ret;
}

struct Input * readH265File(const char * const filePath) {

    const char *const h265FileType = "h265";
    const int size = strlen(h265FileType);

    if (strlen(filePath) <= size) {
        LOG("源文件的文件名不正确！");
        return NULL;
    }

    // 获取源文件的文件类型
    char fileType[size + 1] = "";
    memset(fileType, 0, size + 1);
    strncpy(fileType, filePath + (strlen(filePath) - size), size);

    if (!(strcmp(fileType, "h265") == 0 || strcmp(fileType, "H265") == 0)) {
        LOG("源文件不是 h265 格式的文件！");
        return NULL;
    }

    char *inputBuf = (char *) malloc(HEAP_SIZE * sizeof(char));
    if (!inputBuf) {
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        return NULL;
    }

    FILE *fp_read = fopen(filePath, "rb+");
    if (!fp_read) {
        LOG("Open file error! filePath=%s, errno=%d", filePath, errno);
        return NULL;
    }

    memset(inputBuf, 0, HEAP_SIZE);

    size_t fileLen = fread(inputBuf, 1, HEAP_SIZE, fp_read);
    if (fileLen == 0) {
        LOG("H265 文件为空！H265 文件路径：%s", filePath);
        fclose(fp_read);
        return NULL;
    }

    struct Input *inputData = (struct Input *) malloc(sizeof(struct Input));
    if (!inputData) {
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        return NULL;
    }

    inputData->h265_data = inputBuf;
    inputData->offset = 0;
    inputData->size = fileLen;

    fclose(fp_read);

    return inputData;
}

int write2Jpeg(const struct Output * const data, const char * const filePath){

    int offset = data->offset;
    char *buf = (char *) malloc(offset + 1);
    if(!buf){
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        return -1;
    }

    memset(buf, 0, offset + 1);
    memcpy(buf, data->jpeg_data, offset);

    FILE *fp_write = fopen(filePath, "wb+");
    if (!fp_write) {
        LOG("Open file error! filePath=%s, errno=%d", filePath, errno);
        return -1;
    }

    size_t ret = fwrite(buf, 1, offset, fp_write);
    if(ret == 0){
        LOG("%s line=%d | 保存 Jpeg 文件出错！Jpeg 文件路径：%s", __FUNCTION__, __LINE__, filePath);
        fclose(fp_write);
        return -1;
    }

    fclose(fp_write);
    return 0;
}

/**
 * H265 帧转 Jpeg
 * @param inputFilePath  输入的 H265 文件路径
 * @param outputFilePath 输出的 Jpeg 文件路径
 * @return 0: 成功
 *        -1: 失败
 */
int H265ToJpeg(const char * const inputFilePath, const char * const outputFilePath) {

    if (inputFilePath == NULL || outputFilePath == NULL || strlen(inputFilePath) == 0 || strlen(outputFilePath) == 0) {
        LOG("输入或输出的文件路径为空，请核查！");
        return -1;
    }

    /**
     * 【1】读取 H265 帧数据
     */

    struct Input *inputData = readH265File(inputFilePath);
    if (!inputData) {
        return -1;
    }

    if (DEBUG) {
        LOG("H265 file length=%dB", inputData->size);
    }

    /**
     * 【2】H265 转 Jpeg
     */

    char *outputBuf = (char *) malloc(HEAP_SIZE * sizeof(char));
    if (!outputBuf) {
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        return -1;
    }

    memset(outputBuf, 0, HEAP_SIZE);

    struct Output *outputData = (struct Output *) malloc(sizeof(struct Output));
    if (!outputData) {
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        return -1;
    }

    outputData->jpeg_data = outputBuf;
    outputData->offset = 0;

    // 将 H265 数据转换为 Jpeg 数据
    int ret = convert(inputData, outputData);
    if (ret != 0) {
        LOG("%s line=%d | H265 解码为 Jpeg 出错！ret=%d", __FUNCTION__, __LINE__, ret);
        return -1;
    }

    /**
     * 【3】保存 Jpeg 图片数据
     */

    ret = write2Jpeg(outputData, outputFilePath);
    if (ret == -1) {
        LOG("%s line=%d | 保存 Jpeg 文件出错！Jpeg 文件路径：%s", __FUNCTION__, __LINE__, outputFilePath);
        return -1;
    }

    if (inputData) {
        if (inputData->h265_data) {
            free(inputData->h265_data);
            inputData->h265_data = NULL;
        }
        free(inputData);
    }

    if (outputData) {
        if (outputData->jpeg_data) {
            free(outputData->jpeg_data);
            outputData->jpeg_data = NULL;
        }
        free(outputData);
    }

    return 0;
}

