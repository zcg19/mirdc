// 一个服务器+一个客户端
// 
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <locale.h>
#define  Assert assert

#include "netdata.h"
#include "snapscreen.h"
#include "shower.h"
#include "micodec.h"
#include "rfile.h"
#include "common/simplifythread.h"


class CRemoteDesktopServer : public INetCallback
{
public:
	CRemoteDesktopServer()
		: m_pSrv(0)
		, m_nRunState(0)
	{
		m_pSrv = CreateNetInstance(); Assert(m_pSrv);
		m_rfile.Init(m_pSrv, FALSE);
	}

	~CRemoteDesktopServer()
	{
		if(m_pSrv) DestroyNetInstance(m_pSrv);
	}

	int  Start(int nPort = DEFAULT_MIRDC_PORT)
	{
		// 可以设置的参数有
		// port
		// rate, 
		// factor.
		int nRet;

		nRet   = m_snap.Init(100); Assert(!nRet);
		nRet   = m_pSrv->StartServer(nPort, this, 0); Assert(!nRet);
		printf(">> mirdc server, start -->%d,%d\n", nRet, nPort);
		return nRet;
	}

	void Stop()
	{
		m_nRunState = 0;
		m_thread.SafeStop();
		m_pSrv->Stop();
	}

	int  HaveConnected() { return m_nRunState; }
	CRdcFile * RdcFile() { return &m_rfile; }

	virtual int  OnConnected(void * pUser)
	{
		// 需要保证第一帧的正确性. 
		int nRet, nIp = 0, nPort = 0;

		m_pSrv->GetClientInfo(&nIp, &nPort);
		printf(">> mirdc server, recv a connect query -->(%08x:%d)\n", nIp, nPort);

		Assert(m_nRunState == 0);
		m_thread.SafeStop();
		m_nRunState = 1;

		nRet = m_thread.Start(this, &CRemoteDesktopServer::SnapThreadMain, 0); Assert(!nRet);
		return nRet;
	}

	virtual int  OnDisconnected(void * pUser)
	{
		m_nRunState = 0;
		printf(">> mirdc server, disconnect !!!\n");
		return 0;
	}

	virtual int  OnRecvPacket(NetDataHeader_t * pPacket, void * pUser)
	{
		// 服务器只能收到控制数据
		INPUT * pInput = 0;
		int     nRet, nLen = pPacket->nLen-sizeof(*pPacket);

		if(pPacket->nType > TypeC_ControlData)
		{
			m_rfile.RecvFile(pPacket);
			return 0;
		}

		Assert(pPacket->nType == TypeC_ControlData);
		if(pPacket->nType != TypeC_ControlData || !(nLen > 0 && (nLen%sizeof(INPUT) == 0)))
		{
			Assert(0);
			printf(">> mirdc server, recv a invalidate control data --->%d,%d\n", pPacket->nType, pPacket->nLen);
			return 0;
		}

		pInput = (INPUT*)(pPacket+1);
		for(int i = 0, n = nLen/sizeof(INPUT); i < n; i++)
		{
			nRet   = SendInput(1, &pInput[i], sizeof(INPUT));
			if(!nRet) printf(">> mirdc server, send input failed -->%d,%d\n", pInput->type, GetLastError());
		}
		return 0;
	}


private:
	void SnapThreadMain(void*)
	{
		int    nRet, nRgbSize, nYuvSize, nPktSize, w, h, r;
		char * pRgb = 0, * pYuv = 0, * pPkt = 0, *pRgb2 = 0;
		NetDataHeader_t  * pPacket = 0;

		w = m_snap.GetWidth(); h = m_snap.GetHeight(); r = m_snap.GetRate();
		nRgbSize = m_snap.GetSize(); nYuvSize = CalcFrameSize_YUV(w, h); nPktSize = nYuvSize + sizeof(NetDataHeader_t);
		pRgb     = (char*)malloc(nRgbSize); Assert(pRgb);
		pRgb2    = (char*)malloc(nRgbSize); Assert(pRgb2);
		pYuv     = (char*)malloc(nYuvSize); Assert(pYuv);
		pPkt     = (char*)malloc(nPktSize); Assert(pPkt);
		pPacket  = (NetDataHeader_t*)pPkt;
		INIT_NET_PACKET_S(pPacket, 0, 0);

		nRet = SendVideoInfo();
		nRet = InitCodec(1, w, h, r, m_snap.GetFactor());
		while(m_nRunState)
		{
			int    nLen = 0;
			char * p = 0;

			// 这儿的错误可以恢复. 
			nRet = m_snap.Snap(pRgb, nRgbSize);
			if(nRet) printf(">> rdc server, snap screen error -->%d,%d\n", nRet, GetLastError());

			if(!nRet && memcmp(pRgb, pRgb2, nRgbSize))
			{
				nRet = ConvertToI420(pRgb, nRgbSize, w, h, pYuv); Assert(!nRet);
				nRet = CompressI420(pYuv, nYuvSize, pPkt+sizeof(*pPacket), &nLen); Assert(!nRet);

				if(nLen > 0)
				{
					printf(">> rdc server, send video data -->%d\n", pPacket->nLen);
					pPacket->nType = TypeS_VideoData; pPacket->nLen = nLen+sizeof(*pPacket);
					nRet = m_pSrv->SendPacket(pPacket); Assert(!nRet);
				}

				p = pRgb; pRgb = pRgb2; pRgb2 = p;
			}

			Sleep(r);
		}

		UninitCodec();
		free(pPkt); free(pYuv); free(pRgb);
	}

