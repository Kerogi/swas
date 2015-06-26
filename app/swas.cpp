#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <psapi.h>
#include <Strsafe.h>
#include <Shlobj.h>
#include <Shlwapi.h>
#include <Wininet.h>
//#include <WinCrypt.h>
#include <Rpc.h>
#include <string>
#include <sstream>
#include <vector>
#include "resource.h"

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Wininet.lib")
//#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Rpcrt4.lib")

#define MAX_LOADSTRING 100
#define IDT_REFRESH_CHROME_STATUS 1212

const TCHAR g_tcChromeDetected[] = TEXT("Chrome detected");
const TCHAR g_tcChromeNotDetected[] = TEXT("Chrome not detected");
const TCHAR g_tcSendFailed[] = TEXT("Send failed");
const TCHAR g_tcSendSuccess[] = TEXT("Send successed");

const TCHAR g_tcRawFilepath[] = TEXT("%LOCALAPPDATA%\\Google\\Chrome\\User Data\\Default\\Current Tabs");
const TCHAR g_tcImageName[] = TEXT("chrome.exe");
const TCHAR g_tcUrl[] = TEXT("http://localhost:9999/upload");

const float refreshratehz = 2;
struct ChromeInfo{
	bool bValid;
	DWORD dwProcId;
	ChromeInfo():bValid(false) {}
};
typedef std::basic_string<TCHAR> tstring;
typedef std::basic_stringstream<TCHAR , std::char_traits<TCHAR>, std::allocator<TCHAR> > tstringstream;

void ShowAnError(DWORD err, const TCHAR* pDesc = NULL, const TCHAR* pModuleName = NULL) {
	HMODULE IssuedModule = 0;
	TCHAR tcModuleName[MAX_PATH];
	if(pModuleName) {
		HANDLE hProc = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId() );
		if(hProc) {
			HMODULE hMods[128];
			DWORD nRetCountMod = 0;
			if(EnumProcessModules(hProc, hMods, sizeof(hMods), &nRetCountMod)) {
				for(DWORD i=0; i <= nRetCountMod; ++i) {
					if(0 != hMods[i]
						&& 0 != GetModuleBaseName( hProc, hMods[i], tcModuleName, sizeof(tcModuleName)/sizeof(TCHAR) ) 
						&& 0 != StrStrI(tcModuleName, pModuleName))
						{
							IssuedModule = hMods[i];
							break;
						}
				}
			}
			CloseHandle(hProc);
		}
	}
	LPTSTR errorText = NULL;
	DWORD nFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;
	if( 0 != IssuedModule) nFlags |= FORMAT_MESSAGE_FROM_HMODULE;
	else nFlags |= FORMAT_MESSAGE_FROM_SYSTEM;
	if(0 == FormatMessage( nFlags,
						((0 != IssuedModule)?(IssuedModule): NULL),
						err,
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						(LPTSTR) &errorText,
						0,
						NULL))
	{
		//ShowAnError(GetLastError(), TEXT("Format Error error"),NULL,recCounter);
		tstringstream os;
		if(IssuedModule) {os<<tcModuleName;}
		else {os<<"Unknown";}
		os<<TEXT(" error = ")<<err;
		//os<<TEXT(" FormatMessage error = ")<<;
		if(pDesc) {		
			MessageBox(NULL, os.str().c_str(), pDesc, MB_OK|MB_ICONWARNING );
		} else {
			MessageBox(NULL, os.str().c_str(), TEXT("Error"), MB_OK|MB_ICONWARNING );
		}
	} else {
		if(pDesc) {		
			MessageBox(NULL, (LPCTSTR)errorText, pDesc, MB_OK|MB_ICONWARNING );
		} else {
			MessageBox(NULL, (LPCTSTR)errorText, TEXT("Error"), MB_OK|MB_ICONWARNING );
		}

		if( NULL != errorText )	{
		   LocalFree( errorText );
		   errorText = 0;
		}
	}
}

