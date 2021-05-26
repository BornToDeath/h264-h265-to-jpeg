//
// Created by lixiaoqing on 2021/5/21.
//

#ifndef H265TOJPEG_IDECODER_H
#define H265TOJPEG_IDECODER_H

#include <iostream>
#include <memory>


/**
 * 解码器接口
 */
class IDecoder {

public:

    IDecoder() = default;;

    virtual ~IDecoder() = default;;

    /**
     * 将 H264/H265 解码为 Jpeg
     * @param inputFilePath  输入的 H264/H265 文件路径
     * @param outputFilePath 输出的 Jpeg 文件路径
     * @return
     */
    virtual bool H265ToJpeg(const char *inputFilePath, const char *outputFilePath) = 0;

    /**
     * 获取子类实例。注意：不是单例！
     * @return 子类对象的智能指针
     */
    static std::shared_ptr<IDecoder> getInstance();

    /**
     * 释放单例
     */
//    void releaseInstance();
};

#endif //H265TOJPEG_IDECODER_H
