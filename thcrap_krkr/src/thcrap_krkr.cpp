/**
  * Touhou Community Reliant Automatic Patcher
  * Kirikiri support plugin
  *
  * ----
  *
  * Plugin setup.
  */

#include <thcrap.h>
// (for TL note removal hooks)
#include <tlnote.hpp>
#include <commctrl.h>
#include "thcrap_krkr.h"

int TH_STDCALL thcrap_plugin_init()
{
	if(stack_check_if_unneeded("base_krkr")) {
		return 1;
	}
	return 0;
}

int InitDll(HMODULE hDll)
{
	return 0;
}

// Yes, this _has_ to be included in every project.
// Visual C++ won't use it when imported from a library
// and just defaults to msvcrt's one in this case.
BOOL APIENTRY DllMain(HMODULE hDll, DWORD ulReasonForCall, LPVOID lpReserved)
{
	switch(ulReasonForCall) {
		case DLL_PROCESS_ATTACH:
			InitDll(hDll);
			break;
	}
	return TRUE;
}