bool MatcProcesg_tcImageName(DWORD dwProcId,const TCHAR* tcImageName, bool bTestRunning=false)
{
	if(NULL == tcImageName) return false;
	HANDLE hProc = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcId );
	bool bHasName= false;
	bool bResult = false;
	TCHAR tcModuleName[MAX_PATH];
	if (NULL != hProc ) {
		if(bTestRunning) {
			DWORD ExitCode = 0;
			if(GetExitCodeProcess(hProc, &ExitCode)) {
				if (STILL_ACTIVE != ExitCode) return false;
			} else {
				if(WAIT_TIMEOUT != WaitForSingleObject(hProc, 0)) return false;
			}
		}
		HMODULE hMod;
		DWORD nRetCountMod = 0;
		if(EnumProcessModules(hProc, &hMod, sizeof(hMod), &nRetCountMod)){
			bHasName = (0 != GetModuleBaseName( hProc, hMod, tcModuleName, sizeof(tcModuleName)/sizeof(TCHAR) ) );
		} else {
			bHasName = (0 != GetProcessImageFileName(hProc, tcModuleName, sizeof(tcModuleName)/sizeof(TCHAR)) ) ;
		}
	}
	if(bHasName) {
		const TCHAR *pExeName = _tcsrchr(tcModuleName, TEXT('\\'));	
		pExeName = (pExeName) ? ++pExeName : tcModuleName;
		bResult = (0 == StrCmpI(pExeName,tcImageName));
	}
	CloseHandle(hProc);
	return bResult;
}

bool DetectChrome(ChromeInfo* pPrevInfo, const TCHAR* tcChromeImageName, bool bRetry = false) {
	if(pPrevInfo && pPrevInfo->bValid) {
		pPrevInfo->bValid = MatcProcesg_tcImageName(pPrevInfo->dwProcId, tcChromeImageName, true);
		if(!(!pPrevInfo->bValid && bRetry))
			return pPrevInfo->bValid;
	} 
	DWORD runningProcessIds[1024], dwRetCount;
	int nProcCount;

	if(!EnumProcesses( runningProcessIds, sizeof(runningProcessIds), &dwRetCount )) return false;
	nProcCount = dwRetCount/sizeof(DWORD);
	for (int i = nProcCount; i > 0; --i ) {
		DWORD dwCurrentProcId = runningProcessIds[i];
		if(0 != dwCurrentProcId) {	
			if(MatcProcesg_tcImageName(dwCurrentProcId, tcChromeImageName)){
				if (pPrevInfo) {
					pPrevInfo->bValid = true;
					pPrevInfo->dwProcId = dwCurrentProcId;
					return true;
				}
			}
		}
	}
	return false;
}

bool CreateFullPath(const TCHAR* tcRawFileName, TCHAR* tcFullPath, int nMaxFullpath, TCHAR** sFilePart = 0) {
	TCHAR tcPath[MAX_PATH];
	int res = ExpandEnvironmentStrings(tcRawFileName, tcPath, MAX_PATH);
	if (0 != res && res <= nMaxFullpath)
	{
		int res = GetFullPathName(tcPath, nMaxFullpath, tcFullPath, sFilePart);
		if(0 != res){
			return true;
		}
	}
	return false;
}

HANDLE GetFile(const TCHAR* tcFilepath) {
	HANDLE hFile = CreateFile(tcFilepath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL, OPEN_EXISTING, 0, NULL);
	if(hFile) {
		return hFile;
	} else {
		ShowAnError(GetLastError(), TEXT("File error"));
	}
	return NULL;
}

bool AllocAndCopyTCtoMB(char** ppDst, const TCHAR* pSrc, const int nLendth, UINT nCodePade = CP_ACP){
	if(NULL == pSrc || nLendth<-0) return false;
#ifdef UNICODE
	char* pDst = new char[nLendth]; 
	int res = WideCharToMultiByte( nCodePade, 0, pSrc, nLendth, pDst, nLendth, NULL, NULL );
#else
	char *pDst = new char[ nLendth]; 
	int res = StringCbCopyA(pDst, pSrc, nLendth)
#endif
	if( res != 0) {
		*ppDst = pDst;
		return true;
	} else {
		delete[] pDst;
	}
	return false;
}

