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
    //从 h256_data 拷贝到 buf 中 buf_size 个字节
    struct Input *data = (struct Input *) opaque;
    int size = data->size;
    char *h265_data = data->h265_data;

    if (data->offset + buf_size - 1 < size) {
        // 读取未溢出 直接复制
        memcpy(buf, h265_data + data->offset, buf_size);
        data->offset += buf_size;
        LOG("Callback|read size: %d", buf_size);
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
int decode(AVFrame *pFrame, struct Output *outputData) {

    AVIOContext       *pIOCtx     = NULL;  /* ffmpeg 字节流 IO 上下文 */
    AVFormatContext   *pFormatCtx = NULL;  /* ffmpeg 的全局上下文，所有 ffmpeg 都需要 */
    AVCodecContext    *pCodeCtx   = NULL;  /* ffmpeg 编解码上下文 */
    AVCodec           *pCodec     = NULL;  /* ffmpeg 编解码器 */
    AVCodecParameters *pCodecPars = NULL;  /* ffmpeg 编解码器参数 */
    AVStream          *pStream    = NULL;  /* ffmpeg 视频流 */
    AVPacket          pkt;                 /* ffmpeg 单帧数据包 */

    unsigned char *outBuf = (unsigned char *)av_malloc(HEAP_SIZE);

    // 初始化 AVIOContext。将 outputData 中数据 写入到 outBuf 中
    pIOCtx = avio_alloc_context(outBuf, HEAP_SIZE, 1, outputData, NULL, writeCallback, NULL);

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

    pCodec = avcodec_find_encoder(pCodecPars->codec_id);
    if (!pCodec) {
        LOG("Could not find encoder");
        return -1;
    }

    pCodeCtx = avcodec_alloc_context3(pCodec);
    if (!pCodeCtx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return -1;
    }

    int ret = avcodec_parameters_to_context(pCodeCtx, pCodecPars);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy %s codec pCodecPars to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return -1;
    }

    pCodeCtx->time_base = (AVRational) {1, 25};

    ret = avcodec_open2(pCodeCtx, pCodec, NULL);
    if (ret < 0) {
        LOG("Could not open codec.");
        return -1;
    }

    // 写文件头
    ret = avformat_write_header(pFormatCtx, NULL);
    if (ret < 0) {
        LOG("write_header fail");
        return -1;
    }

    //Encode
    // 给AVPacket分配足够大的空间
    av_new_packet(&pkt, pFrame->width * pFrame->height * 3);

    // 编码数据
    ret = avcodec_send_frame(pCodeCtx, pFrame);
    if (ret < 0) {
        LOG("Could not avcodec_send_frame.");
        return -1;
    }

    // 得到编码后数据
    ret = avcodec_receive_packet(pCodeCtx, &pkt);
    if (ret < 0) {
        LOG("Could not avcodec_receive_packet");
        return -1;
    }

    // LOG("av_write_frame");
    ret = av_write_frame(pFormatCtx, &pkt);
    avio_flush(pIOCtx);
    // LOG("av_write_frame done");
    if (ret < 0) {
        LOG("Could not av_write_frame");
        return -1;
    }

    // 写文件尾
    av_write_trailer(pFormatCtx);
    // LOG("av_write_trailer");

    // LOG("av_packet_unref");
    av_packet_unref(&pkt);

    if (pFormatCtx->pb) {
        avio_context_free(&pFormatCtx->pb);
    }
    // LOG("avio_free");

    // LOG("avcodec_close");
    /**
     * int avcodec_close(AVCodecContext *avctx);
     * 关闭给定的 AVCodecContext 并释放与之关联的所有数据（但不是 AVCodecContext 本身）
     */
    avcodec_close(pCodeCtx);

    if (pFormatCtx) {
        /**
         * void avformat_free_context(AVFormatContext *s);
         * 释放 AVFormatContext 上下文
         */
        avformat_free_context(pFormatCtx);
    }
    // LOG("avformat_free_context");

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
     * 为缓冲 I/O 分配并初始化 AVIOContext ，稍后必须使用 avio_context_free() 释放它
     * 从 opaque 中读取数据，读到 buffer 中
     */
    avio = avio_alloc_context(ioBuf, HEAP_SIZE, 0, inputData, readCallback, NULL, NULL);

    /**
     * AVFormatContext *avformat_alloc_context(void);
     * 初始化 ffmpeg 上下文
     */
    fmtCtx = avformat_alloc_context();
    fmtCtx->pb = avio;

    /**
     * int avformat_open_input(AVFormatContext **ps, const char *url, ff_const59 AVInputFormat *fmt, AVDictionary **options);
     * 打开输入文件，初始化输入视频码流的 AVFormatContext
     * 成功返回 0 ，失败返回负值
     *   ps:      指向用户提供的 AVFormatContext 指针
     *   url:     要打开的流的 url
     *   fmt:     如果非空，则此参数强制使用特定的输入格式，否则将自动检测格式
     *   options: 包含 AVFormatContext 和 demuxer 私有选项的字典。返回时，此参数将被销毁并替换为包含找不到的选项，都有效则返回为空
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
     *   options: 字典，包含一些配置选项
     */
    ret = avformat_find_stream_info(fmtCtx, NULL);
    if (ret < 0) {
        LOG("%s line=%d | Error in find stream, ret=%d", __FUNCTION__, __LINE__, ret);
        return -1;
    }

    streamType = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (streamType < 0) {
        LOG("Error in find best stream type");
        return -1;
    }

    // 获取流对应的解码器参数
    codecPar = fmtCtx->streams[streamType]->codecpar;

    /**
     * AVCodec *avcodec_find_decoder(enum AVCodecID id);
     * 根据解码器 ID 查找匹配的已注册解码器。未找到返回 NULL
     */
    // 对找到的视频流寻解码器  // todo 软解？？？断点查看是硬解还是软解，是否能调试源码
    codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        LOG("%s line=%d | Error in get the codec", __FUNCTION__, __LINE__);
        return -1;
    }

    // 初始化解码器上下文
    codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        LOG("Error in allocate the codecCtx");
        return -1;
    }

    // 替换解码器上下文参数
    ret = avcodec_parameters_to_context(codecCtx, codecPar);
    if (ret < 0) {
        LOG("Error in replace the parameters int the codecCtx");
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
     * 初始化 AVCodeContext 以使用给定的 AVCodec
     */
    // 打开解码器
    avcodec_open2(codecCtx, codec, NULL);

    LOG("codecCtx->width=%d, codecCtx->height=%d", codecCtx->width, codecCtx->height);

    frame = av_frame_alloc(); // 初始化帧, 用默认值填充字段
    if (!frame) {
        LOG("Error in allocate the frame");
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
     * 返回流的下一帧。此函数返回存储在文件中的内容，不对有效的帧进行验证，不会省略有效帧之间的无效数据，以便给解码器最大可用于解码的信息。
     * 成功返回 >=0 （>0 是文件末尾），失败返回负值
     */
    // 从输入文件中读取一个数据包, 存储到 packet 中。一个 packet 是一帧压缩数据（I + P + P + ...）？
    av_read_frame(fmtCtx, &packet);

    if (packet.stream_index == streamType) {                                                 // 读取的数据包类型正确
        /**
         * int avcodec_send_packet(AVCodecContext *avctx, const AVPacket *packet);
         * 将原始分组数据包发送给解码器
         * avctx: 编解码器上下文
         * packet:
         */
        // 将数据包发送到解码器中
        if (avcodec_send_packet(codecCtx, &packet) < 0) {
            LOG("Error in the send packet");
            return -1;
        }

        /**
         * int avcodec_receive_frame(AVCodecContext *avctx, AVFrame *frame);
         * 从解码器返回解码输出数据
         * avctx: 编解码器上下文
         * frame:
         */
        // 从解码器获取解码后的帧，循环获取数据（一个分组数据包可能存在多帧数据）
        // 异步获取数据，前面 avcodec_send_packet() ，后面 while 循环接收 frame
        while (avcodec_receive_frame(codecCtx, frame) == 0) {
            return decode(frame, outputData);
        }
    }

    // 释放数据包
    av_packet_unref(&packet);

    // 关闭解码器
    avcodec_close(codecCtx);

    // 关闭 ffmpeg 上下文
    avformat_close_input(&fmtCtx);

    return 0;
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
 * H265 转 Jpeg
 * @return  0: 成功
 *         -1: 失败
 */
int H265ToJpeg() {

    /**
     * 【1】读取 H265 帧数据
     */

    char *inputFile = "/Users/lixiaoqing/Desktop/lixiaoqing/codes/c++/H265ToJpeg/imgs/img02.h265";
    LOG("源文件：%s", inputFile);

    struct Input * inputData = readH265File(inputFile);
    if(!inputData){
        return -1;
    }

    if(DEBUG){
        LOG("H265 file length=%dB", inputData->size);
    }

    /**
     * 【2】H265 转 Jpeg
     */

    char *outputBuf = (char *) malloc(HEAP_SIZE * sizeof(char));
    if(!outputBuf){
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        return -1;
    }

    memset(outputBuf, 0, HEAP_SIZE);

    struct Output * outputData = (struct Output *)malloc(sizeof(struct Output));
    if(!outputData){
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        return -1;
    }

    outputData->jpeg_data = outputBuf;
    outputData->offset = 0;

    int ret = convert(inputData, outputData);
    if (ret != 0) {
        LOG("%s line=%d | H265 解码为 Jpeg 出错！ret=%d", __FUNCTION__, __LINE__, ret);
        return -1;
    }

    /**
     * 【3】保存 Jpeg 图片数据
     */

    char *outputFile = "/Users/lixiaoqing/Desktop/lixiaoqing/codes/c++/H265ToJpeg/imgs/output.jpeg";
    LOG("目标文件：%s", outputFile);

    ret = write2Jpeg(outputData, outputFile);
    if(ret == -1){
        LOG("%s line=%d | 保存 Jpeg 文件出错！Jpeg 文件路径：%s", __FUNCTION__, __LINE__, outputFile);
        return -1;
    }

    if (inputData) {
        if(inputData->h265_data){
            free(inputData->h265_data);
            inputData->h265_data = NULL;
        }
        free(inputData);
    }

    if (outputData) {
        if(outputData->jpeg_data){
            free(outputData->jpeg_data);
            outputData->jpeg_data = NULL;
        }
        free(outputData);
    }

    return 0;
}

