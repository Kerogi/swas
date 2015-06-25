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
#include <WinCrypt.h>
#include "resource.h"

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "Crypt32.lib")

#define MAX_LOADSTRING 100
#define IDT_REFRESH_CHROME_STATUS 1212

const TCHAR sChromeDetected[] = TEXT("Chrome detected");
const TCHAR sChromeNotDetected[] = TEXT("Chrome not detected");
const TCHAR sSendFailed[] = TEXT("Send failed");
const TCHAR sSendSuccess[] = TEXT("Send successed");

const TCHAR sFileName[] = TEXT("Google\\Chrome\\User Data\\Default\\Current Tabs");
const TCHAR sImageName[] = TEXT("chrome.exe");
const TCHAR sUrl[] = TEXT("localhost:9999");

const float refreshratehz = 2;
struct ChromeInfo{
	bool bValid;
	DWORD dwProcId;
	ChromeInfo():bValid(false) {}
};

bool MatcProcessImageName(DWORD dwProcId,const TCHAR* szImageName, bool bTestRunning=false)
{
	if(NULL == szImageName) return false;
	HANDLE hProc = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcId );
	bool bHasName= false;
	bool bResult = false;
	TCHAR szModuleName[MAX_PATH];
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
			bHasName = (0 != GetModuleBaseName( hProc, hMod, szModuleName, sizeof(szModuleName)/sizeof(TCHAR) ) );
		} else {
			bHasName = (0 != GetProcessImageFileName(hProc, szModuleName, sizeof(szModuleName)/sizeof(TCHAR)) ) ;
		}
	}
	if(bHasName) {
		const TCHAR *pExeName = _tcsrchr(szModuleName, TEXT('\\'));	
		pExeName = (pExeName) ? ++pExeName : szModuleName;
		bResult = (0 == _tcscmp(pExeName, szImageName));
	}
	CloseHandle(hProc);
	return bResult;
}

bool DetectChrome(ChromeInfo* pPrevInfo, const TCHAR* szChromeImageName, bool bRetry = false) {
	if(pPrevInfo && pPrevInfo->bValid) {
		pPrevInfo->bValid = MatcProcessImageName(pPrevInfo->dwProcId, szChromeImageName, true);
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
			if(MatcProcessImageName(dwCurrentProcId, szChromeImageName)){
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

bool GetFilePath(const TCHAR* sFilename, TCHAR* sFullPath, int nMaxFullpath) {
	TCHAR szPath[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath)))
	{
		if(PathAppend(szPath, sFilename)){
			if(SUCCEEDED(StringCbCopy(sFullPath, nMaxFullpath, szPath))){
					return true;
			}
		}
	}
	return false;
}

HFILE GetFile(const TCHAR* sFilepath) {
	return NULL;
}
#define URL_SIZE 24
bool SendFile(HFILE hFile, const TCHAR* sUrl) {
	
	TCHAR extraInfo[URL_SIZE];
	TCHAR hostName[URL_SIZE];
	TCHAR passwordSet[URL_SIZE];
	TCHAR schemeUrl[URL_SIZE];
	TCHAR fileUrlPath[URL_SIZE];
	TCHAR userName[URL_SIZE];

	URL_COMPONENTS aUrl;
	aUrl.dwStructSize=sizeof(URL_COMPONENTS);
	aUrl.dwHostNameLength=URL_SIZE;
	aUrl.dwPasswordLength=URL_SIZE;
	aUrl.dwSchemeLength=URL_SIZE;
	aUrl.dwUrlPathLength=URL_SIZE;
	aUrl.dwUserNameLength=URL_SIZE;
	aUrl.dwExtraInfoLength=URL_SIZE;
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

		if(InternetConnect(hInternet,aUrl.lpszHostName, aUrl.nPort, aUrl.lpszUserName, aUrl.lpszHostName, INTERNET_SERVICE_HTTP, 0, 0)){
			HINTERNET hReq = HttpOpenRequest(hInternet, TEXT("POST"), TEXT("Afile"), NULL, NULL, rgpszAcceptTypes, 
				INTERNET_FLAG_NO_UI | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD, 0);
			if(hReq) {
				char data[] = "\x01\x02\x03\x04";
				DWORD dataSize = 4;

				//see http://stackoverflow.com/questions/6407755/how-to-send-a-zip-file-using-wininet-in-my-vc-application
				static char hdrs[] =    "Content-Type: multipart/form-data; boundary=AaB03x";   
				static char head[] =    "--AaB03x\r\n"
										"Content-Disposition: form-data; name=\"userfile\"; filename=\"test.bin\"\r\n"
										"Content-Type: application/octet-stream\r\n"
										"\r\n";
				static char tail[] =    "\r\n"
										"--AaB03x--\r\n";
				HttpAddRequestHeadersA(hReq, 
                      hdrs, -1, 
                      HTTP_ADDREQ_FLAG_REPLACE | 
                      HTTP_ADDREQ_FLAG_ADD); 
				
				INTERNET_BUFFERS bufferIn;

				memset(&bufferIn, 0, sizeof(INTERNET_BUFFERS));

				bufferIn.dwStructSize  = sizeof(INTERNET_BUFFERS);
				bufferIn.dwBufferTotal = strlen(head) + dataSize + strlen(tail);
				DWORD bytesWritten;
				if(HttpSendRequestEx(hReq, &bufferIn, NULL, HSR_INITIATE, 0)) {
					InternetWriteFile(hReq, (const void*)head, strlen(head), &bytesWritten);

					InternetWriteFile(hReq, (const void*)data, dataSize, &bytesWritten);
					// or a while loop for call InternetWriteFile every 1024 bytes...

					InternetWriteFile(hReq, (const void*)tail, strlen(tail), &bytesWritten);

					HttpEndRequest(hReq, NULL, HSR_INITIATE, 0);
				}
			}
		}
		InternetCloseHandle(hInternet);
	}
	return false;
}

