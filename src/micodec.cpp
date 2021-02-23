#include "micodec.h"
#include "libyuv.h"
#include "openh264/codec/api/svc/codec_api.h"

#include <stdio.h>
#include <memory.h>
#include <assert.h>
#define  Assert assert


#define  __USE_LIBYUV__
#ifdef   _DEBUG
#pragma  comment(lib, "lib/libyuv.x86.dbg.lib")
#else
#pragma  comment(lib, "lib/libyuv.x86.lib")
#endif
#pragma  comment(lib, "lib/welsenc.x86.dbg.lib")
#pragma  comment(lib, "lib/welsdec.x86.dbg.lib")


int  CalcFrameSize_YUV(int nWidth, int nHeight)
{
	int nSize = 0;
	int nHalfWidth  = (nWidth + 1) >> 1;
	int nHalfHeight = (nHeight + 1) >> 1;

	nSize = nWidth * nHeight + nHalfWidth * nHalfHeight * 2;
	return nSize;
}

int  CalcFrameSize_RGB(int nWidth, int nHeight)
{
	return nWidth * nHeight * 4;
}


#ifdef __USE_LIBYUV__
int  ConvertToI420(const char * pSrcFrame, int nFrameLength, int nWidth, int nHeight, char * pDstFrame)
{
	uint8 * pDstY, *pDstU, *pDstV;
	int     nDstStride = nWidth, nAbsHeight, nHalfWidth, nHalfHeight;
	libyuv::RotationMode nRotation = libyuv::kRotate0;

	nAbsHeight = nHeight > 0 ? nHeight : -nHeight;
	nHalfWidth = (nWidth + 1) >> 1;
	nHalfHeight = (nAbsHeight + 1) >> 1;

	pDstY = (uint8*)pDstFrame;
	pDstU = pDstY + nWidth * nAbsHeight;
	pDstV = pDstU + nHalfWidth * nHalfHeight;

	return libyuv::ConvertToI420
	(
		(const uint8*)pSrcFrame, nFrameLength,
		pDstY, nDstStride,
		pDstU, (nDstStride + 1) / 2,
		pDstV, (nDstStride + 1) / 2,
		0, 0,
		nWidth, nHeight, nWidth, nHeight,
		nRotation,
		libyuv::FOURCC_ARGB
	);
}

int  ConvertFromI420(const char * pSrcFrame, int nWidth, int nHeight, char * pDstFrame, int nDstFrameSize)
{
	uint8 * pSrcY, *pSrcU, *pSrcV;
	int     nSrcStride = nWidth, nAbsHeight, nHalfWidth, nHalfHeight;

	nAbsHeight = nHeight > 0 ? nHeight : -nHeight;
	nHalfWidth = (nWidth + 1) >> 1;
	nHalfHeight = (nAbsHeight + 1) >> 1;

	pSrcY = (uint8*)pSrcFrame;
	pSrcU = pSrcY + nWidth * nAbsHeight;
	pSrcV = pSrcU + nHalfWidth * nHalfHeight;

	return libyuv::ConvertFromI420
	(
		pSrcY, nSrcStride,
		pSrcU, (nSrcStride + 1) / 2,
		pSrcV, (nSrcStride + 1) / 2,
		(uint8*)pDstFrame, 0,
		nWidth, nHeight,
		libyuv::FOURCC_ARGB
	);
}


#else
int  ConvertToI420(const char * pSrcFrame, int nFrameLength, int nWidth, int nHeight, char * pDstFrame)
{
	return ConvertRgb32ToYuv420((const unsigned char*)pSrcFrame, (unsigned char*)pDstFrame, nWidth, nHeight);
}

int  ConvertFromI420(const char * pSrcFrame, int nWidth, int nHeight, char * pDstFrame, int nDstFrameSize)
{
	return ConvertYuv420ToRgb32((unsigned char*)pSrcFrame, (unsigned char*)pDstFrame, nWidth, nHeight);
}


#endif


struct Codec264_t
{
	int           w, h, r, id;
	ISVCEncoder * enc;
	ISVCDecoder * dec;
}g_codec = {0};

