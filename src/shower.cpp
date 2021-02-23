// 1 client 显示窗口, 将图片播放出来, 注意频率控制. 
// 2 处理窗口鼠标事件. 
// 
#include <windows.h>
#include <stdio.h>
#include "shower.h"


#ifndef  Assert
#include <assert.h>
#define  Assert assert
#endif


TCHAR     * g_szClassName  = L"mirdc";
TCHAR     * g_szCaption    = L"mirdc";
HINSTANCE   g_hInst        = 0;
HWND        g_hWnd         = 0;
CShower   * g_shower       = 0;
void SaveToBmpFile(const char * file, unsigned char * rgb, int w, int h);


static LONG WINAPI AppWndProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	switch(nMsg) 
	{ 
	case WM_CREATE: 
		g_shower->OnCreate(wParam, lParam);
		return 0;

	case WM_DESTROY: 
		g_shower->OnClose(wParam, lParam);
		return 0; 

	case WM_PAINT: 
		// Paint the window's client area. 
		// --------------------------------------------------------------
		// zcg+, 2012/08/25 *********************************************
		// 当系统发送 WM_PAINT 消息的时候, 如果不使用 BeginPaint 处理的话. 
		// 系统将会重复的发送此消息, 陷入死循环. 
		// 发送 WM_PAINT 消息时, 窗口的 Update Region 是非空的, 当你响应这个
		// 消息的时候, 如果不调用 BeginPaint 去清空. 系统看到 Region 非空就
		// 会重发 WM_PAINT 消息. 
		{
			PAINTSTRUCT paint;
			HDC hDc = ::BeginPaint(hWnd, &paint);

			g_shower->OnPaint(wParam, lParam);
			::EndPaint(hWnd, &paint);
		}
		return 0; 
	case WM_GETMINMAXINFO:
		g_shower->OnGetMinMaxInfo(wParam, lParam);
		return 0;

	case WM_SIZE: 
		g_shower->OnSize(wParam, lParam);
		return 0; 

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_MOUSEMOVE:
	case WM_MOUSEWHEEL:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
		g_shower->OnKeyMouseMsg(nMsg, wParam, lParam);
		return 0;

	default: 
		return (LONG)DefWindowProc(hWnd, nMsg, wParam, lParam); 
	} 

	return 0; 
}

HWND CreateWindowImpl(int w, int h)
{
	int         nRet      = 0;
	const DWORD dwExStyle = 0;
	HWND        hwndApp   = 0;
	int         x = 0,  y = 0;

	// center???
	// 没有标题栏不能移动
	x = CW_USEDEFAULT;  y = CW_USEDEFAULT;

	hwndApp = ::CreateWindowEx(
		dwExStyle, 
		g_szClassName,          // Class name
		g_szCaption,            // Caption
		WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN, // Style bits
		x, y, w, h,             // Position
		(HWND)NULL,             // Parent window (no parent)
		(HMENU)NULL,            // use class menu
		g_hInst,                // handle to window instance
		(LPSTR)NULL             // no params to pass on
		);

	if(hwndApp)
	{
		::ShowWindow(hwndApp, SW_SHOW);
		::UpdateWindow(hwndApp);
	}

	g_hWnd = hwndApp;
	return hwndApp;
}

int  CreateWindowClass()
{
	WNDCLASS     cls   = {0};
	int          nRet  = 0;

	if(!g_hInst)
	{
		g_hInst = (HINSTANCE)GetModuleHandle(0);
		Assert(g_hInst);
	}

	cls.hCursor        = LoadCursor(NULL, IDC_ARROW);
	cls.hIcon          = LoadIcon(g_hInst, TEXT("AMCapIcon"));
	cls.lpszClassName  = g_szClassName;
	cls.hbrBackground  = 0; //(HBRUSH)(COLOR_WINDOW + 1);
	cls.hInstance      = g_hInst;
	cls.style          = CS_BYTEALIGNCLIENT | CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
	cls.lpfnWndProc    = (WNDPROC)AppWndProc;

	if(!RegisterClass(&cls)) return -1;
	return nRet;
}


// =============================================
int  CShower::Init(IShowerCallback * pCall)
{
	int nRet;
	HDC hDc = GetWindowDC(0);

	m_pCall = pCall;
	nRet    = CreateWindowClass(); Assert(!nRet);
	m_nScreenWidth = GetDeviceCaps(hDc, HORZRES); m_nScreenHeight = GetDeviceCaps(hDc, VERTRES);
	ReleaseDC(0, hDc);
	return nRet;
}

int  CShower::GetShowWindowRect(int * w, int * h)
{
	RECT rc = {0};

	GetWindowRect(m_hWnd, &rc);
	*w = (rc.right-rc.left)-m_nClientX;
	*h = (rc.bottom-rc.top)-m_nClientX;

	if(!m_bFullScreen)
	{
		*w -= m_nClientX;
		*h -= m_nClientY;
	}
	Assert(*w > 0 && *h > 0);
	return 0;
}

int  CShower::CreateShowWindow(int w, int h)
{
	int nRet = 0;

	if(m_hWnd) return 0;
	if(!w) w = m_nScreenWidth/2; if(!h) h = m_nScreenHeight/2;
	m_nShowWidth   = w; m_nShowHeight = h;

	m_hWnd = CreateWindowImpl(w, h); Assert(m_hWnd);
	if(!m_hWnd) return -1;

	g_shower = this;
	m_hDc    = GetWindowDC(m_hWnd);
	m_hDcMem = CreateCompatibleDC(m_hDc);
	m_nRun   = 1;
	return nRet;
}

