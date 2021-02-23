#pragma once


class CSnapScreen
{
public:
	CSnapScreen()
		: m_hDc(0)
		, m_hDcMem(0)
		, m_hBmpMem(0)
		, m_nWidth(0)
		, m_nHeight(0)
		, m_nBitsPixel(0)
		, m_nBmpSize(0)
		, m_nFactor(1)
	{
		m_nRate   = 100;
	}

	~CSnapScreen()
	{
		Uninit();
	}

	int   GetWidth()  { return m_nWidth; }
	int   GetHeight() { return m_nHeight; }
	int   GetRate()   { return m_nRate; }
	int   GetSize()   { return m_nBmpSize; }
	float GetFactor() { return m_nFactor; }

	int   Init(int nRate)
	{
		m_hDc        = GetWindowDC(0);
		m_nWidth     = GetDeviceCaps(m_hDc, HORZRES);
		m_nHeight    = GetDeviceCaps(m_hDc, VERTRES); 
		m_nBitsPixel = GetDeviceCaps(m_hDc, BITSPIXEL)*GetDeviceCaps(m_hDc, PLANES);
		m_hDcMem     = CreateCompatibleDC(m_hDc);
		m_hBmpMem    = CreateCompatibleBitmap(m_hDc, m_nWidth, m_nHeight);
		SelectObject(m_hDcMem, m_hBmpMem);

		if     (m_nBitsPixel <= 1)  m_nBitsPixel = 1;
		else if(m_nBitsPixel <= 4)  m_nBitsPixel = 4;
		else if(m_nBitsPixel <= 8)  m_nBitsPixel = 8;
		else if(m_nBitsPixel <= 24) m_nBitsPixel = 24;
		else    m_nBitsPixel  = 32;

		m_nRate      = nRate;
		m_nBmpSize   = (m_nWidth*m_nHeight*m_nBitsPixel/8);

		memset(&m_bmpInfo, 0, sizeof(m_bmpInfo));
		m_bmpInfo.biSize   = sizeof(m_bmpInfo);
		m_bmpInfo.biPlanes = 1;  m_bmpInfo.biClrImportant = BI_RGB;
		m_bmpInfo.biWidth  = m_nWidth; m_bmpInfo.biHeight = m_nHeight; m_bmpInfo.biBitCount = m_nBitsPixel;
		printf(">> snap, init -->%d,%d,%d\n", m_nWidth, m_nHeight, m_nBitsPixel);
		return 0;
	}

	void  Uninit()
	{
		if(m_hDc)       ReleaseDC(0, m_hDc);
		if(m_hDcMem)    DeleteObject(m_hDcMem);
		if(m_hBmpMem)   DeleteObject(m_hBmpMem);

		m_hDc = 0; m_hDcMem = 0; m_hBmpMem = 0;
	}

	int   Snap(char * pBmpData, int nSize = 0)
	{
		int  nRet = 0, wd = m_nWidth, hd = m_nHeight; 

		Assert(pBmpData && nSize >= 0);
		nRet = BitBlt(m_hDcMem, 0, 0, wd, hd, m_hDc, 0, 0, SRCCOPY) ? 0:-1;
		if(!nRet) nRet = GetDIBits(m_hDcMem, m_hBmpMem, 0, hd, pBmpData, (BITMAPINFO*)&m_bmpInfo, DIB_RGB_COLORS) == hd ? 0:-2;

		return nRet;
	}


private:
	HDC               m_hDc, m_hDcMem;
	HBITMAP           m_hBmpMem;
	int               m_nWidth, m_nHeight, m_nBitsPixel, m_nBmpSize;
	int               m_nRate;
	float             m_nFactor;

	BITMAPINFOHEADER  m_bmpInfo;
};
