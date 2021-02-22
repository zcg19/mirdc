a simply windows remote desktop


1 依赖 openh264，进行图片压缩
2 依赖 webrtc中的 libjpeg和 libyuv进行图片转换成 yuv格式. 
  实际上可以去掉 libjpeg库，并未使用. 
3 功能较简单，并未提供编解码配置。

4 图片压缩测试与方案选择：
4.1 1677 x 900的图片bmp格式大概 5M多，设置每秒发送10个图片。
4.2 进行 jpeg极致压缩后一个大于 100K，这样每秒需要发送 1M的数据。
4.3 使用 openh264编码后第一帧图片大约几十K，这样每秒最大的数据约为几百K。
4.4 因为 ffmpeg的库比较庞大所以使用了 openh264进行编解码。
