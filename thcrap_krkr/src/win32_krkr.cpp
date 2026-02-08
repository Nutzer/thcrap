/**
  * Touhou Community Reliant Automatic Patcher
  * Kirkiri support plugin
  *
  * ----
  *
  * Kirkiri-specific wrappers around Win32 API functions.
  */

#include <thcrap.h>

/// Detour chains
/// -------------

int WINAPI MultiByteToWideCharSJIS(
	UINT CodePage,
	DWORD dwFlags,
	LPCSTR lpMultiByteStr,
	int cbMultiByte,
	LPWSTR lpWideCharStr,
	int cchWideChar
)
{
	(void)CodePage;
	(void)dwFlags;

#define CODEPAGE_SHIFT_JIS 932
	int ret = MultiByteToWideChar(CODEPAGE_SHIFT_JIS, MB_ERR_INVALID_CHARS,
		lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar
	);
	return ret;
}

void krkr_mod_detour(void)
{
	// XXX: This just overrides the chains set up in win32_utf8.
	//      Kirikiri sends some messages that crash, needs to be investigated further.
	detour_chain("user32.dll", 1, "DefWindowProcA", DefWindowProcA, NULL, NULL);

	// XXX: The internal parser of Kirikiri expects UTF16 input, and files are encoded in shift-jis or utf16.
	//      At some point patching the parser would be cleaner, but this is the easiest solution.
	detour_chain("kernel32.dll", 1, "MultiByteToWideChar", MultiByteToWideCharSJIS, NULL, NULL);
	detour_chain("gdi32.dll", 1, "GetGlyphOutlineA", GetGlyphOutlineA, NULL,
								 "GetTextExtentPoint32A", GetTextExtentPoint32A, NULL, NULL);
}
