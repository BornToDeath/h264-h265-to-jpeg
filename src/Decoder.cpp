//
// Created by lixiaoqing on 2021/5/21.
//

#include <iostream>
#include "Decoder.h"
#include "Encoder.h"


// 单例实现的 Decoder 对象
static Decoder *decoder = nullptr;
static std::mutex singletonMutex;

/**
 * 单例实现
 * @return
 */
IDecoder *IDecoder::getInstance() {
    // 双检锁
    if (decoder == nullptr) {
        std::unique_lock<std::mutex> lock(singletonMutex);
        if (decoder == nullptr) {
            decoder = new Decoder();
        }
    }
    return decoder;
}

/**
 * 日志打印
 * @param format
 * @param ...
 */
void LOG(const char *format, ...) {
    // 对可变参数进行组合，合成一条完整的日志数据
    char log[STACK_SIZE] = {0};
    va_list arg_list;
    va_start(arg_list, format);
    vsnprintf(log, STACK_SIZE, format, arg_list);
    va_end(arg_list);

    // 获取当前日期
    time_t curTs;
    time(&curTs);
    struct tm *timeInfo = localtime(&curTs);
    const int size = 64;
    char now[size];
    strftime(now, size, "%Y-%m-%d %H:%M:%S", timeInfo);

    printf("%s|%s\n", now, log);
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
    Input *data = (Input *) opaque;
    int size = data->size;
    char *h265_data = data->h265_data;

    if (data->offset + buf_size - 1 < size) {
        // 读取未溢出 直接复制
        memcpy(buf, h265_data + data->offset, buf_size);
        data->offset += buf_size;
        if (DEBUG) {
            LOG("%s line=%d | read size: %d", __FUNCTION__, __LINE__, buf_size);
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
            if (DEBUG) {
                LOG("%s line=%d | read offset=%d", __FUNCTION__, __LINE__, data->offset);
            }
            return real_read;
        }
    }
}


Decoder::Decoder() {
    fmtCtx = nullptr;    /* ffmpeg 的全局上下文，所有 ffmpeg 都需要 */
    avioCtx = nullptr;   /* ffmpeg 字节流 IO 上下文 */
    codec = nullptr;     /* ffmpeg 编解码器 */
    codecCtx = nullptr;  /* ffmpeg 编解码上下文 */
    codecPar = nullptr;  /* ffmpeg 编解码器参数 */
    frame = nullptr;     /* ffmpeg 单帧缓存 */
    packet = nullptr;    /* ffmpeg 单帧数据包 */
    ioBuf = nullptr;     /* IO Buffer */
    streamType = -1;     /* ffmpeg 流类型 */
}

Decoder::~Decoder() {
    release();
}

void Decoder::release() {
    if (ioBuf) {
        // 释放 IO Buffer
        av_free(ioBuf);
        ioBuf = nullptr;
    }
    if (avioCtx) {
        // 释放 IO 上下文
        avio_context_free(&avioCtx);
        avioCtx = nullptr;
    }
    if (fmtCtx) {
        // 关闭 ffmpeg 上下文
        avformat_close_input(&fmtCtx);
        fmtCtx = nullptr;
    }
    if (codecCtx) {
        // 关闭解码器
        avcodec_free_context(&codecCtx);
//        avcodec_close(codecCtx);
        codecCtx = nullptr;
    }
    if (frame) {
        av_free(frame);
        frame = nullptr;
    }
    if (packet) {
//        // 释放 AVPacket 中的 data
//        av_packet_unref(packet);
        // 释放 AVPacket
        av_packet_free(&packet);
        packet = nullptr;
    }
}

bool Decoder::H265ToJpeg(const char *const inputFilePath, const char *const outputFilePath) {

    if (inputFilePath == nullptr || outputFilePath == nullptr || strlen(inputFilePath) == 0 ||
        strlen(outputFilePath) == 0) {
        LOG("输入或输出的文件路径为空，请核查！输入文件:%s, 输出文件:%s", inputFilePath, outputFilePath);
        return false;
    }

    /**
     * 【1】读取 H265 帧数据
     */

    bool isOk = readH265File(inputFilePath);
    if (!isOk) {
        return false;
    }

    if (DEBUG) {
        LOG("File length=%dB", inputData->size);
    }

    /**
     * 【2】H265 转 Jpeg
     */

    isOk = constructOutputData();
    if (!isOk) {
        return false;
    }

    // 将 H265 数据转换为 Jpeg 数据
    isOk = convert();
    if (!isOk) {
        LOG("%s line=%d | H265 转 Jpeg 出错！", __FUNCTION__, __LINE__);
        return false;
    }

    /**
     * 【3】保存 Jpeg 图片数据
     */

    isOk = writeJpeg2File(outputFilePath);
    if (!isOk) {
        LOG("%s line=%d | 保存 Jpeg 文件出错！Jpeg 文件路径：%s", __FUNCTION__, __LINE__, outputFilePath);
        return false;
    }

    return true;
}