	int  SendVideoInfo()
	{
		char szBuf[sizeof(NetDataHeader_t)+sizeof(NetS_VideoInfo_t)] = {0};
		NetDataHeader_t  * packet = (NetDataHeader_t*)szBuf;
		NetS_VideoInfo_t * vInfo  = (NetS_VideoInfo_t*)(packet+1);

		INIT_NET_PACKET_S(packet, TypeS_VideoInfo, sizeof(szBuf));
		vInfo->nCompress  = 1;
		vInfo->nRate      = m_snap.GetRate();
		vInfo->nRgbWidth  = m_snap.GetWidth();
		vInfo->nRgbHeight = m_snap.GetHeight();
		vInfo->nFactor    = m_snap.GetFactor();
		return m_pSrv->SendPacket(packet);
	}


private:
	typedef CSimplifyThread<CRemoteDesktopServer> CRdcServerThread;
	CSnapScreen         m_snap;
	CRdcFile            m_rfile;
	INet              * m_pSrv;
	CRdcServerThread    m_thread;
	volatile int        m_nRunState;
};


class CRemoteDesktopClient : public INetCallback , public IShowerCallback
{
public:
	CRemoteDesktopClient()
		: m_pClt(0)
		, m_bRecvInfo(FALSE)
		, m_nWidth(0), m_nHeight(0)
		, m_pRgb(0), m_pYuv(0), m_nRgbSize(0), m_nYuvSize(0)
		, m_nMouseX(0), m_nMouseY(0), m_nKeyDown(0)
		, m_nCount(0)
	{
		int nRet;

		memset(&m_vInfo, 0, sizeof(m_vInfo));
		m_shower.Init(this);

		m_pClt = CreateNetInstance(); Assert(m_pClt);
		m_rfile.Init(m_pClt, TRUE);

		nRet = InitCodec(2, 0, 0, 0); Assert(!nRet);
	}

	~CRemoteDesktopClient()
	{
		if(m_pClt) DestroyNetInstance(m_pClt);
	}

	int  Start(int nIp, int nPort = DEFAULT_MIRDC_PORT)
	{
		int nRet;

		m_bRecvInfo = FALSE;
		nRet = m_shower.CreateShowWindow(0, 0); Assert(!nRet);
		nRet = m_pClt->StartClient(nIp, nPort, this, 0);
		if(nRet)
		{
			printf(">> mirdc client, error to connect srv(%d,%d) --->%08x:%d\n", nRet, GetLastError(), nIp, nPort);
			return nRet;
		}

		return nRet;
	}

	void Stop()
	{
		m_pClt->Stop();
		OnDisconnected(0);
	}

	int  RunLoop()       { return m_shower.MessageLoop(); }
	int  RunOnce()       { return m_shower.MessageOnce(); }
	int  HaveConnected() { return m_pClt->GetClientInfo(0,0) ? 1:0; }
	CRdcFile * RdcFile() { return &m_rfile; }

	virtual int  OnConnected(void * pUser)
	{
		return 0;
	}