bool AllocAndCopyMBtoTC(TCHAR** ppDst, const char* pSrc, const int nLendth, UINT nCodePade = CP_ACP){
	if(NULL == pSrc || nLendth<-0) return false;
#ifdef UNICODE
	TCHAR* pDst = new TCHAR[nLendth]; 
	int res = MultiByteToWideChar(nCodePade, 0, pSrc, nLendth, pDst, nLendth);
#else
	char *pDst = new char[nLendth]; 
	int res = StringCbCopyA(pDst, pSrc, nLendth)
#endif
	if( res != 0) {
		*ppDst = pDst;
		return true;
	} else {
		delete[] pDst;
	}
	return false;
}


bool GenerateBoundary(char* sDst, int nDstSize, const char* sFileName) {
	if (nDstSize<38) return false;

    UUID uuid;
    RPC_STATUS ret_val = ::UuidCreate(&uuid);
	bool result = false;

    if (ret_val == RPC_S_OK)
    {
        char* wszUuid = NULL;
        ::UuidToStringA(&uuid, (RPC_CSTR*)&wszUuid);
        if (wszUuid != NULL) {
			if(SUCCEEDED(StringCbCopyA(sDst, nDstSize, wszUuid))) {
				if(sFileName!=NULL) {
					StringCchCatA(sDst, nDstSize, sFileName);
				}
				result = true; 
			}
		}
        ::RpcStringFreeA((RPC_CSTR*)&wszUuid);
        wszUuid = NULL;
    }
	return result;
}



