#pragma once
#include "netdata.h"


enum enumFileAction
{
	Action_Unknown = 0, 
	Action_SendFile, 
	Action_RecvFile, 
};


class CRdcFile
{
public:
	CRdcFile()
		: m_pNet(0), m_bIsClient(FALSE), m_nAction(0)
		, m_pFile(0), m_nFileSize(0), m_nFileLen(0)
		, m_szBuf(0)
	{
		// console event.
		DWORD  nRet, nMode = 0;

		m_hStdin = GetStdHandle(STD_INPUT_HANDLE); Assert(m_hStdin != INVALID_HANDLE_VALUE);
		GetConsoleMode(m_hStdin, &nMode);
		nRet = SetConsoleMode(m_hStdin, nMode|ENABLE_MOUSE_INPUT); Assert(nRet);
	}

	~CRdcFile()
	{
		if(m_szBuf) free(m_szBuf);
	}

	int  Init(INet * pNet, BOOL bIsClient)
	{
		m_pNet = pNet; m_bIsClient = bIsClient;
		return 0;
	}

	int  GetKeyEvent(wchar_t * szBuf, int nCount)
	{
		// PeekConsoleInput 这个函数比较诡异, 有的文件读不到, 可能和后缀有关. 
		// 会触发事件但是读不出 KEY_EVENT. 
		int          nRet, nLen = 0, nKey = 0;
		INPUT_RECORD inBuf[1024] = {0};

		nRet = PeekConsoleInput(m_hStdin, inBuf, _countof(inBuf), (LPDWORD)&nLen);
		for(int i = 0; i < (int)nLen; i++)
		{
			switch(inBuf[i].EventType)
			{
			case KEY_EVENT:
				if(inBuf[i].Event.KeyEvent.uChar.UnicodeChar && inBuf[i].Event.KeyEvent.bKeyDown)
				{
					Assert(nKey < nCount);
					szBuf[nKey++] = inBuf[i].Event.KeyEvent.uChar.UnicodeChar;
				}
				break;
			}
		}

		if(nKey > 0) FlushConsoleInputBuffer(m_hStdin);
		return   nKey;
	}

	int  OpenFile(LPCWSTR szPath)
	{
		// 没有考虑同步. 
		DWORD dwAttr = GetFileAttributes(szPath);
		char  szBuf[sizeof(NetDataHeader_t)+sizeof(NetU_FileInfo_t)] = {0};
		NetDataHeader_t * pPacket   = (NetDataHeader_t*)(szBuf);
		NetU_FileInfo_t * pFileInfo = (NetU_FileInfo_t*)(pPacket+1);

		if(m_pFile || !m_pNet->GetClientInfo(0, 0)) return -1;
		if(dwAttr!=-1 && !(dwAttr&FILE_ATTRIBUTE_DIRECTORY))
		{
			m_pFile   = _wfopen(szPath, L"rb");
			m_nAction = Action_SendFile; m_nFileLen = 0;
		}

		if(m_pFile)
		{
			fseek(m_pFile, 0, SEEK_END);
			m_nFileSize = ftell(m_pFile);
			fseek(m_pFile, 0, SEEK_SET);

			if(m_nFileSize == 0)
			{
				fclose(m_pFile); m_pFile = 0;
				return -2;
			}
		}

		if(m_nFileSize > 0)
		{
			const wchar_t * p = wcsrchr(szPath, '\\');
			if(p) p++; else p = szPath;

			INIT_NET_PACKET_S(pPacket, TypeU_PushFileInfo, sizeof(szBuf));
			if(m_bIsClient) pPacket->nMagic[1] = 'C';
			pFileInfo->nSize = m_nFileSize;
			wcscpy(pFileInfo->szName, p);
			m_pNet->SendPacket(pPacket);
		}

		return m_pFile ? 0 : -3;
	}

	void CloseFile()
	{
		if(m_pFile)  fclose(m_pFile);
		m_pFile = 0; m_nFileSize = 0; m_nAction = 0;
	}

	int  SendFile()
	{
		int               nBufSize = 1024 * 4;
		NetDataHeader_t * pPacket = 0;

		Assert(m_pFile);
		if(m_nAction != Action_SendFile) return -1;
		if(!m_szBuf)
		{
			m_szBuf = (char*)malloc(nBufSize); Assert(m_szBuf);
		}

		pPacket = (NetDataHeader_t*)m_szBuf;
		int nRet = fread(pPacket+1, 1, nBufSize-sizeof(*pPacket), m_pFile);
		if(!nRet)  
		{
			CloseFile();
			return 1;
		}

		INIT_NET_PACKET_S(pPacket, TypeU_PushFileData, nRet+sizeof(*pPacket));
		if(m_bIsClient) pPacket->nMagic[1] = 'C';

		return m_pNet->SendPacket(pPacket);
	}

	int  RecvFile(NetDataHeader_t * pPacket)
	{
		int nRet, nLen = 0;

		if(pPacket->nType == TypeU_PushFileInfo)
		{
			wchar_t szFile[MAX_PATH] = {0};

			if(m_pFile)
			{
				printf(">> mirdc file, warning find a uncompleted file!!!(%d)\n", m_nFileSize);
				CloseFile();
			}
			_snwprintf(szFile, _countof(szFile), L"tmpfile\\%s", ((NetU_FileInfo_t*)(pPacket+1))->szName);
			m_nFileSize = ((NetU_FileInfo_t*)(pPacket+1))->nSize;
			m_pFile     = _wfopen(szFile, L"wb");

			printf(">> mirdc file, recv file info -->%d:%p:%S\n", m_nFileSize, m_pFile, szFile);
			if(!m_pFile)  return -2;
			m_nAction   = Action_RecvFile; m_nFileLen = 0;
			return 0;
		}

		Assert(pPacket->nType == TypeU_PushFileData);
		if(!m_pFile || !m_nFileSize) return -3;
		if(m_nAction != Action_RecvFile) return -4;

		nLen = pPacket->nLen-sizeof(*pPacket);
		nRet = fwrite(pPacket+1, 1, nLen, m_pFile); Assert(nRet == nLen);
		m_nFileLen += nLen;
		if(m_nFileLen >= m_nFileSize)
		{
			printf(">> mirdc file, recv file finished(%d,%d)\n", m_nFileLen, m_nFileSize);
			CloseFile();
		}

		return 0;
	}


private:
	HANDLE   m_hStdin;
	FILE   * m_pFile;
	INet   * m_pNet;
	char   * m_szBuf;
	BOOL     m_bIsClient;
	int      m_nFileSize, m_nFileLen, m_nAction;
};