	virtual int  OnDisconnected(void * pUser)
	{
		m_bRecvInfo = FALSE;

		if(m_pRgb) free(m_pRgb);
		if(m_pYuv) free(m_pYuv);
		m_pRgb = 0; m_pYuv = 0;

		printf(">> mirdc client, disconnect !!!\n");
		return 0;
	}

	virtual int  OnRecvPacket(NetDataHeader_t * pPacket, void * pUser)
	{
		if(pPacket->nType > TypeC_ControlData)
		{
			m_rfile.RecvFile(pPacket);
			return 0;
		}

		if(pPacket->nType == TypeS_VideoInfo)
		{
			return OnRecvVideoInfo(pPacket);			
		}

		if(!m_bRecvInfo || pPacket->nType != TypeS_VideoData)
		{
			printf(">> mirdc client, invalidate packet, no vinfo or type error(%d,%d)\n", m_bRecvInfo, pPacket->nType);
			return 0;
		}

		int  nRet = 0, nLen = m_nYuvSize;
		Assert(pPacket->nLen > sizeof(*pPacket));
		Assert(m_pYuv && m_pRgb);

		//printf(">> mirdc client, recv packet -->%d\n", pPacket->nLen);
		nRet = UnCompressI420((char*)(pPacket+1), pPacket->nLen-sizeof(*pPacket), m_pYuv, &nLen);
		if(!nRet) nRet = ConvertFromI420(m_pYuv, m_nWidth, m_nHeight, m_pRgb, m_nRgbSize);
		if(nRet)
		{
			// 不能恢复所以断开连接 ???
			printf(">> mirdc client, uncompress data error -->%d,%d,%d\n", nRet, pPacket->nLen, m_nCount);
			return -1;
		}

		m_nCount++;
		return OnRecvVideoData();
	}

	virtual int  OnIdleMsg()
	{
		return 0;
	}

	virtual int  OnKeyMouseMsg(unsigned int nMsg, WPARAM wParam, LPARAM lParam)
	{
		if(!m_bRecvInfo) return 0;

		switch(nMsg)
		{
		case WM_KEYDOWN:        return OnKeyEvent(nMsg, wParam, lParam);
		case WM_KEYUP:          return OnKeyEvent(nMsg, wParam, lParam);
		case WM_MOUSEMOVE:      return OnMouseMove(wParam, lParam);
		case WM_MOUSEWHEEL:     return OnMouseWheel(wParam, lParam);
		case WM_LBUTTONDOWN:    return OnButtonClickEvent(nMsg, wParam, lParam);
		case WM_LBUTTONUP:      return OnButtonClickEvent(nMsg, wParam, lParam);
		case WM_LBUTTONDBLCLK:  return OnButtonDBClickEvent(nMsg, wParam, lParam);
		case WM_RBUTTONDOWN:    return OnButtonClickEvent(nMsg, wParam, lParam);
		case WM_RBUTTONUP:      return OnButtonClickEvent(nMsg, wParam, lParam);
		case WM_RBUTTONDBLCLK:  return OnButtonDBClickEvent(nMsg, wParam, lParam);
		case WM_MBUTTONDOWN:    return OnButtonClickEvent(nMsg, wParam, lParam);
		case WM_MBUTTONUP:      return OnButtonClickEvent(nMsg, wParam, lParam);
		case WM_MBUTTONDBLCLK:  return OnButtonDBClickEvent(nMsg, wParam, lParam);
		}
		return 0;
	}


private:
	int  OnRecvVideoInfo(NetDataHeader_t * pPacket)
	{
		if(m_bRecvInfo)
		{
			printf(">> mirdc client, invalidate packet, double videoinfo!!!\n");
			return -1;
		}

		if(pPacket->nLen == sizeof(m_vInfo)+sizeof(*pPacket))
		{
			memcpy(&m_vInfo, pPacket+1, sizeof(m_vInfo));
		}

		if(!m_vInfo.nRgbHeight || !m_vInfo.nRgbWidth || !m_vInfo.nFactor || !m_vInfo.nRate)
		{
			printf(">> mirdc client, invalidate packet, videoinfo!\n");
			return -1;
		}

		m_bRecvInfo = TRUE;
		m_nWidth    = (int)(m_vInfo.nRgbWidth*m_vInfo.nFactor);
		m_nHeight   = (int)(m_vInfo.nRgbHeight*m_vInfo.nFactor);
		m_nYuvSize  = CalcFrameSize_YUV(m_nWidth, m_nHeight);
		m_nRgbSize  = m_nWidth*m_nHeight*4;
		m_pRgb      = (char*)malloc(m_nRgbSize); Assert(m_pRgb);
		m_pYuv      = (char*)malloc(m_nYuvSize); Assert(m_pYuv);

		printf(">> mirdc client, conn server ok, vinfo -->%d,%d, %f,%d, %d\n", m_vInfo.nRgbWidth, m_vInfo.nRgbHeight, m_vInfo.nFactor, m_vInfo.nRate, m_vInfo.nCompress);
		return (m_pRgb && m_pYuv) ? 0 : -1;
	}

