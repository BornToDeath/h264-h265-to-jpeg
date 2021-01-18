#include <stdio.h>

//#include "H265ToJpeg.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavdevice/avdevice.h"

#define MAX_LEN (1048576)

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

//函数声明
//int saveJpg(AVFrame *pFrame, struct Output *output_data);

//缓存1Mb
const int BUF_SIZE = 1048576;


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
        printf("Callback|read size: %d\n", buf_size);
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
            printf("callback|read offset: %d\n", data->offset);
            return real_read;
        }
    }
}

//写文件的回调函数
int write_buffer(void *opaque, uint8_t *buf, int buf_size) {
    struct Output *data = (struct Output *) opaque;

    if (buf_size > BUF_SIZE) {
        return -1;
    }

    memcpy(data->jpeg_data + data->offset, buf, buf_size);
    data->offset += buf_size;
    return buf_size;
}

int saveJpg(AVFrame *pFrame, struct Output *output_data) {
    int width = pFrame->width;
    int height = pFrame->height;
    AVCodecContext *pCodeCtx = NULL;

    //打开输出文件
    // fp_write = fopen(out_name, "wb+");
    AVFormatContext *pFormatCtx = NULL;
    avformat_alloc_output_context2(&pFormatCtx, NULL, "mjpeg", NULL);
    // AVFormatContext *pFormatCtx = avformat_alloc_context();
    // 设置输出文件格式
    // pFormatCtx->oformat = av_guess_format("mjpeg", NULL, NULL);

    unsigned char *outbuffer = (unsigned char *) av_malloc(BUF_SIZE);
    //新建一个AVIOContext  并与pFormatCtx关联
    AVIOContext *avio_out = avio_alloc_context(outbuffer, BUF_SIZE, 1, output_data, NULL, write_buffer, NULL);
    pFormatCtx->pb = avio_out;
    pFormatCtx->flags = AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_FLUSH_PACKETS;

    // //创建并初始化输出AVIOContext
    // if (avio_open(&pFormatCtx->pb, "./bin/nothing.jpeg", AVIO_FLAG_READ_WRITE) < 0)
    // {
    //     printf("Couldn't open output file.\n");
    //     return -1;
    // }

    // 构建一个新stream
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
        printf("Could not find encoder\n");
        return -1;
    }

    pCodeCtx = avcodec_alloc_context3(pCodec);
    if (!pCodeCtx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return -1;
    }

    if ((avcodec_parameters_to_context(pCodeCtx, pAVStream->codecpar)) < 0) {
        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return -1;
    }

    pCodeCtx->time_base = (AVRational) {1, 25};

    if (avcodec_open2(pCodeCtx, pCodec, NULL) < 0) {
        printf("Could not open codec.");
        return -1;
    }

    int ret = avformat_write_header(pFormatCtx, NULL);
    if (ret < 0) {
        printf("write_header fail\n");
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
        printf("Could not avcodec_send_frame.");
        return -1;
    }

    // 得到编码后数据
    ret = avcodec_receive_packet(pCodeCtx, &pkt);
    if (ret < 0) {
        printf("Could not avcodec_receive_packet");
        return -1;
    }

    // printf("av_write_frame\n");
    ret = av_write_frame(pFormatCtx, &pkt);
    avio_flush(avio_out);
    // printf("av_write_frame done\n");
    if (ret < 0) {
        printf("Could not av_write_frame");
        return -1;
    }
    // printf("av_packet_unref\n");
    av_packet_unref(&pkt);

    //Write Trailer
    av_write_trailer(pFormatCtx);
    // printf("av_write_trailer\n");

    if (pFormatCtx->pb) {
        avio_context_free(&pFormatCtx->pb);
    }
    // printf("avio_free\n");

    if (pFormatCtx) {
        /**
         * void avformat_free_context(AVFormatContext *s);
         * 释放 AVFormatContext
         */
        avformat_free_context(pFormatCtx);
    }
    // printf("avformat_free_context\n");

    if (pCodeCtx) {
        /**
         * int avcodec_close(AVCodecContext *avctx);
         * 关闭给定的 AVCodecContext 并释放与之关联的所有数据（但不是 AVCodecContext 本身）
         */
        avcodec_close(pCodeCtx);
    }
    // printf("avcodec_close\n");

    return 0;
}

