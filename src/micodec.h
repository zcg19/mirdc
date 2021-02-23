#pragma once


int  CalcFrameSize_YUV(int nWidth, int nHeight);
int  CalcFrameSize_RGB(int nWidth, int nHeight);
int  ConvertToI420(const char * pSrcFrame, int nFrameLength, int nWidth, int nHeight, char * pDstFrame);
int  ConvertFromI420(const char * pSrcFrame, int nWidth, int nHeight, char * pDstFrame, int nDstFrameSize);

int  ConvertYuv420ToRgb32(const unsigned char * yuv, unsigned char * rgb, int w, int h);
int  ConvertRgb32ToYuv420(const unsigned char * rgb, unsigned char * yuv, int w, int h);


// t: 1=enc, 2=dec
int  InitCodec(int t, int w, int h, int r, float f = 1.0);
void UninitCodec();
int  CompressI420(char * src, int srclen, char * dst, int * dstlen);
int  UnCompressI420(char * src, int srclen, char * dst, int * dstlen);
