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
                LOG("readCallback | read offset: %d", data->offset);
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
    struct Output *data = (struct Output *) opaque;

    if (buf_size > HEAP_SIZE) {
        return -1;
    }

    memcpy(data->jpeg_data + data->offset, buf, buf_size);
    data->offset += buf_size;
    return buf_size;
}

/**
 *
 * @param pFrame
 * @param output_data
 * @return
 */
int saveJpeg(AVFrame *pFrame, struct Output *output_data) {
    int width = pFrame->width;
    int height = pFrame->height;

    if(DEBUG){
        LOG("width=%d, height=%d", height, width);
    }

    AVCodecContext *pCodeCtx = NULL;

    // 打开输出文件
    AVFormatContext *pFormatCtx = NULL;
    avformat_alloc_output_context2(&pFormatCtx, NULL, "mjpeg", NULL);

    unsigned char *outBuf = (unsigned char *)av_malloc(HEAP_SIZE);

    // 新建一个 AVIOContext ，并与 pFormatCtx 关联
    AVIOContext *avioCtx = avio_alloc_context(outBuf, HEAP_SIZE, 1, output_data, NULL, writeCallback, NULL);

    pFormatCtx->pb = avioCtx;
    pFormatCtx->flags = AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_FLUSH_PACKETS;

    // 构建一个新 stream
    AVStream *pAVStream = avformat_new_stream(pFormatCtx, 0);
    if (pAVStream == NULL) {
        return -1;
    }

    AVCodecParameters *parameters = pAVStream->codecpar;
    parameters->codec_id = pFormatCtx->oformat->video_codec;
    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
    parameters->format = AV_PIX_FMT_YUVJ420P;
    parameters->width = pFrame->width;
    parameters->height = pFrame->height;

    AVCodec *pCodec = avcodec_find_encoder(pAVStream->codecpar->codec_id);

    if (!pCodec) {
        LOG("Could not find encoder");
        return -1;
    }

    pCodeCtx = avcodec_alloc_context3(pCodec);
    if (!pCodeCtx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return -1;
    }

    int ret = avcodec_parameters_to_context(pCodeCtx, pAVStream->codecpar);
    if ( ret < 0) {
        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return -1;
    }

    pCodeCtx->time_base = (AVRational) {1, 25};

    ret = avcodec_open2(pCodeCtx, pCodec, NULL);
    if ( ret < 0) {
        LOG("Could not open codec.");
        return -1;
    }

    ret = avformat_write_header(pFormatCtx, NULL);
    if (ret < 0) {
        LOG("write_header fail");
        return -1;
    }

    int y_size = width * height;

    //Encode
    // 给AVPacket分配足够大的空间
    AVPacket pkt;
    av_new_packet(&pkt, y_size * 3);

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
    avio_flush(avioCtx);
    // LOG("av_write_frame done");
    if (ret < 0) {
        LOG("Could not av_write_frame");
        return -1;
    }

    // LOG("av_packet_unref");
    av_packet_unref(&pkt);

    //Write Trailer
    av_write_trailer(pFormatCtx);
    // LOG("av_write_trailer");

    if (pFormatCtx->pb) {
        avio_context_free(&pFormatCtx->pb);
    }
    // LOG("avio_free");

    if (pFormatCtx) {
        /**
         * void avformat_free_context(AVFormatContext *s);
         * 释放 AVFormatContext 上下文
         */
        avformat_free_context(pFormatCtx);
    }
    // LOG("avformat_free_context");

    if (pCodeCtx) {
        /**
         * int avcodec_close(AVCodecContext *avctx);
         * 关闭给定的 AVCodecContext 并释放与之关联的所有数据（但不是 AVCodecContext 本身）
         */
        avcodec_close(pCodeCtx);
    }
    // LOG("avcodec_close");

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
     * 打开输入流并读取标头。成功返回 0 ，失败返回负值
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

    /**
     * int avcodec_open2(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
     * 初始化 AVCodeContext 以使用给定的 AVCodec
     */
    avcodec_open2(codecCtx, codec, NULL); //打开解码器

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
    av_read_frame(fmtCtx, &packet); //从文件中读取一个数据包, 存储到avpkt中
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
        // 循环获取数据（一个分组数据包可能存在多帧数据）
        while (avcodec_receive_frame(codecCtx, frame) == 0) {
            return saveJpeg(frame, outputData);
        }
    }

    av_packet_unref(&packet);

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

    const char * const h265FileType = "h265";
    const int size = strlen(h265FileType);

    if(strlen(inputFile) <= size){
        LOG("源文件的文件名不正确！");
        return -1;
    }

    // 获取源文件的文件类型
    char fileType[size+1] = "";
    memset(fileType, 0, size+1);
    strncpy(fileType, inputFile + (strlen(inputFile) - size), size);

    if(!(strcmp(fileType, "h265") == 0 || strcmp(fileType, "H265") == 0)){
        LOG("源文件不是 h265 格式的文件！");
        return -1;
    }

    FILE *fp_read = fopen(inputFile, "rb+");
    if (!fp_read) {
        LOG("Open file error! inputFile=%s, errno=%d", inputFile, errno);
        return -1;
    }

    char *inputBuf = (char *) malloc(HEAP_SIZE * sizeof(char));
    memset(inputBuf, 0, HEAP_SIZE);

    size_t fileLen = fread(inputBuf, 1, HEAP_SIZE, fp_read);
    if (!fileLen) {
        LOG("Read file error! inputFile=%s, fileLen=%ld", inputFile, fileLen);
        return -1;
    }

    if(DEBUG){
        LOG("H265 file length=%ldB", fileLen);
    }

    /**
     * 【2】H265 转 Jpeg
     */

    struct Input inputData;
    inputData.h265_data = inputBuf;
    inputData.offset = 0;
    inputData.size = fileLen;

    char *outputBuf = (char *) malloc(HEAP_SIZE * sizeof(char));
    memset(outputBuf, 0, HEAP_SIZE);

    struct Output outputData;
    outputData.jpeg_data = outputBuf;
    outputData.offset = 0;

    int ret = convert(&inputData, &outputData);
    if (ret != 0) {
        return -1;
    }

    int offset = outputData.offset;

    /**
     * 【3】保存 Jpeg 图片数据
     */

    char *outputFile = "/Users/lixiaoqing/Desktop/lixiaoqing/codes/c++/H265ToJpeg/imgs/output.jpeg";
    char *buf = (char *) malloc(offset + 1);
    memset(buf, 0, offset + 1);
    memcpy(buf, outputBuf, offset);

    FILE *fp_write = fopen(outputFile, "wb+");
    if (!fp_write) {
        LOG("Open file error! outputFile=%s, errno=%d", outputFile, errno);
        return -1;
    }

    fwrite(buf, 1, offset, fp_write);

    LOG("目标文件：%s", outputFile);

    if (inputBuf) {
        free(inputBuf);
    }
    if (outputBuf) {
        free(outputBuf);
    }
    if (buf) {
        free(buf);
    }

    fclose(fp_read);
    fclose(fp_write);

    return 0;
}

