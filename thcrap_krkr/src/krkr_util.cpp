/**
  * Touhou Community Reliant Automatic Patcher
  * Kirikiri support plugin
  *
  * ----
  *
  * Kirikiri data types
  */

#include <string>
#include <thcrap.h>
#include "krkr_util.hpp"

wchar_t *krkr_string_data(
	krkr_string_t& string
)
{
	if (string._data)
		return string._data;
	return string._inplace;
}


char *krkr_string_to_utf8(
	krkr_string_t& string
)
{
	wchar_t *data = krkr_string_data(string);
	size_t len = std::char_traits<wchar_t>::length(data)  *4;
	char *utf8 = (char*)malloc(len);
	StringToUTF8(utf8, data, len);
	return utf8;
}

krkr_layer_image_t krkr_parse_layer(
	uintptr_t base
)
{
	uintptr_t image = *(uintptr_t*)(base + 0xc4);
	uintptr_t imageData = *(uintptr_t*)(image + 0x7c);

	krkr_layer_image_t res = {};
	res.pixel_data = *(uintptr_t*)(imageData + 0x4);
	res.stride = *(uintptr_t*)(imageData + 0x10);
	res.width = *(uint32_t*)(imageData + 0x18);
	res.height = *(uint32_t*)(imageData + 0x1c);
	return res;
}

