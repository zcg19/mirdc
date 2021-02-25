a simply windows remote desktop control


1 依赖 openh264，进行图片压缩
2 依赖 libyuv进行图片转换成 yuv格式，去掉依赖 libjpeg. 
3 common文件夹下的文件复用 natproxy工程
4 功能较简单，未提供编解码配置。
5 比较懒，界面很简单或者说没有做界面，实际上是一个控制台。

6 图片压缩测试与方案选择：
6.1 1677 x 900的图片bmp格式大概 5M多，设置每秒发送10个图片。
6.2 进行 jpeg极致压缩后一个图片也大于 100K，这样每秒至少需要发送多于 1M的数据。
6.3 使用 openh264编码后第一帧图片大约几十K，这样每秒最大的数据约为几百K。
6.4 因为 ffmpeg的库比较庞大所以使用了 openh264进行编解码。

7 配合natproxy使用的话可以穿透nat，就像teamview。
