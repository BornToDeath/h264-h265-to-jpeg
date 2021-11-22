# ffmpeg 库文件说明

# 更新日志

## 20211119 更新

测试发现此目录下的mac平台静态库和动态库都不可使用。如果不想重新编译可以这样做：
1. mac命令行安装ffmpeg工具。
2. 找到安装之后的ffmpeg路径。在我的电脑是：`/usr/local/Cellar/ffmpeg/4.4_2` 。
3. 找到目录下的库文件，直接使用，即可。

## ~~original~~

经测试，只有动态库才可以使用，即 `mac_shared` 和 `x86_64_shared` 下的 ffmpeg 库文件。