int  CShower::MessageLoop(int nSleepTime)
{
	while(m_nRun)
	{
		if(!MessageOnce())
		{
			m_pCall->OnIdleMsg();
			if(nSleepTime) Sleep(nSleepTime);
		}
	}

	return 0;
}

bool CShower::MessageOnce()
{
	BOOL  bRet = -1;
	MSG   msg;

	//bRet = ::GetMessage(&msg, NULL, 0, 0);
	bRet = ::PeekMessage(&msg, 0, 0, 0, PM_REMOVE);
	if(bRet && bRet != -1 && msg.message != WM_QUIT)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		return true;
	}
	return false;
}

int  CShower::ShowBmp(const char * bmp, int w, int h)
{
	int        nRet = 0;
	BITMAPINFO bmpInfo = {0};

	bmpInfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmpInfo.bmiHeader.biWidth       = w;
	bmpInfo.bmiHeader.biHeight      = h;
	bmpInfo.bmiHeader.biPlanes      = 1;
	bmpInfo.bmiHeader.biBitCount    = 32;
	bmpInfo.bmiHeader.biCompression = BI_RGB;

	if(!m_hDcMem) return 0;
	if(m_nBmpWidth != w || m_nBmpHeight != h)
	{
		m_nBmpWidth = w; m_nBmpHeight = h;
		if(m_hBmpMem) DeleteObject(m_hBmpMem);
		m_hBmpMem = CreateCompatibleBitmap(m_hDc, w, h);
		SelectObject(m_hDcMem, m_hBmpMem);
	}

	{
		CGenericLockHandler  lh(m_lock);
		SetBkColor(m_hDcMem, RGB(255,255,255));
		nRet     = SetDIBitsToDevice(m_hDcMem, 0, 0, w, h, 0, 0, 0, h, bmp, &bmpInfo, DIB_RGB_COLORS); Assert(nRet == h);
	}

	RedrawWindow(m_hWnd, 0, 0, RDW_INVALIDATE);
	return 0;
}


int  CShower::OnCreate(WPARAM wParam, LPARAM lParam)
{
	return 0;
}

int  CShower::OnClose(WPARAM wParam, LPARAM lParam)
{
	if(m_hDcMem)  DeleteDC(m_hDcMem);
	if(m_hDc)     ReleaseDC(m_hWnd, m_hDc);
	if(m_hBmpMem) DeleteObject(m_hBmpMem);
	m_hDc  = 0;   m_hDcMem = 0; m_hBmpMem = 0; m_hWnd = 0;
	m_nBmpWidth = 0; m_nBmpHeight = 0; m_nRun = 0;
	return 0;
}

int  CShower::OnPaint(WPARAM wParam, LPARAM lParam)
{
	RECT  rc = {0};
	int   x  = 0, y = 0;

	if(m_bFullScreen)
	{
		rc.right   = m_nScreenWidth;
		rc.bottom  = m_nScreenHeight;
	}
	else
	{
		GetClientRect(m_hWnd, &rc);
		if(!m_nClientX && !m_nClientY)
		{
			m_nClientX = (m_nShowWidth-rc.right) >> 1;
			m_nClientY = (m_nShowHeight-rc.bottom) -m_nClientX;
		}
		x = m_nClientX; y = m_nClientY;
	}

	CGenericLockHandler  lh(m_lock);
	StretchBlt(m_hDc, x, y, rc.right, rc.bottom, m_hDcMem, 0, 0, m_nBmpWidth, m_nBmpHeight, SRCCOPY);
	return 0;
}

int  CShower::OnGetMinMaxInfo(WPARAM wParam, LPARAM lParam)
{
	LPMINMAXINFO info = (LPMINMAXINFO)lParam;
	info->ptMinTrackSize.x = m_nShowWidth;
	info->ptMinTrackSize.y = m_nShowHeight;
	return 0;
}

int  CShower::OnSize(WPARAM wParam, LPARAM lParam)
{
	int nAction = (int)wParam;
	if(nAction == SIZE_MAXIMIZED)
	{
		LONG nStyle;

		m_bFullScreen = TRUE;
		nStyle = GetWindowLong(m_hWnd, GWL_STYLE);
		SetWindowLong(m_hWnd, GWL_STYLE, nStyle&~WS_CAPTION);
		SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, m_nScreenWidth, m_nScreenHeight, SWP_SHOWWINDOW|SWP_NOREPOSITION|SWP_NOZORDER);
	}
	return 0;
}

int  CShower::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
	WORD wVirtualKey = (WORD)wParam;
	if(m_bFullScreen && wVirtualKey == VK_ESCAPE)
	{
		LONG nStyle;

		m_bFullScreen = FALSE;
		nStyle = GetWindowLong(m_hWnd, GWL_STYLE);
		SetWindowLong(m_hWnd, GWL_STYLE, nStyle|WS_CAPTION);
		ShowWindow(m_hWnd, SW_RESTORE);
	}

	return 0;
}

int  CShower::OnKeyMouseMsg(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	if(nMsg == WM_KEYDOWN) OnKeyDown(wParam, lParam);
	if(m_pCall) return m_pCall->OnKeyMouseMsg(nMsg, wParam, lParam);
	return 0;
}