static int  InitWelsEncoder(int w, int h, int r, float f)
{
	int              nRet, nImgSize = 0;
	ISVCEncoder    * pEncoder = 0;
	SEncParamBase    param;

	// default: 
	// r = 100, f = 0.8
	nRet = WelsCreateSVCEncoder(&pEncoder); Assert(!nRet && pEncoder);
	param.iUsageType     = SCREEN_CONTENT_REAL_TIME;
	param.iPicWidth      = (int)(w*f);
	param.iPicHeight     = (int)(h*f);
	param.iRCMode        = RC_QUALITY_MODE;
	param.fMaxFrameRate  = (float)r;
	param.iTargetBitrate = 2500000;
	nRet = pEncoder->Initialize(&param); Assert(!nRet);

	g_codec.w = w; g_codec.h = h; g_codec.r = r;
	g_codec.enc = pEncoder;
	printf(">> init codec -->%d,%d,%d,%f\n", w, h, r, f);
	return nRet;
}

static int  InitWelsDecoder()
{
	int              nRet, nThreadCount = 0;
	ISVCDecoder    * pDecoder = 0;
	SDecodingParam   param = {0};

	nRet = WelsCreateDecoder(&pDecoder); Assert(!nRet && pDecoder);
	pDecoder->SetOption(DECODER_OPTION_NUM_OF_THREADS, &nThreadCount);

	param.sVideoProperty.size = sizeof(param.sVideoProperty);
	param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
	param.eEcActiveIdc        = ERROR_CON_SLICE_COPY;
	param.uiTargetDqLayer     = (unsigned char)-1;
	nRet = pDecoder->Initialize(&param); Assert(!nRet);

	g_codec.dec = pDecoder;
	return nRet;
}

int  InitCodec(int t, int w, int h, int r, float f)
{
	memset(&g_codec, 0, sizeof(g_codec));

	if(t&1) InitWelsEncoder(w, h, r, f);
	if(t&2) InitWelsDecoder();

	return 0;
}

void UninitCodec()
{
	if(g_codec.enc)
	{
		g_codec.enc->Uninitialize();
		WelsDestroySVCEncoder(g_codec.enc);
		g_codec.enc = 0;
	}
	if(g_codec.dec)
	{
		g_codec.dec->Uninitialize();
		WelsDestroyDecoder(g_codec.dec);
		g_codec.dec = 0;
	}
}

int  CompressI420(char * src, int srclen, char * dst, int * dstlen)
{
	int              nRet, off = 0, w = g_codec.w, h = g_codec.h;
	SSourcePicture   spic  = {0};
	SFrameBSInfo     dframe = {0};

	spic.iColorFormat     = videoFormatI420;
	spic.iPicWidth        = w;
	spic.iPicHeight       = h;
	spic.iStride[0]       = w; 
	spic.iStride[1]       = spic.iStride[2] = w>>1;
	spic.pData[0]         = (unsigned char*)src; 
	spic.pData[1]         = spic.pData[0] + (w*h); 
	spic.pData[2]         = spic.pData[1] + (w*h>>2);
	spic.uiTimeStamp      = g_codec.id++/* * (1000/g_codec.r)*/;

	Assert(g_codec.enc);
	nRet = g_codec.enc->EncodeFrame(&spic, &dframe); Assert(nRet == cmResultSuccess);
	if(nRet == cmResultSuccess && dframe.iFrameSizeInBytes > 0)
	{
		for(int j = 0; j < dframe.iLayerNum; j++)
		{
			int             nSize = 0;
			SLayerBSInfo  * pLayerInfo = &dframe.sLayerInfo[j];
			unsigned char * pData = pLayerInfo->pBsBuf;

			for(int k = 0; k < pLayerInfo->iNalCount; k++)
			{
				memcpy(dst+off, &pLayerInfo->pNalLengthInByte[k], 4);  off += 4;
				memcpy(dst+off, pData, pLayerInfo->pNalLengthInByte[k]); off += pLayerInfo->pNalLengthInByte[k]; pData += pLayerInfo->pNalLengthInByte[k];
			}
		}
	}

	*dstlen = off;
	return nRet;
}

