/**
  * Touhou Community Reliant Automatic Patcher
  * Kirikiri support plugin
  *
  * ----
  *
  * Breakpoints for image replacement.
  */

#include <thcrap.h>
#include <algorithm>

#include <png.h>
#include "png_ex.h"
#include "krkr_util.hpp"

size_t BP_patch_image(x86_reg_t *regs, json_t *bp_info)
{
	// This breakpoints hooks into the Layer.loadImages()-function
	// after the images have been loaded, and patches the raw RGBA-data.

	uintptr_t path_ptr = (uintptr_t)json_object_get_pointer(bp_info, regs, "filename");
	uintptr_t image_ptr = (uintptr_t)json_object_get_pointer(bp_info, regs, "image");


	*(uint8_t*)(image_ptr + 0x140) = 1; // Breakpoint overrides this instruction.

	if (path_ptr < 0x1000) {
		// XXX: There is one case in gtk4.exe where this happens.
		return 1;
	}

	krkr_string_t *path_str = *(krkr_string_t**)path_ptr;
	krkr_layer_image_t layer = krkr_parse_layer(image_ptr);

	char *path = krkr_string_to_utf8(*path_str);
	FORMAT_VLA_STR(char, fname, "%s.%s", path, strstr(path, ".png") ? "" : ".png");
	char **chain = resolve_chain_game(fname);
	stack_chain_iterate_t sci;
	sci.fn = NULL;
	while (stack_chain_iterate(&sci, chain, SCI_BACKWARDS)) {
		png_image_ex png = {};

		png.img.version = PNG_IMAGE_VERSION;

		size_t file_size;
		void *file_buffer = patch_file_load(sci.patch_info, sci.fn, &file_size);
		if (!file_buffer) {
			continue;
		}

		if (png_image_begin_read_from_memory(&png.img, file_buffer, file_size)) {
			png.img.format = PNG_FORMAT_BGRA;
			if (png.img.format != PNG_FORMAT_INVALID) {
				size_t png_size = PNG_IMAGE_SIZE(png.img);
				png.buf = (png_bytep)malloc(png_size);

				if (png.buf) {
					png_image_finish_read(&png.img, 0, png.buf, 0, NULL);
				}
			}
		}
		free(file_buffer);

		// XXX: At some point, the blitting from thanm should be factored out of thanm and generalized.
		//      Then, we can use it here.
		if (png.buf) {
			if (png.img.width != layer.width || png.img.height != layer.height) {
				log_printf("invalid size for replacement image, expected %i/%i, got %i/%i\n",
					layer.width, layer.height, png.img.width, png.img.height);
			}
			else {
				// image needs to be flipped
				size_t stride = layer.width  *4;
				for (uint32_t row = 0; row < layer.height; ++row) {
					memcpy((void*)(layer.pixel_data + (row  *layer.stride)),
						(void*)((uintptr_t)png.buf + ((layer.height - row - 1)  *stride)),
						stride);
				}
				break;
			}
			free(png.buf);
		}
	}

	chain_free(chain);
	VLA_FREE(fname);
	free(path);

	return 1;
}
