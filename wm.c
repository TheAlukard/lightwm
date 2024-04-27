#include <windows.h>
#include "tiling.h"
#include "error.h"
#include "config.h"
#include "keyboard.h"
#include "messages.h" 
#include "shared_mem.h"

#define EXIT_OK 0
#define EXIT_FAILED 1

HMODULE wmDll;
HHOOK hookShellProcHandle;

void cleanupObjects()
{
	cleanupKeyboard();
	
	if (hookShellProcHandle) {
		UnhookWindowsHookEx(hookShellProcHandle);
	}
	
	if (wmDll) {
		FreeLibrary(wmDll);
	}

	cleanupMemoryMapFile();
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, PWSTR cmdLine, int cmdShow)
{
	int exitCode = EXIT_OK;

    SetProcessDPIAware();

	if (!initializeKeyboardConfig()) {
		reportGeneralError(L"Setup keyboard config"); 
		goto cleanup; 
	}

	if (!storeDwordInSharedMemory(GetCurrentThreadId())) {
		reportGeneralError(L"Failed writing thread id to shared memory");
		goto cleanup;
	}

	wmDll = LoadLibraryW(L"lightwm_dll");
	
	if (wmDll == NULL) {
		reportWin32Error(L"LoadLibrary of lightwm_dll"); 
		goto cleanup;
	}
	
	FARPROC shellProc = GetProcAddress(wmDll, "ShellProc");

	if (shellProc == NULL) { 
		reportWin32Error(L"GetProcAddress for ShellProc");
		goto cleanup; 
	}
	
	hookShellProcHandle = SetWindowsHookExW(WH_SHELL, (HOOKPROC)shellProc, wmDll, 0);

	if (hookShellProcHandle == NULL) {
		reportWin32Error(L"SetWindowsHookExW for shell hook");
		goto cleanup;
	}

	tileWindows();

	MSG msg;

	while (GetMessage(&msg, (HWND)-1, 0, 0) != 0) {
		switch (msg.message) {
			case WM_HOTKEY:
				if (msg.wParam == QUIT_LIGHTWM_HOTKEY_ID) {
					goto cleanup;
				}

				handleHotkey(msg.wParam, msg.lParam);
				break; 
			case LWM_WINDOW_EVENT:
				tileWindows();
				break;
		}
	}

	exitCode = EXIT_FAILED;

cleanup:
	cleanupObjects();
	
	return exitCode;
}