int convert(struct Input *inputData, struct Output *outputData) {

    /* ffmpeg 的全局上下文，所有 ffmpeg 都需要 */
    AVFormatContext *fmtCtx = NULL;

    /* ffmpeg 编解码器 */
    const AVCodec *codec = NULL;

    /* ffmpeg 编解码上下文 */
    AVCodecContext *codeCtx = NULL;

    /* ffmpeg 编解码器参数 */
    AVCodecParameters *originPar = NULL;

    /* ffmpeg 单帧缓存 */
    AVFrame *frame = NULL;

    /* ffmpeg 单帧数据包 */
    AVPacket avpkt;

    /* ffmpeg 视频流 */
    AVStream *stream = NULL;

    /* ffmpeg 流类型 */
    int streamType;

    unsigned char *ioBuf = (unsigned char *) av_malloc(BUF_SIZE);

    // 初始化 AVIOContext ，fillIOBuf 为自定义的回调函数
    AVIOContext *avio = avio_alloc_context(ioBuf, BUF_SIZE, 0, inputData, readCallback, NULL, NULL);

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
    int isOk = avformat_open_input(&fmtCtx, "nothing", NULL, NULL);
    if (isOk < 0) {
        printf("%s line=%d|Error in avformat_open_input(), isOk=%d", __FUNCTION__, __LINE__, isOk);
        return -1;
    }

    /**
     * int avformat_find_stream_info(AVFormatContext *ic, AVDictionary **options);
     * 读取检查媒体文件的数据包以获取具体的流信息，如媒体存入的编码格式。失败返回负值
     *   ic:      媒体文件上下文
     *   options: 字典，包含一些配置选项
     */
    isOk = avformat_find_stream_info(fmtCtx, NULL);
    if (isOk < 0) {
        printf("Error in find stream, isOk=%d", isOk);
        return -1;
    }

    //av_dump_format(fmt_ctx, 0, inputFile, 0); // 打印控制信息
    av_init_packet(&avpkt); // 初始化数据包
    avpkt.data = NULL;
    avpkt.size = 0;

    streamType = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (streamType < 0) {
        printf("Error in find best stream type");
        return -1;
    }

    // 获取流对应的解码器参数
    originPar = fmtCtx->streams[streamType]->codecpar;

    /**
     * AVCodec *avcodec_find_decoder(enum AVCodecID id);
     * 查找具有匹配编解码器 ID 的已注册解码器
     */
    codec = avcodec_find_decoder(originPar->codec_id); // 根据解码器参数获取解码器
    if (!codec) { // 未找到返回NULL
        printf("Error in get the codec");
        return -1;
    }

    codeCtx = avcodec_alloc_context3(codec); // 初始化解码器上下文
    if (!codeCtx) {
        printf("Error in allocate the codeCtx");
        return -1;
    }

    // 替换解码器上下文参数
    isOk = avcodec_parameters_to_context(codeCtx, originPar);
    if ( isOk < 0) {
        printf("Error in replace the parameters int the codeCtx");
        return -1;
    }

    /**
     * int avcodec_open2(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
     * 初始化 AVCodeContext 以使用给定的 AVCodec
     */
    avcodec_open2(codeCtx, codec, NULL); //打开解码器

    frame = av_frame_alloc(); // 初始化帧, 用默认值填充字段
    if (!frame) {
        printf("Error in allocate the frame");
        return -1;
    }

    /**
     * int av_read_frame(AVFormatContext *s, AVPacket *pkt);
     * 返回流的下一帧。此函数返回存储在文件中的内容，不对有效的帧进行验证，不会省略有效帧之间的无效数据，以便给解码器最大可用于解码的信息。
     * 成功返回 >=0 （>0 是文件末尾），失败返回负值
     */
    av_read_frame(fmtCtx, &avpkt); //从文件中读取一个数据包, 存储到avpkt中
    if (avpkt.stream_index == streamType) {                                                 // 读取的数据包类型正确
        /**
         * int avcodec_send_packet(AVCodecContext *avctx, const AVPacket *avpkt);
         * 将原始分组数据包发送给解码器
         * avctx: 编解码器上下文
         * avpkt:
         */
        // 将数据包发送到解码器中
        if (avcodec_send_packet(codeCtx, &avpkt) < 0) {
            printf("Error in the send packet");
            return -1;
        }

        /**
         * int avcodec_receive_frame(AVCodecContext *avctx, AVFrame *frame);
         * 从解码器返回解码输出数据
         * avctx: 编解码器上下文
         * frame:
         */
        // 循环获取数据（一个分组数据包可能存在多帧数据）
        while (avcodec_receive_frame(codeCtx, frame) == 0) {
            return saveJpg(frame, outputData);
        }
    }

    av_packet_unref(&avpkt);

    return 0;
}

int H265ToJpeg() {

    /**
     * 【1】读取 H265 帧数据
     */

    char *inputFile = "/Users/lixiaoqing/Desktop/lixiaoqing/codes/c++/H265ToJpeg/imgs/img02.h265";

    FILE *fp_read = fopen(inputFile, "rb+");
    if (!fp_read) {
        printf("Open file error! inputFile=%s, errno=%d\n", inputFile, errno);
        return -1;
    }

    char *inputBuf = (char *) malloc(BUF_SIZE * sizeof(char));
    memset(inputBuf, 0, BUF_SIZE);

    size_t fileLen = fread(inputBuf, 1, BUF_SIZE, fp_read);
    if (!fileLen) {
        printf("Read file error! inputFile=%s, fileLen=%ld\n", inputFile, fileLen);
        return -1;
    }

    printf("H265 file length=%ldB\n", fileLen);

    /**
     * 【2】H265 转换为 Jpeg
     */

    struct Input inputData;
    inputData.h265_data = inputBuf;
    inputData.offset = 0;
    inputData.size = fileLen;

    char *outputBuf = (char *) malloc(BUF_SIZE * sizeof(char));
    memset(outputBuf, 0, BUF_SIZE);

    struct Output outputData;
    outputData.jpeg_data = outputBuf;
    outputData.offset = 0;

    int isOk = convert(&inputData, &outputData);
    if (isOk != 0) {
        return -1;
    }

    int ret = outputData.offset;

    /**
     * 【3】保存 Jpeg 图片数据
     */

    char *outputFile = "/Users/lixiaoqing/Desktop/lixiaoqing/codes/c++/H265ToJpeg/imgs/output.jpeg";
    char *buf = (char *) malloc(ret + 1);
    memset(buf, 0, ret + 1);
    memcpy(buf, outputBuf, ret);

    //测试 将读取到的数据全部写入文件
    FILE *fp_write = fopen(outputFile, "wb+");
    if (!feof(fp_write)) {
        int true_size = fwrite(buf, 1, ret, fp_write);
    } else {
        printf("write file err");
        return 0;
    }

    if (buf) {
        free(buf);
    }
    if (inputBuf) {
        free(inputBuf);
    }
    if (outputBuf) {
        free(outputBuf);
    }

    fclose(fp_read);
    fclose(fp_write);

    return 0;
}

