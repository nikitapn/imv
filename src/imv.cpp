#include "stdafx.h"
#include "resource.h"
#include "imv.hpp"

CAppModule _Module;

int Run(LPTSTR lpstrCmdLine, int nCmdShow = SW_SHOWDEFAULT) {
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	auto str = std::string(lpstrCmdLine);
	str = str.substr(1, str.length() - 2);

	int nRet = -1;
	try {
		ImvWindow wndMain(str);
		
		if (wndMain.Create(NULL, &ImvWindow::rcDefault, str.c_str(), CFrameWinTraits::GetWndStyle(0), CFrameWinTraits::GetWndExStyle(0)) == NULL)
		{
			ATLTRACE(_T("Main window creation failed!\n"));
			return 0;
		}

		wndMain.ShowWindow(nCmdShow);

		nRet = theLoop.Run();
	} catch (std::exception& ex) {
		MessageBoxA(NULL, ex.what(), NULL, MB_ICONERROR);
		return -1;
	} catch (...) {
		return -1;
	}
	
	_Module.RemoveMessageLoop();
	
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	if (*lpstrCmdLine == '\0') return -1;
	
	if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) != S_OK) {
		return -1;
	}

	INITCOMMONCONTROLSEX iccx;
	iccx.dwSize = sizeof(iccx);
	iccx.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES;
	BOOL bRet = ::InitCommonControlsEx(&iccx);
	bRet;
	ATLASSERT(bRet);

	HRESULT hRes = _Module.Init(NULL, hInstance);
	hRes;

	ATLASSERT(SUCCEEDED(hRes));

	int nRet = Run(lpstrCmdLine, nCmdShow);

	_Module.Term();

	GR::get_instance().Uninitialize();
	CoUninitialize();

	return nRet;
}
