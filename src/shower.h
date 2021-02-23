#pragma once
#include "common/lock.h"
#include "netdata.h"


class CShower
{
public:
	CShower()
		: m_hWnd()
		, m_nScreenWidth(0)
		, m_nScreenHeight(0)
		, m_nShowWidth(0)
		, m_nShowHeight(0)
		, m_nBmpWidth(0)
		, m_nBmpHeight(0)
		, m_nClientX(0)
		, m_nClientY(0)
		, m_hDc(0)
		, m_hDcMem(0)
		, m_hBmpMem(0)
		, m_bFullScreen(FALSE)
		, m_nRun(0)
		, m_pCall(0)
	{}

	virtual ~CShower()
	{
	}

	int  Init(IShowerCallback * pCall);
	int  GetShowWindowRect(int * w, int * h);
	int  CreateShowWindow(int w, int h);
	int  ShowBmp(const char * bmp, int w, int h);
	int  MessageLoop(int nSleepTime = 50);
	bool MessageOnce();

	int  OnCreate(WPARAM wParam, LPARAM lParam);
	int  OnClose(WPARAM wParam, LPARAM lParam);
	int  OnPaint(WPARAM wParam, LPARAM lParam);
	int  OnGetMinMaxInfo(WPARAM wParam, LPARAM lParam);
	int  OnSize(WPARAM wParam, LPARAM lParam);
	int  OnKeyDown(WPARAM wParam, LPARAM lParam);
	int  OnKeyMouseMsg(UINT nMsg, WPARAM wParam, LPARAM lParm);


private:
	HWND                    m_hWnd;
	int                     m_nScreenWidth, m_nScreenHeight;
	int                     m_nShowWidth, m_nShowHeight, m_nClientX, m_nClientY;
	int                     m_nBmpWidth, m_nBmpHeight;
	volatile int            m_nRun;
	HDC                     m_hDc, m_hDcMem;
	HBITMAP                 m_hBmpMem;
	BOOL                    m_bFullScreen;
	CCriticalSetionObject   m_lock;

	IShowerCallback       * m_pCall;
};