#define HEADERS_MAX_SIZE 512
#define URL_PART_SIZE 24
bool SendFile(HANDLE hFile, const TCHAR* sUrl, const char* sVariableName, const char* sFileName) {
	if(!hFile) return false;
	bool result = false;
	LARGE_INTEGER lnSize;
	if(!GetFileSizeEx(hFile, &lnSize)){
		CloseHandle(hFile);
		return false;
	}
	DWORD nDataSize = lnSize.LowPart;
	if(!(nDataSize>0)) {
		CloseHandle(hFile);
		return false;
	}
	std::vector<char> pData(nDataSize);
	DWORD nReadSize = 0; 
	if(!ReadFile(hFile, (void*)&pData[0], nDataSize, &nReadSize, NULL) || !(nReadSize >0) || (nReadSize != nDataSize)) {
		CloseHandle(hFile);
		return false;
	}
	CloseHandle(hFile);

	TCHAR extraInfo[URL_PART_SIZE];
	TCHAR hostName[URL_PART_SIZE];
	TCHAR passwordSet[URL_PART_SIZE];
	TCHAR schemeUrl[URL_PART_SIZE];
	TCHAR fileUrlPath[URL_PART_SIZE];
	TCHAR userName[URL_PART_SIZE];

	URL_COMPONENTS aUrl;
	aUrl.dwStructSize=sizeof(URL_COMPONENTS);
	aUrl.dwHostNameLength=URL_PART_SIZE;
	aUrl.dwPasswordLength=URL_PART_SIZE;
	aUrl.dwSchemeLength=URL_PART_SIZE;
	aUrl.dwUrlPathLength=URL_PART_SIZE;
	aUrl.dwUserNameLength=URL_PART_SIZE;
	aUrl.dwExtraInfoLength=URL_PART_SIZE;
	aUrl.lpszExtraInfo=extraInfo;
	aUrl.lpszHostName=hostName;
	aUrl.lpszPassword=passwordSet;
	aUrl.lpszScheme=schemeUrl;
	aUrl.lpszUrlPath=fileUrlPath;
	aUrl.lpszUserName=userName;

	PCTSTR rgpszAcceptTypes[] = {_T("text/*"), NULL};

	if(!InternetCrackUrl(sUrl, 0, 0, &aUrl)) return false;
	HINTERNET hInternet = InternetOpen(TEXT("swas"),INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if(hInternet) {
		HINTERNET hConnect = InternetConnect(hInternet,aUrl.lpszHostName, aUrl.nPort, aUrl.lpszUserName, aUrl.lpszHostName, INTERNET_SERVICE_HTTP, 0, 0);
		if(hConnect){
			HINTERNET hReq = HttpOpenRequest(hConnect, TEXT("POST"), aUrl.lpszUrlPath, NULL, NULL, rgpszAcceptTypes, 
				INTERNET_FLAG_NO_UI | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD, 0);
			if(hReq) {
				

				//see http://stackoverflow.com/questions/6407755/how-to-send-a-zip-file-using-wininet-in-my-vc-application
				char sBoundary[HEADERS_MAX_SIZE];
				if(GenerateBoundary(sBoundary, HEADERS_MAX_SIZE, sFileName)) {
					std::stringstream osHeaders;
					std::stringstream osHeadPart;
					std::stringstream osTailPart;
					const char const endl[] = "\r\n"; 

					osHeaders<<"Content-Type: multipart/form-data; boundary="<<sBoundary;

					osHeadPart<<"--"<<sBoundary<<endl;
					osHeadPart<<"Content-Disposition: form-data; ";
					osHeadPart<<"name=\""<<((NULL != sVariableName)?sVariableName:"filearg")<<"\"; ";
					osHeadPart<<"filename=\""<<((NULL != sFileName)?sFileName:"Chrome Tabs")<<"\""<<endl;
					osHeadPart<<"Content-Type: application/octet-stream"<<endl;
					osHeadPart<<endl;

					osTailPart<<endl<<"--"<<sBoundary<<"--"<<endl;

					std::string sHeaders = osHeaders.str();
					std::string sHeadPart = osHeadPart.str();
					std::string sTailPart = osTailPart.str();

					HttpAddRequestHeadersA(hReq, sHeaders.c_str(), -1, HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD); 
				
					INTERNET_BUFFERS bufferIn;

					memset(&bufferIn, 0, sizeof(INTERNET_BUFFERS));

					bufferIn.dwStructSize  = sizeof(INTERNET_BUFFERS);
					bufferIn.dwBufferTotal = sHeadPart.length() + nDataSize + sTailPart.length();
					DWORD bytesWritten;
					if(HttpSendRequestEx(hReq, &bufferIn, NULL, HSR_INITIATE, 0)) {
						InternetWriteFile(hReq, (const void*)sHeadPart.c_str(), sHeadPart.length(), &bytesWritten);

						InternetWriteFile(hReq, (const void*)&pData[0], nDataSize, &bytesWritten);
						// or a while loop for call InternetWriteFile every 1024 bytes...

						InternetWriteFile(hReq, (const void*)sTailPart.c_str(), sTailPart.length(), &bytesWritten);
						result = true;
					}else{
						ShowAnError(GetLastError(), TEXT("Network error"), TEXT("Wininet"));
					}
				}
				HttpEndRequest(hReq, NULL, HSR_INITIATE, 0);

			}else{
				ShowAnError(GetLastError(), TEXT("Network error"), TEXT("Wininet"));
			}
		}else{
			ShowAnError(GetLastError(), TEXT("Network error"), TEXT("Wininet"));
		}
		InternetCloseHandle(hInternet);
	}
	return result;
}

bool LoadStringOrDefault(HINSTANCE hInst, TCHAR* lsDst, int nDstSize, UINT uStringResID, const TCHAR* szDefault) {
	if (0 == LoadString(hInst, uStringResID, lsDst, nDstSize)){
		StringCbCopy(lsDst, nDstSize, szDefault);
		return false;
	}
	return true;
}
bool UpdateField(HWND hWnd, UINT nCtrId, char* pStr, const int nLength, bool bSet = false){
	if (NULL == pStr || nLength <=0 ) return false;
	bool result = false;
	if(bSet) {
#ifdef UNICODE
		TCHAR* sBuf= new TCHAR[nLength];
		int nLenUnicode = MultiByteToWideChar(CP_ACP, 0, pStr, nLength, sBuf, nLength);
		if(0 < nLenUnicode && nLenUnicode <= nLength) {
			result = (0 != SetDlgItemText(hWnd, nCtrId, sBuf));
		}
		delete[] sBuf;
#else
		SetDlgItemText(hWnd, nCtrId, pStr);
#endif	
		return result;
	} 
#ifdef UNICODE
	TCHAR* sBuf= new TCHAR[nLength];
	int nLenUnicode = GetDlgItemText(hWnd, nCtrId, sBuf, nLength);
	if(0 < nLenUnicode && nLenUnicode <= nLength-1) {
		result = (0 != WideCharToMultiByte( CP_ACP, 0, sBuf, nLenUnicode, pStr, nLength, NULL, NULL ));
		pStr[nLenUnicode] = 0;
	}
	delete[] sBuf;
#else
	GetDlgItemText(hWnd, nCtrId, pStr, nLength);
#endif
	return result;
}
INT_PTR CALLBACK DlgProc(
  _In_ HWND   hDlg,
  _In_ UINT   uMsg,
  _In_ WPARAM wParam,
  _In_ LPARAM lParam
) {
	int wmId, wmEvent;
	static ChromeInfo chromeInfo;
	static TCHAR ltcChromeDetected[MAX_LOADSTRING];
	static TCHAR ltcChromeNotDetected[MAX_LOADSTRING];
	static TCHAR ltcSendSuccess[MAX_LOADSTRING];
	static TCHAR ltcSendFailed[MAX_LOADSTRING];

	static char szVariable[MAX_LOADSTRING];
	static char sRawFilepath[MAX_PATH];
	static char sUrl[MAX_PATH];

	switch (uMsg)
	{
		case WM_INITDIALOG:
			{
				ShowAnError(87);
				HINSTANCE hInstance = reinterpret_cast<HINSTANCE>(lParam);
				TCHAR szTemp[MAX_LOADSTRING];
				if (0 != LoadString(hInstance, IDS_APP_TITLE, szTemp, MAX_LOADSTRING)){
					SetWindowText(hDlg, szTemp);
				}
				if (0 != LoadString(hInstance, IDS_BTN_SEND_CAPTION, szTemp, MAX_LOADSTRING)){
					SetDlgItemText(hDlg, IDC_BTN_SEND, szTemp);
				}
				LoadStringOrDefault(hInstance, ltcChromeDetected, MAX_LOADSTRING, IDS_CHROME_DETECTED, g_tcChromeDetected);
				LoadStringOrDefault(hInstance, ltcChromeNotDetected, MAX_LOADSTRING, IDS_CHROME_NOTDETECTED, g_tcChromeNotDetected);

				SetDlgItemText(hDlg, IDC_LBL_CHROME_STATUS, ltcChromeNotDetected);

				LoadStringOrDefault(hInstance, ltcSendFailed, MAX_LOADSTRING, IDS_SEND_FAILED, g_tcSendFailed);
				LoadStringOrDefault(hInstance, ltcSendSuccess, MAX_LOADSTRING, IDS_SEND_SUCCESS, g_tcSendSuccess);

				SetDlgItemText(hDlg, IDC_LBL_SEND_STATUS, TEXT(""));

				if (0 != SetTimer(hDlg,	IDT_REFRESH_CHROME_STATUS, int(1000 / refreshratehz), NULL))
				{
					ShowWindow(GetDlgItem(hDlg, IDC_BTN_REFRESH), SW_HIDE);
				} 
				else 
				{
					if (0 != LoadString(hInstance, IDS_BTN_REFRESH_CAPTION, szTemp, MAX_LOADSTRING)){
						SetDlgItemText(hDlg, IDC_BTN_REFRESH, szTemp);
					}
					ShowWindow(GetDlgItem(hDlg, IDC_BTN_REFRESH), SW_SHOW);
				}
				
				char* pTemp = 0;
				if (AllocAndCopyTCtoMB(&pTemp, g_tcRawFilepath, MAX_PATH)) {
					StringCbCopyA(sRawFilepath, MAX_PATH, pTemp);
					delete[] pTemp;
				}
				pTemp = 0;
				if (AllocAndCopyTCtoMB(&pTemp, g_tcUrl, MAX_PATH)) {
					StringCbCopyA(sUrl, MAX_PATH, pTemp);
					delete[] pTemp;
				}

				StringCbCopyA(szVariable, MAX_LOADSTRING, "filearg");

				UpdateField(hDlg, IDC_EDT_FILEPATH, sRawFilepath, MAX_PATH, true);
				UpdateField(hDlg, IDC_EDT_SEND_URL, sUrl, MAX_PATH, true);
			}
			return TRUE;
		case WM_TIMER: 
			switch (wParam) 
			{ 
				case IDT_REFRESH_CHROME_STATUS: 
					if(DetectChrome(&chromeInfo, g_tcImageName)){
						SetDlgItemText(hDlg, IDC_LBL_CHROME_STATUS, ltcChromeDetected);
					} else {
						SetDlgItemText(hDlg, IDC_LBL_CHROME_STATUS, ltcChromeNotDetected);
					}

					return TRUE;
			}
			break;
		case WM_COMMAND:
			wmId    = LOWORD(wParam);
			wmEvent = HIWORD(wParam);
			switch (wmId)
			{
				case IDCANCEL:
						SendMessage(hDlg, WM_CLOSE, 0, 0);
						return TRUE;
				case IDC_BTN_SEND:{
						UpdateField(hDlg, IDC_EDT_FILEPATH, sRawFilepath, MAX_PATH);
						UpdateField(hDlg, IDC_EDT_SEND_URL, sUrl, MAX_PATH);
						TCHAR tcRawFilepath[MAX_PATH];
						TCHAR tcUrl[MAX_PATH];
						TCHAR* pTemp = 0;
						if (AllocAndCopyMBtoTC(&pTemp, sUrl, MAX_PATH)) {
							StringCbCopy(tcUrl, MAX_PATH, pTemp);
							delete[] pTemp;
						} 
						pTemp = 0;
						if (AllocAndCopyMBtoTC(&pTemp, sRawFilepath, MAX_PATH)) {
							StringCbCopy(tcRawFilepath, MAX_PATH, pTemp);
							delete[] pTemp;
						}
						TCHAR tcFullPath[MAX_PATH];
						TCHAR *tcFilenamePart;
						if(CreateFullPath(tcRawFilepath, tcFullPath, sizeof(tcFullPath)/ sizeof(TCHAR), &tcFilenamePart) )
						{
							char* pFilenamePart = 0;
							if (AllocAndCopyTCtoMB(&pFilenamePart, tcFilenamePart, MAX_PATH)) 

							if(SendFile(GetFile(tcFullPath), tcUrl, szVariable, pFilenamePart)) {
								SetDlgItemText(hDlg, IDC_LBL_SEND_STATUS, ltcSendSuccess);
							} else {
								SetDlgItemText(hDlg, IDC_LBL_SEND_STATUS, ltcSendFailed);
							}
							
							if(pFilenamePart) delete[] pFilenamePart;
						}
					}
					return TRUE;	
				case IDC_BTN_REFRESH:
					if(DetectChrome(&chromeInfo, g_tcImageName, true)){
						SetDlgItemText(hDlg, IDC_LBL_CHROME_STATUS, ltcChromeDetected);
					} else {
						SetDlgItemText(hDlg, IDC_LBL_CHROME_STATUS, ltcChromeNotDetected);
					}
					return TRUE;
					
			}
			break;

		case WM_CLOSE:
			KillTimer(hDlg, IDT_REFRESH_CHROME_STATUS);
			DestroyWindow(hDlg);
			return TRUE;
		case WM_DESTROY:
			PostQuitMessage(0);
			return TRUE;
	}

	return FALSE;
}



int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{

	HWND hDlg = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DLG_MAIN), NULL , DlgProc, reinterpret_cast<LPARAM>(hInstance));
	ShowWindow(hDlg, SW_SHOW);


	// Main message loop:
	BOOL ret;
	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0))
	{
		if(!IsDialogMessage(hDlg, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}