//
// Created by lixiaoqing on 2021/5/21.
//

#include <mutex>
#include "Decoder.h"
#include "Encoder.h"


// 单例实现的 Decoder 对象
//static Decoder *decoder = nullptr;
//static std::mutex singletonMutex;


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

    printf("%s | %s\n", now, log);
}

std::shared_ptr<IDecoder> IDecoder::getInstance() {
    if (DEBUG) {
        LOG("%s", __PRETTY_FUNCTION__);
    }
    auto decoder = std::make_shared<Decoder>();
    return decoder;
//    // 双检锁
//    if (decoder == nullptr) {
//        std::unique_lock<std::mutex> lock(singletonMutex);
//        if (decoder == nullptr) {
//            decoder = new Decoder();
//        }
//    }
//    return decoder;
}

//void IDecoder::releaseInstance() {
//    if (DEBUG) {
//        LOG("%s", __PRETTY_FUNCTION__);
//    }
//    if (decoder) {
//        delete decoder;
//        decoder = nullptr;
//    }
//}


Decoder::Decoder() {
    LOG("%s", __PRETTY_FUNCTION__);
    fmtCtx = nullptr;    /* ffmpeg 的全局上下文，所有 ffmpeg 都需要 */
    codecCtx = nullptr;  /* ffmpeg 编解码上下文 */
    frame = nullptr;     /* ffmpeg 单帧缓存 */
    packet = nullptr;    /* ffmpeg 单帧数据包 */
}

Decoder::~Decoder() {
    if (DEBUG) {
        LOG("%s", __PRETTY_FUNCTION__);
    }
    // 释放 ffmpeg 资源
    release();
}

void Decoder::release() {
    if (DEBUG) {
        LOG("%s", __PRETTY_FUNCTION__);
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
//        av_free(frame);
        av_frame_free(&frame);
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

    // 合法性检查
    if (inputFilePath == nullptr || outputFilePath == nullptr || strlen(inputFilePath) == 0 ||
        strlen(outputFilePath) == 0) {
        LOG("输入或输出的文件路径为空，请核查！输入文件:%s, 输出文件:%s", inputFilePath, outputFilePath);
        return false;
    }

    // 用于打印错误日志
    char errorBuf[STACK_SIZE];

    /**
     * int avformat_open_input(AVFormatContext **ps, const char *url, ff_const59 AVInputFormat *fmt, AVDictionary **options);
     * 打开输入文件，初始化输入视频码流的 AVFormatContext
     * 成功返回值 >=0 ，失败返回负值
     *   ps:      指向用户提供的 AVFormatContext 指针
     *   url:     要打开的流的 url 。当启用IO模式后（即 fmtCtx->pb 有效）时，此参数无效
     *   fmt:     如果非空，则此参数强制使用特定的输入格式，否则将自动检测格式
     *   options: 附加选项，一般可为 NULL
     */

    int ret = avformat_open_input(&fmtCtx, inputFilePath, nullptr, nullptr);
    if (ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("%s line=%d | Error in avformat_open_input(), ret=%d, error=%s", __PRETTY_FUNCTION__, __LINE__, ret,
            errorBuf);
        release();
        return false;
    }

    /**
     * int avformat_find_stream_info(AVFormatContext *ic, AVDictionary **options);
     * 探测码流格式。读取检查媒体文件的数据包以获取具体的流信息，如媒体存入的编码格式。失败返回负值
     *   ic:      媒体文件上下文
     *   options: 额外选项，包含一些配置选项
     */

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        av_strerror(ret, errorBuf, STACK_SIZE);
        LOG("%s line=%d | Error in find stream, ret=%d, error=%s", __PRETTY_FUNCTION__, __LINE__, ret, errorBuf);
        release();
        return false;
    }

    // 获取视频流的索引
    int streamType = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamType < 0) {
        LOG("Error in find best stream type, streamType=%d", streamType);
        release();
        return false;
    }

    // 获取流对应的解码器参数
    AVCodecParameters * codecPar = fmtCtx->streams[streamType]->codecpar;

    if (DEBUG) {
        struct AVRational base = fmtCtx->streams[streamType]->time_base;
        LOG("时基：num=%d, den=%d", base.num, base.den);
    }

    /**
     * AVCodec *avcodec_find_decoder(enum AVCodecID id);
     * 根据解码器 ID 查找一个匹配的已注册解码器。未找到返回 NULL
     */

    // 对找到的视频流解码器
    AVCodec * codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        LOG("%s line=%d | avcodec_find_decoder failed.", __PRETTY_FUNCTION__, __LINE__);
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
        LOG("%s line=%d | Error in allocate the frame", __PRETTY_FUNCTION__, __LINE__);
        release();
        return false;
    }

    /**
     * AVPacket *av_packet_alloc(void)
     * 申请 AVPacket
     */

    packet = av_packet_alloc();
    if (!packet) {
        LOG("%s line=%d | av_packet_alloc failed.", __PRETTY_FUNCTION__, __LINE__);
        return false;
    }

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

    if (packet->stream_index != streamType) {
        LOG("%s | 读取的数据包类型不正确", __PRETTY_FUNCTION__);
        release();
        return false;
    }

    /**
     * int avcodec_send_packet(AVCodecContext *avctx, const AVPacket *packet);
     * 将原始分组数据包发送给解码器
     * avctx: 编解码器上下文
     * packet:
     */

    // 将数据包发送到解码器中
    ret = avcodec_send_packet(codecCtx, packet);
    av_packet_unref(packet);
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
        ret = Encoder(outputFilePath).yuv2Jpeg(frame);
        if (ret == 0) {
            LOG("Yuv 编码为 Jpeg 失败！ret=%d", ret);
        }
        av_frame_unref(frame);
        break;
    }

    // 释放资源
    release();

    return ret;
}
