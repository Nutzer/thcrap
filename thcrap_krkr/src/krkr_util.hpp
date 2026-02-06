/**
  * Touhou Community Reliant Automatic Patcher
  * Kirikiri support plugin
  *
  * ----
  *
  * Kirikiri data types
  */

#pragma once
#include <cstdint>

struct krkr_string_t {
	uint32_t refcnt;
	wchar_t* _data;
	wchar_t _inplace[];
};

struct krkr_layer_image_t {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uintptr_t pixel_data;
};

wchar_t* krkr_string_data(krkr_string_t& string);
char* krkr_string_to_utf8(krkr_string_t& string);

krkr_layer_image_t krkr_parse_layer(uintptr_t base);
