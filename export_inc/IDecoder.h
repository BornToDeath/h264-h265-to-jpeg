//
// Created by lixiaoqing on 2021/5/21.
//

#ifndef H265TOJPEG_IDECODER_H
#define H265TOJPEG_IDECODER_H


/**
 * 解码器接口
 */
class IDecoder {

public:

    virtual ~IDecoder() = default;

    /**
     * 将 H264/H265 解码为 Jpeg
     * @param inputFilePath  输入的 H264/H265 文件路径
     * @param outputFilePath 输出的 Jpeg 文件路径
     * @return
     */
    virtual bool H265ToJpeg(const char *const inputFilePath, const char *const outputFilePath) = 0;

    /**
     * 单例
     * @return
     */
    static IDecoder *getInstance();

    /**
     * 释放单例
     */
    void release();
};

#endif //H265TOJPEG_IDECODER_H
