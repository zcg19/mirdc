// 服务器和客户端, 
// 1 仅允许一个客户端, 多个客户端会冲突. 
// 
#include <stdio.h>
#include "common/xsocket.h"
#include "common/simplifythread.h"
#include "netdata.h"


class CNet : public INet
{
public:
	CNet()
		: m_nRunState(0)
		, m_nConnectState(0)
		, m_pCall(0)
		, m_pUser(0)
	{
		int nRet;
		nRet = InitSocket(); Assert(!nRet);
	}

	virtual ~CNet()
	{
	}

	virtual int  StartServer(int nPort, INetCallback * pCall, void * pUser)
	{
		int nRet;

		m_pCall = pCall; m_pUser = pUser;
		nRet = m_server.Listen(0, nPort); Assert(!nRet);

		m_nRunState = 1;
		nRet = m_thread.Start(this, &CNet::WorkThreadMain, (void*)1); Assert(!nRet);
		return nRet;
	}

	virtual int  StartClient(int nIp, int nPort, INetCallback * pCall, void * pUser)
	{
		int  nRet;

		m_pCall = pCall; m_pUser = pUser;
		nRet = m_client.Connect(nIp, nPort);
		if(nRet) return nRet;

		m_nRunState = 1;
		m_pCall->OnConnected(m_pUser);

		m_thread.SafeStop();
		nRet = m_thread.Start(this, &CNet::WorkThreadMain, (void*)0); Assert(!nRet);
		return nRet;
	}

	virtual bool GetClientInfo(int * nIp, int * nPort)
	{
		m_client.GetSockAddr();

		if(nIp)   *nIp   = m_client.RemoteIp();
		if(nPort) *nPort = m_client.RemotePort();
		return m_client.IsConnected() ? true:false;
	}

	virtual void Stop()
	{
		m_nRunState = 0;
		m_client.Close();
		m_server.Close();
		m_thread.SafeStop();
	}

	virtual int  SendPacket(const NetDataHeader_t * pPacket)
	{
		if(InterlockedCompareExchange(&m_nConnectState, 0, 0) <= 0) return 0;

		// 因为没有加锁, 可能会造成发送失败. 
		Assert(pPacket->nLen > 0);
		m_client.SendAll((const char*)pPacket, pPacket->nLen);
		return 0;
	}


private:
	void WorkThreadMain(void * param)
	{
		int    nRet  = 0, nSize = 1 * 1024 * 1024;
		char * szBuf = (char*)malloc(nSize); Assert(szBuf);

		while(m_nRunState > 0)
		{
			if(param)
			{
				CXdSocket::State_t state = {0};
				sockaddr_in        addr = {0};
				SOCKET             s = 0;

				nRet = m_server.Accept(&s, &addr);
				if(nRet) break;

				state.socket = 1; state.connect = 1;
				m_client.Attach(s, state);
				m_pCall->OnConnected(m_pUser);
			}

			InterlockedIncrement(&m_nConnectState);

			HandleNetData(szBuf, nSize);
			m_pCall->OnDisconnected(m_pUser);

			InterlockedDecrement(&m_nConnectState);

			m_client.Close();
			if(!param) break;
		}

		free(szBuf);
	}

	void HandleNetData(char * szBuf, int nSize)
	{
		int  nRet, nLen = 0;

		while(1)
		{
			NetDataHeader_t * pPacket = (NetDataHeader_t*)szBuf;

			Assert(nLen >= 0);
			nRet = RecvPacket(pPacket, nSize, &nLen);
			if(nRet) break;

			Assert(pPacket->nLen <= nLen);
			while(nLen > sizeof(*pPacket) && nLen >= pPacket->nLen)
			{
				nRet    = m_pCall->OnRecvPacket(pPacket, m_pUser);
				if(nRet) break;

				nLen   -= pPacket->nLen;
				pPacket = (NetDataHeader_t*)(((char*)pPacket)+pPacket->nLen);
			}

			if(nRet) break;
			Assert(nLen >= 0);
			if(nLen > 0) memmove(szBuf, pPacket, nLen);
		}
	}

	BOOL IsValidPacket(NetDataHeader_t * pPacket)
	{
		BOOL bIsServer = m_server.Socket() != 0;
		if(pPacket->nLen < sizeof(*pPacket)) return FALSE;
		if(pPacket->nMagic[0] != 'M') return FALSE;
		if(pPacket->nMagic[1] != (bIsServer ? 'C':'S')) return FALSE;
		if(pPacket->nType == 0) return FALSE;

		return TRUE;
	}

	int  RecvPacket(NetDataHeader_t * pPacket, int nSize, int * nLen)
	{
		int    sz = nSize, off = *nLen, ret = 0;
		char * p  = (char*)pPacket;

		sz -= off; p += off;
		while(1)
		{
			int len = 0;

			ret = m_client.Recv(p, sz, &len);
			if(ret)
			{
				printf(">> net, error to recv -->(%d,%d),%d,%d\n", sz, len, ret, GetLastError());
				return ret;
			}

			off += len; p += len; sz -= len;
			Assert(len > sizeof(*pPacket));
			Assert(pPacket->nLen < nSize);

			if(!IsValidPacket(pPacket))
			{
				Assert(0);
				return -8;
			}

			if(off > 0 && pPacket->nLen <= off) break;
		}

		*nLen = off;
		return 0;
	}


private:
	typedef CSimplifyThread<CNet> CNetThread;
	CXdSocket       m_server, m_client;
	CNetThread      m_thread;
	INetCallback  * m_pCall;
	void          * m_pUser;
	volatile long   m_nConnectState, m_nRunState;
};


INet * CreateNetInstance()
{
	return new CNet;
}

void DestroyNetInstance(INet * pNet)
{
	delete (CNet*)pNet;
}
