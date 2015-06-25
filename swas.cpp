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
#include "resource.h"

#define MAX_LOADSTRING 100
#define IDT_REFRESH_CHROME_STATUS 1212

const TCHAR sChromeDetected[] = TEXT("Chrome detected");
const TCHAR sChromeNotDetected[] = TEXT("Chrome not detected");
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

bool DetectChrome(ChromeInfo* pPrevInfo, bool bRetry = false, const TCHAR* szChromeImageName = TEXT("chrome.exe")) {
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
				if (0 == LoadString(hInstance, IDS_CHROME_DETECTED, lszChromeDetected, MAX_LOADSTRING)){
					StringCbCopy(lszChromeDetected, MAX_LOADSTRING, sChromeDetected);
				}
				if (0 == LoadString(hInstance, IDS_CHROME_NOTDETECTED, lszChromeNotDetected, MAX_LOADSTRING)){
					StringCbCopy(lszChromeNotDetected, MAX_LOADSTRING, sChromeNotDetected);
				}
				SetDlgItemText(hwndDlg, IDC_LBL_CHROME_STATUS, lszChromeNotDetected);

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
					if(DetectChrome(&chromeInfo)){
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
					return TRUE;	
				case IDC_BTN_REFRESH:
					if(DetectChrome(&chromeInfo, true)){
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