bool LoadStringOrDefault(HINSTANCE hInst, TCHAR* lsDst, int nDstSize, UINT uStringResID, const TCHAR* szDefault) {
	if (0 == LoadString(hInst, uStringResID, lsDst, nDstSize)){
		StringCbCopy(lsDst, nDstSize, szDefault);
		return false;
	}
	return true;
}

INT_PTR CALLBACK DlgProc(
  _In_ HWND   hwndDlg,
  _In_ UINT   uMsg,
  _In_ WPARAM wParam,
  _In_ LPARAM lParam
) {
	int wmId, wmEvent;
	static ChromeInfo chromeInfo;
	static TCHAR lszChromeDetected[MAX_LOADSTRING];
	static TCHAR lszChromeNotDetected[MAX_LOADSTRING];
	static TCHAR lszSendSuccess[MAX_LOADSTRING];
	static TCHAR lszSendFailed[MAX_LOADSTRING];
	static TCHAR szFullPath[MAX_PATH];

	switch (uMsg)
	{
		case WM_INITDIALOG:
			{
				HINSTANCE hInstance = reinterpret_cast<HINSTANCE>(lParam);
				TCHAR szTemp[MAX_LOADSTRING];
				if (0 != LoadString(hInstance, IDS_APP_TITLE, szTemp, MAX_LOADSTRING)){
					SetWindowText(hwndDlg, szTemp);
				}
				if (0 != LoadString(hInstance, IDS_BTN_SEND_CAPTION, szTemp, MAX_LOADSTRING)){
					SetDlgItemText(hwndDlg, IDC_BTN_SEND, szTemp);
				}
				LoadStringOrDefault(hInstance, lszChromeDetected, MAX_LOADSTRING, IDS_CHROME_DETECTED, sChromeDetected);
				LoadStringOrDefault(hInstance, lszChromeNotDetected, MAX_LOADSTRING, IDS_CHROME_NOTDETECTED, sChromeNotDetected);

				SetDlgItemText(hwndDlg, IDC_LBL_CHROME_STATUS, lszChromeNotDetected);

				LoadStringOrDefault(hInstance, lszSendFailed, MAX_LOADSTRING, IDS_SEND_FAILED, sSendFailed);
				LoadStringOrDefault(hInstance, lszSendSuccess, MAX_LOADSTRING, IDS_SEND_SUCCESS, sSendSuccess);

				SetDlgItemText(hwndDlg, IDC_LBL_SEND_STATUS, TEXT(""));

				if (0 != SetTimer(hwndDlg,	IDT_REFRESH_CHROME_STATUS, int(1000 / refreshratehz), NULL))
				{
					ShowWindow(GetDlgItem(hwndDlg, IDC_BTN_REFRESH), SW_HIDE);
				} 
				else 
				{
					if (0 != LoadString(hInstance, IDS_BTN_REFRESH_CAPTION, szTemp, MAX_LOADSTRING)){
						SetDlgItemText(hwndDlg, IDC_BTN_REFRESH, szTemp);
					}
					ShowWindow(GetDlgItem(hwndDlg, IDC_BTN_REFRESH), SW_SHOW);
				}
			}
			return TRUE;
		case WM_TIMER: 
			switch (wParam) 
			{ 
				case IDT_REFRESH_CHROME_STATUS: 
					if(DetectChrome(&chromeInfo, sImageName)){
						SetDlgItemText(hwndDlg, IDC_LBL_CHROME_STATUS, lszChromeDetected);
					} else {
						SetDlgItemText(hwndDlg, IDC_LBL_CHROME_STATUS, lszChromeNotDetected);
					}

					return TRUE;
			}
			break;
		case WM_COMMAND:
			wmId    = LOWORD(wParam);
			wmEvent = HIWORD(wParam);
			switch (wmId)
			{
				case IDC_BTN_SEND:
					if(GetFilePath(sFileName, szFullPath, sizeof(szFullPath)/ sizeof(TCHAR)) 
						&& SendFile(GetFile(szFullPath), sUrl)){
						SetDlgItemText(hwndDlg, IDC_LBL_SEND_STATUS, lszSendSuccess);
					} else {
						SetDlgItemText(hwndDlg, IDC_LBL_SEND_STATUS, lszSendFailed);
					}
					return TRUE;	
				case IDC_BTN_REFRESH:
					if(DetectChrome(&chromeInfo, sFileName, true)){
						SetDlgItemText(hwndDlg, IDC_LBL_CHROME_STATUS, lszChromeDetected);
					} else {
						SetDlgItemText(hwndDlg, IDC_LBL_CHROME_STATUS, lszChromeNotDetected);
					}
					return TRUE;
					
			}
			break;

		case WM_CLOSE:
			KillTimer(hwndDlg, IDT_REFRESH_CHROME_STATUS);
			PostQuitMessage(0);
			return TRUE;
	}

	return DefWindowProc(hwndDlg, uMsg, wParam, lParam);
	//return FALSE;
}



int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
 	// TODO: Place code here.
	MSG msg;

	HWND hWnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DLG_MAIN), NULL , DlgProc, reinterpret_cast<LPARAM>(hInstance));
	ShowWindow(hWnd, SW_SHOW);


	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int) msg.wParam;
}