bool Decoder::readH265File(const char *const filePath) {

    /**
     * 【1】先对文件的类型进行合法性检查
     */

    // 获取文件类型后缀名
    const char *fileType = strrchr(filePath, '.');
    if (fileType == nullptr) {
        LOG("%s", "输入文件的后缀名不正确，请检查文件！");
        return false;
    }

    // 源文件类型
    const char *fileTypesList[] = {".h264", ".H264", ".h265", ".H265"};
    int size = sizeof(fileTypesList) / sizeof(fileTypesList[0]);
    bool flag = false;
    int i;
    for (i = 0; i < size; ++i) {
        const char *type = fileTypesList[i];
        if (strcmp(fileType, type) == 0) {
            flag = true;
            break;
        }
    }
    if (!flag) {
        LOG("输入文件不是 h264 或 h265 的文件，请检查文件！");
        return false;
    }

    /**
     * 【2】读取文件到内存
     */

    char *inputBuf = (char *) malloc(HEAP_SIZE * sizeof(char));
    if (!inputBuf) {
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        return false;
    }

    memset(inputBuf, 0, HEAP_SIZE);

    FILE *fp_read = fopen(filePath, "rb+");
    if (!fp_read) {
        LOG("Open file error! filePath=%s, errno=%d", filePath, errno);
        if (inputBuf) {
            free(inputBuf);
            inputBuf = nullptr;
        }
        return false;
    }

    size_t fileLen = fread(inputBuf, 1, HEAP_SIZE, fp_read);
    if (fileLen == 0) {
        LOG("H265 文件为空，请检查文件！H265 文件路径：%s", filePath);
        if (inputBuf) {
            free(inputBuf);
            inputBuf = nullptr;
        }
        fclose(fp_read);
        return false;
    }

//    inputData = (struct Input *) malloc(sizeof(struct Input));
    inputData = std::make_shared<Input>();
    if (!inputData) {
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        if (inputBuf) {
            free(inputBuf);
            inputBuf = nullptr;
        }
        return false;
    }

    inputData->h265_data = inputBuf;
    inputData->offset = 0;
    inputData->size = static_cast<int>(fileLen);

    fclose(fp_read);

    return true;
}

bool Decoder::constructOutputData() {

    char *outputBuf = (char *) malloc(HEAP_SIZE * sizeof(char));
    if (!outputBuf) {
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        return false;
    }

    memset(outputBuf, 0, HEAP_SIZE);

//    outputData = (struct Output *) malloc(sizeof(struct Output));
    outputData = std::make_shared<Output>();
    if (!outputData) {
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        if (outputBuf) {
            free(outputBuf);
            outputBuf = nullptr;
        }
        return false;
    }

    outputData->jpeg_data = outputBuf;
    outputData->offset = 0;

    return true;
}