int  UnCompressI420(char * src, int srclen, char * dst, int * dstlen)
{
	int              nRet, nLen, off = 0;
	unsigned char  * pImgYuv[3] = {0};
	SBufferInfo      dstInfo = {0};

	Assert(g_codec.dec && srclen > 0);
	while(1)
	{
		memset(&dstInfo, 0, sizeof(dstInfo)); memset(pImgYuv, 0, sizeof(pImgYuv));
		nLen = *(int*)src; src += 4; Assert(nLen+4 <= srclen);
		nRet = g_codec.dec->DecodeFrameNoDelay((unsigned char *)src+1, nLen-1, pImgYuv, &dstInfo);
		if(dstInfo.iBufferStatus == 1)
		{
			int          w, h, s_0, s_1;
			unsigned char * pData = pImgYuv[0];

			w   = dstInfo.UsrData.sSystemBuffer.iWidth; h = dstInfo.UsrData.sSystemBuffer.iHeight;
			s_0 = dstInfo.UsrData.sSystemBuffer.iStride[0]; s_1 = dstInfo.UsrData.sSystemBuffer.iStride[1];
			for(int i = 0; i < h; i++) { memcpy(dst+off, pData, w); off += w; pData += s_0; }
			pData = pImgYuv[1], w = w/2; h = h/2;
			for(int i = 0; i < h; i++) { memcpy(dst+off, pData, w); off += w; pData += s_1; }
			pData = pImgYuv[2];
			for(int i = 0; i < h; i++) { memcpy(dst+off, pData, w); off += w; pData += s_1; }
		}
		src += nLen; srclen -= nLen+4; 
		if(srclen <= 0) break;
	}

	*dstlen = off;
	return nRet;
}


// -----------------------------------------------------------------------------
// zcg+
template <typename TYPE>
inline void Swap(TYPE & a, TYPE & b)
{
	TYPE t = a; a = b; b = t;
}

inline unsigned char ConvertRgbAdjust(double v)
{
	return (unsigned char)((v >= 0 && v <= 255) ? v : (v < 0 ? 0 : 255));
}

int  ConvertRgb32ToYuv420(const unsigned char * rgb, unsigned char * yuv, int w, int h)
{
	unsigned char * yuv_y = yuv;
	unsigned char * yuv_u = yuv_y + w * h;
	unsigned char * yuv_v = yuv_u + ((w * h) >> 2);

	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			unsigned char y, u, v, r, g, b;
			int off = 0;

			off = w * 4 * i + j * 4;
			b   = rgb[off + 0];
			g   = rgb[off + 1];
			r   = rgb[off + 2];

			y   = ConvertRgbAdjust((66 * r + 129 * g + 25 * b + 0x1080) >> 8);
			v   = ConvertRgbAdjust((112 * b - 74 * g - 38 * r + 0x8080) >> 8);
			u   = ConvertRgbAdjust((112 * r - 94 * g - 18 * b + 0x8080) >> 8);

			off = (i >> 1) * (w >> 1) + (j >> 1);
			yuv_y[i * w + j] = y;
			yuv_v[off] = v;
			yuv_u[off] = u;
		}
	}

	return 0;
}

int  ConvertYuv420ToRgb32(const unsigned char * yuv, unsigned char * rgb, int w, int h)
{
	const unsigned char * yuv_y = yuv;
	const unsigned char * yuv_u = yuv_y + w * h;
	const unsigned char * yuv_v = yuv_u + ((w * h) >> 2);

	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			unsigned char y, u, v, r, g, b;
			int off = 0;

			off = (i >> 1) * (w >> 1) + (j >> 1);
			y   = yuv_y[i * w + j];
			v   = yuv_v[off];
			u   = yuv_u[off];

			r   = ConvertRgbAdjust(y + (1.4075 * (v - 128)));
			g   = ConvertRgbAdjust(y - (0.3455 * (u - 128) - 0.7169 * (v - 128)));
			b   = ConvertRgbAdjust(y + (1.7790 * (u - 128)));

			off = w * 4 * i + j * 4;
			rgb[off + 0] = b;
			rgb[off + 1] = g;
			rgb[off + 2] = r;
			rgb[off + 3] = 0xff;
		}
	}
	return 0;
}