	int  OnRecvVideoData()
	{
		int nRet;

		nRet = m_shower.ShowBmp(m_pRgb, m_nWidth, m_nHeight); Assert(!nRet);
		return nRet;
	}


	int  OnMouseMove(WPARAM wParam, LPARAM lParam)
	{
		MAKE_NET_PACKET_INPUT_C(packet, input, 1);
		int x = LOWORD(lParam), y = HIWORD(lParam);
		int w = 0, h = 0;

		if(m_nMouseX == x && m_nMouseY == x) return 0;
		m_nMouseX = x; m_nMouseY = y;

		// 注意这儿是相对于(65535, 65535), 而不是(m_vInfo.nRgbWidth, m_vInfo.nRgbHeight). 
		m_shower.GetShowWindowRect(&w, &h);
		input->type       = INPUT_MOUSE;
		input->mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
		input->mi.dx      = (x*65535)/w;
		input->mi.dy      = (y*65535)/h;

		//int rx = (x*m_vInfo.nRgbWidth)/w, ry = (y*m_vInfo.nRgbHeight)/h;
		//printf(">> mirdc client, mouse move -->%d,%d\n", rx, ry);
		return m_pClt->SendPacket(packet);
	}

	int  OnMouseWheel(WPARAM wParam, LPARAM lParam)
	{
		DWORD mData = HIWORD(wParam);
		MAKE_NET_PACKET_INPUT_C(packet, input, 1);

		input->type         = INPUT_MOUSE;
		input->mi.dwFlags   = MOUSEEVENTF_WHEEL;
		input->mi.mouseData = mData;

		return m_pClt->SendPacket(packet);
	}

	int  OnKeyEvent(int nMsg, WPARAM wParam, LPARAM lParam)
	{
		WORD nKey = (WORD)wParam;
		MAKE_NET_PACKET_INPUT_C(packet, input, 1);

		// 1 不能过滤 WM_KEYDOWN键, 例如 ->键. 
		// 2 不能响应 w+r键, 因为按 w键后窗口焦点会切换. 导致 r键不能截获到. 
		// 3 shift+方向键不能响应, 不知道为什么
		// 4 VM虚拟机里按了ctrl+alt键后, 就不能在记事本中再写入字了, 
		//   因为 ctrl+alt键被VM虚拟机用来释放虚拟机焦点了. 导致 alt键弹起事件不能获取到. 
		// **通常键不起作用是因为导致了焦点丢失. 
		input->type    = INPUT_KEYBOARD;
		input->ki.wVk  = nKey;
		if(nMsg == WM_KEYUP) input->ki.dwFlags = KEYEVENTF_KEYUP;

		return m_pClt->SendPacket(packet);
	}

	int  OnButtonClickEvent(int nMsg, WPARAM wParam, LPARAM lParam)
	{
		MAKE_NET_PACKET_INPUT_C(packet, input, 1);

		input->type       = INPUT_MOUSE;
		switch(nMsg)
		{
		case WM_LBUTTONDOWN: input->mi.dwFlags = MOUSEEVENTF_LEFTDOWN;   break;
		case WM_LBUTTONUP:   input->mi.dwFlags = MOUSEEVENTF_LEFTUP;     break;
		case WM_RBUTTONDOWN: input->mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;  break;
		case WM_RBUTTONUP:   input->mi.dwFlags = MOUSEEVENTF_RIGHTUP;    break;
		case WM_MBUTTONDOWN: input->mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN; break;
		case WM_MBUTTONUP:   input->mi.dwFlags = MOUSEEVENTF_MIDDLEUP;   break;
		default:             Assert(0); break;
		}

		return m_pClt->SendPacket(packet);
	}

