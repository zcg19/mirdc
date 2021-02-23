#pragma once


enum enumNetDataType
{
	TypeS_Unknown = 0, 
	TypeS_VideoInfo, 
	TypeS_VideoData, 
	TypeC_ControlData, 
	TypeU_PushFileInfo, 
	TypeU_PushFileData, 
};

typedef struct NetDataHeader_t
{
	char    nMagic[2];   // MS, MC
	char    nType;       // 
	int     nLen;
}NetDataHeader_t;

typedef struct NetS_VideoInfo_t
{
	int     nRgbWidth, nRgbHeight;
	int     nRate;
	int     nCompress;  // 0=no, 1=x264
	float   nFactor;
}NetS_VideoInfo_t;

typedef struct NetU_FileInfo_t
{
	int     nSize;
	wchar_t szName[128];
}NetU_FileInfo_t;


#define DEFAULT_MIRDC_PORT          14285
#define INIT_NET_PACKET_S(_p, _t, _l) \
	(_p)->nMagic[0] = 'M'; (_p)->nMagic[1] = 'S'; (_p)->nType = _t; (_p)->nLen = _l;
#define MAKE_NET_PACKET_INPUT_C(_pkt, _input, _cnt) \
	char  __szBuf[sizeof(NetDataHeader_t)+sizeof(INPUT)*_cnt] = {0}; \
	NetDataHeader_t * _pkt   = (NetDataHeader_t*)__szBuf; \
	INPUT           * _input = (INPUT*)(_pkt+1); \
	_pkt->nMagic[0] = 'M'; _pkt->nMagic[1] = 'C'; \
	_pkt->nType     = TypeC_ControlData; \
	_pkt->nLen      = sizeof(__szBuf);


struct INetCallback
{
public:
	virtual int  OnConnected(void * pUser) = 0;
	virtual int  OnDisconnected(void * pUser) = 0;
	virtual int  OnRecvPacket(NetDataHeader_t * pPacket, void * pUser) = 0;
};

struct INet
{
public:
	virtual int  StartServer(int nPort, INetCallback * pCall, void * pUser) = 0;
	virtual int  StartClient(int nIp, int nPort, INetCallback * pCall, void * pUser) = 0;
	virtual void Stop() = 0;
	virtual int  SendPacket(const NetDataHeader_t * pPacket) = 0;
	virtual bool GetClientInfo(int * nIp, int * nPort) = 0;
};

struct IShowerCallback
{
public:
	virtual int  OnKeyMouseMsg(unsigned int nMsg, WPARAM wParam, LPARAM lParam) = 0;
	virtual int  OnIdleMsg() = 0;
};


INet * CreateNetInstance();
void   DestroyNetInstance(INet * pNet);