bool Decoder::convert() {

    // 用于打印错误日志
    char errorBuf[STACK_SIZE];

    // IO Buffer
    ioBuf = (unsigned char *) av_malloc(HEAP_SIZE);
    if (!ioBuf) {
        LOG("%s line=%d | av_malloc failed", __FUNCTION__, __LINE__);
        release();
        return false;
    }

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
    avioCtx = avio_alloc_context(ioBuf, HEAP_SIZE, 0, inputData.get(), readCallback, NULL, NULL);
    if (!avioCtx) {
        LOG("%s line=%d | avio_alloc_context failed.", __FUNCTION__, __LINE__);
        release();
        return false;
    }

    /**
     * AVFormatContext *avformat_alloc_context(void);
     * 初始化 ffmpeg 上下文
     */

    // 初始化 AVFormatContext
    fmtCtx = avformat_alloc_context();
    if (!fmtCtx) {
        LOG("avformat_alloc_context failed.");
        release();
        return false;
    }

    // pb: IO 上下文，通过对该变量进行赋值可改变输入源或输出目的
    fmtCtx->pb = avioCtx;

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
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("%s line=%d | Error in avformat_open_input(), ret=%d, error=%s", __FUNCTION__, __LINE__, ret, errorBuf);
        release();
        return false;
    }

    /**
     * int avformat_find_stream_info(AVFormatContext *ic, AVDictionary **options);
     * 探测码流格式。读取检查媒体文件的数据包以获取具体的流信息，如媒体存入的编码格式。失败返回负值
     *   ic:      媒体文件上下文
     *   options: 额外选项，包含一些配置选项
     */

    ret = avformat_find_stream_info(fmtCtx, NULL);
    if (ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("%s line=%d | Error in find stream, ret=%d, error=%s", __FUNCTION__, __LINE__, ret, errorBuf);
        release();
        return false;
    }

    // 获取视频流的索引
    streamType = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (streamType < 0) {
        LOG("Error in find best stream type, streamType=%d", streamType);
        release();
        return false;
    }

    // 获取流对应的解码器参数
    codecPar = fmtCtx->streams[streamType]->codecpar;

    if (DEBUG) {
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
        LOG("%s line=%d | avcodec_find_decoder failed.", __FUNCTION__, __LINE__);
        release();
        return false;
    }

    /**
     * AVCodecContext *avcodec_alloc_context3(const AVCodec *codec);
     * 申请 AVCodecContext 空间
     */

    codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        LOG("avcodec_alloc_context3 failed.");
        release();
        return false;
    }

    // 替换解码器上下文参数。将视频流信息拷贝到 AVCodecContext 中
    ret = avcodec_parameters_to_context(codecCtx, codecPar);
    if (ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("avcodec_parameters_to_context failed, ret=%d, error=%s", ret, errorBuf);
        release();
        return false;
    }

    if (DEBUG) {
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
    ret = avcodec_open2(codecCtx, codec, NULL);
    if (ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("avcodec_open2 failed, ret=%d, error=%s", ret, errorBuf);
        release();
        return false;
    }

    if (DEBUG) {
        LOG("codecCtx->width=%d, codecCtx->height=%d", codecCtx->width, codecCtx->height);
    }

    // 初始化 AVFrame ，用默认值填充字段
    frame = av_frame_alloc();
    if (!frame) {
        LOG("%s line=%d | Error in allocate the frame", __FUNCTION__, __LINE__);
        release();
        return false;
    }

    /**
     * AVPacket *av_packet_alloc(void)
     * 申请 AVPacket
     */

    packet = av_packet_alloc();

    /**
     * void av_init_packet(AVPacket *pkt);
     * 初始化数据包
     */

    av_init_packet(packet);
    packet->data = nullptr;
    packet->size = 0;

    /**
     * int av_read_frame(AVFormatContext *s, AVPacket *pkt);
     *   s  : 输入的 AVFormatContext
     *   pkt: 输出的 AVPacket
     * 返回流的下一帧。此函数返回存储在文件中的内容，不对有效的帧进行验证，不会省略有效帧之间的无效数据，以便给解码器最大可用于解码的信息。
     * 成功返回 >=0 （>0 是文件末尾），失败返回负值
     */

    // 读取码流数据中的一帧视频帧。从输入文件中读取一个 AVPacket 数据包, 存储到 packet 中。一个 packet 是一帧压缩数据（I + P + P + ...）？
    ret = av_read_frame(fmtCtx, packet);
    if (ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("av_read_frame failed, ret=%d, error=%s", ret, errorBuf);
        release();
        return false;
    }

    if (DEBUG) {
        LOG("packet->pts=%lld", packet->pts);
    }

    if (packet->stream_index == streamType) {                                                 // 读取的数据包类型正确
        /**
         * int avcodec_send_packet(AVCodecContext *avctx, const AVPacket *packet);
         * 将原始分组数据包发送给解码器
         * avctx: 编解码器上下文
         * packet:
         */

        // 将数据包发送到解码器中
        ret = avcodec_send_packet(codecCtx, packet);
        if (ret < 0) {
            av_strerror(ret, errorBuf, STACK_SIZE);
            LOG("Error in the send packet, ret=%d, error=%s", ret, errorBuf);
            release();
            return false;
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
            ret = Encoder().yuv2Jpeg(frame, outputData.get());
            if (ret == 0) {
                LOG("Yuv 编码为 Jpeg 失败！ret=%d", ret);
            }
            break;
        }
    }

    // 注意：此处不能 release()!!! 因为后面还要编码 (将 yuv 编码为 jpeg), 需要用到 frame
//    release();

    return ret;
}

bool Decoder::writeJpeg2File(const char *const filePath) {
    int offset = outputData->offset;
    char *buf = (char *) malloc(offset + 1);
    if (!buf) {
        LOG("%s line=%d | malloc failed.", __FUNCTION__, __LINE__);
        return false;
    }

    memset(buf, 0, offset + 1);
    memcpy(buf, outputData->jpeg_data, offset);

    FILE *fp_write = fopen(filePath, "wb+");
    if (!fp_write) {
        LOG("%s line=%d | Open file error! filePath=%s, errno=%d", __FUNCTION__, __LINE__, filePath, errno);
        if (buf) {
            free(buf);
            buf = nullptr;
        }
        return false;
    }

    size_t ret = fwrite(buf, 1, offset, fp_write);
    if (ret == 0) {
        LOG("%s line=%d | fwrite error! Jpeg 文件路径：%s", __FUNCTION__, __LINE__, filePath);
        fclose(fp_write);
        if (buf) {
            free(buf);
            buf = nullptr;
        }
        return false;
    }

    LOG("保存 Jpeg 数据到文件: %s", filePath);
    fclose(fp_write);

    return true;
}