	int  OnButtonDBClickEvent(int nMsg, WPARAM wParam, LPARAM lParam)
	{
		DWORD dwFlags1 = 0, dwFlags2 = 0;
		MAKE_NET_PACKET_INPUT_C(packet,  input, 4);

		switch(nMsg)
		{
		case WM_LBUTTONDBLCLK: dwFlags1 = MOUSEEVENTF_LEFTDOWN;   dwFlags2 = MOUSEEVENTF_LEFTUP;   break;
		case WM_RBUTTONDBLCLK: dwFlags1 = MOUSEEVENTF_RIGHTDOWN;  dwFlags2 = MOUSEEVENTF_RIGHTUP;  break;
		case WM_MBUTTONDBLCLK: dwFlags1 = MOUSEEVENTF_MIDDLEDOWN; dwFlags2 = MOUSEEVENTF_MIDDLEUP; break;
		default:               Assert(0); break;
		}

		input[0].type       = INPUT_MOUSE; input[1].type       = INPUT_MOUSE;
		input[2].type       = INPUT_MOUSE; input[3].type       = INPUT_MOUSE;
		input[0].mi.dwFlags = dwFlags1;    input[1].mi.dwFlags = dwFlags2;
		input[2].mi.dwFlags = dwFlags1;	   input[3].mi.dwFlags = dwFlags2;

		return m_pClt->SendPacket(packet);
	}


private:
	CShower             m_shower;
	CRdcFile            m_rfile;
	INet              * m_pClt;
	NetS_VideoInfo_t    m_vInfo;
	BOOL                m_bRecvInfo;
	char              * m_pRgb, * m_pYuv;
	int                 m_nRgbSize, m_nYuvSize; 
	int                 m_nWidth, m_nHeight, m_nCount;
	int                 m_nMouseX, m_nMouseY, m_nKeyDown;
};


void  RdcMain(int argc, char ** argv)
{
	int    nRet0, nClient = 0, nFile = 0;

	CRemoteDesktopServer rds;
	CRemoteDesktopClient rdc;


	::setlocale(LC_ALL, "");
	nRet0 = rds.Start();
	printf(">> main, start server --->%d\n", nRet0);


	while(1)
	{
		int         nRet = 0, nExit = 0, nCount = 0;
		wchar_t     ch = 0,   szBuf[1024] = {0};
		CRdcFile  * pFile = 0;

		if(nClient) nRet   = rdc.RunOnce();
		if(nRet)    continue;

		// 判断是否有连接
		pFile = rdc.HaveConnected() ? rdc.RdcFile() : rds.RdcFile();
		if(!nFile)  nCount = pFile->GetKeyEvent(szBuf, _countof(szBuf));

		if(nCount == 1)
		{
			ch = szBuf[0];
			printf("%c\n", ch);
		}

		if(nCount > 3)
		{
			nRet = pFile->OpenFile(szBuf);
			printf(">> main, push file -->%d:%S\n", nRet, szBuf);
			if(!nRet) nFile = 1; 
		}

		if(nFile == 1)
		{
			if(pFile->SendFile())
			{
				nFile = 0;
				printf(">> main, push file ok!\n");
			}
		}

		switch(ch)
		{
		case 'h':
			printf(">> main, option -->c, q\n");
			break;
		case 'q':
			{
				nExit = 1;
				rds.Stop();
				rdc.Stop();
			}
			break;
		case 'c': 
			{
				int  nIp = 0;

				if(rdc.HaveConnected() || rds.HaveConnected())
				{
					printf(">> main, only a server(%d) or a client(%d) connection!!!\n", rds.HaveConnected(), rdc.HaveConnected());
					break;
				}

				while(!nIp || nIp == -1)
				{
					printf(">> connect ip: "); 
					fflush(stdin); scanf("%s", (char*)szBuf);
					nIp = inet_addr((char*)szBuf);
				}

				nRet = rdc.Start(nIp);
				if(!nRet) nClient = 1;
				printf(">> main, start client -->%d:%s\n", nRet, szBuf);
			}
			break;
		}

		if(nExit) break;
		Sleep(50);
	}
}
