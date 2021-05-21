# H264/H265 转 Jpeg 

## 项目简介

此项目是 H264/H265 转 Jpeg 的源码，可支持将 H264 或 H265 格式的图片文件转码为 Jpeg 格式的图片文件。

转码流程如下：

1. 解码：将 H264/H265 解码为 YUV。
2. 编码：将 YUV 编码为 Jpeg。

## 项目结构

项目结构如下：

`export_inc`: 对外暴露的 H265 转 Jpeg 的头文件

`lib`: ffmpeg 库文件

`src`: H265 转 Jpeg 相关的源文件和头文件

`test`: 测试文件

`CMakeLists.txt`: CMakeLists 文件

`main.cpp`: 测试代码 

## 参考

此项目参考资源如下：

1. [h265ToJpeg](https://github.com/lucish/h265ToJpeg)
2. [ffmpeg-build-script](https://github.com/markus-perl/ffmpeg-build-script/)
