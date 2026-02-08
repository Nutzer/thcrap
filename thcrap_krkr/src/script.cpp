/**
  * Touhou Community Reliant Automatic Patcher
  * Kirikiri support plugin
  *
  * ----
  *
  * Breakpoints for patching script and scenario files.
  */

#include <map>
#include <string.h>
#include <thcrap.h>

#include "krkr_util.hpp"
#include <algorithm>
#include <sha256.h>
#include <mutex>

char *perform_patch(char *filename_full, char *content);


// Kirikiri uses a custom allocator.
//   When we swap in patched strings, we need to swap them back
//   before the original gets free'd or bad things will happen.
std::map<wchar_t*, wchar_t*> patch_replacements;
std::mutex patch_replacements_lock;

void patch_string(krkr_string_t *string, char *patch_utf8)
{
	std::scoped_lock lock{ patch_replacements_lock };

	wchar_t *patch_utf16 = (wchar_t*)utf8_to_utf16(patch_utf8);
	patch_replacements.emplace(patch_utf16, string->_data);
	string->_data = patch_utf16;
}

void unpatch_string(krkr_string_t *string)
{
	std::scoped_lock lock{ patch_replacements_lock };

	auto it = patch_replacements.find(string->_data);
	if (it != patch_replacements.end()) {
		string->_data = it->second;
		patch_replacements.erase(it);
	}
}

size_t BP_patchtjs(x86_reg_t *regs, json_t *bp_info)
{
	// This breakpoint hooks into the tjs parser, after the script has been read.

	krkr_string_t *filename_str = *(krkr_string_t**)((uintptr_t)regs->ebp - 0x14);
	krkr_string_t *content_str = *(krkr_string_t**)((uintptr_t)regs->ebp - 0xc);

	char *filename = krkr_string_to_utf8(*filename_str);
	char *content = krkr_string_to_utf8(*content_str);
	char *patched = perform_patch(filename, content);
	patch_string(content_str, patched);

	free(patched);
	free(filename);
	free(content);

	*(uint16_t*)(regs->ebp - 0x30) = 0x20; // Breakpoint overrides this instruction
	return 1;
}

size_t BP_patchks(x86_reg_t *regs, json_t *bp_info)
{
	// This breakpoint hooks into the ks parser, after the script has been read.

	krkr_string_t *filename_str = **(krkr_string_t***)((uintptr_t)regs->esp + 0x14);
	krkr_string_t *content_str = *(krkr_string_t**)((uintptr_t)regs->ebp - 0x4);

	char *filename = krkr_string_to_utf8(*filename_str);
	char *content = krkr_string_to_utf8(*content_str);
	char *patched = perform_patch(filename, content);
	patch_string(content_str, patched);

	free(patched);
	free(filename);
	free(content);
	return 1;
}

size_t BP_string_free(x86_reg_t *regs, json_t *bp_info)
{
	// This breakpoint hooks into the code that handles string cleanup

	krkr_string_t *str = (krkr_string_t*)((uintptr_t)regs->ebx);

	unpatch_string(str);

	return 1;
}


char *perform_patch(char *filename_full, char *content) {
	// TJS paths in Kirikiri internally use the schema path/to/archive.xp3>local/path
	// We don't care which archive is used, so we need to extract the local path part.
	char *fname_local = filename_full;
	char *archive_path_end;
	while (archive_path_end = strchr(fname_local, '>')) {
		fname_local = archive_path_end + 1;
	}

	char *filename_js = strdup_cat(fname_local, ".jdiff");
	json_t *patch_index = jsondata_game_get(filename_js);
	if (!patch_index) {
		jsondata_game_add(filename_js);
		patch_index = jsondata_game_get(filename_js);
	}
	json_t *strings = jsondata_game_get("strings.js");
	

	SHA256_HASH hash = sha256_calc((uint8_t*)content, strlen(content));
	sha256_str_t hash_str;
	sha256_to_string(hash, hash_str);
	json_t *patch = json_object_get(patch_index, hash_str);

	static bool INCOMPATIBLE_WARNING_SHOWN = false;
	if (patch_index && !patch && !INCOMPATIBLE_WARNING_SHOWN) {
		INCOMPATIBLE_WARNING_SHOWN = true;
		log_error_mboxf("Unsupported game version",
			"This version of the game is not supported by this patch. "
			"Running the game in this state will result in missing features / translations. "
			"If you have a static english patch installed, please uninstall it and try again.\n"
			"Filename: %s\n"
			"Hash: %s\n",
			fname_local, hash_str
		);
	}

	// Collect patches from jdiff and sort them by start byte.
	// Patches are in the format @start_byte,end_byte
	// ----------
	struct patch_t {
		uintptr_t start, end;
		const char *rep;
	};
	std::vector<patch_t> patches;
	const char *key;
	json_t const *value;
	json_object_foreach(patch, key, value) {
		if (!json_is_string(value) || strlen(key) < 4 || *key != '@') {
			log_printf("invalid patch key: %s\n", key);
			continue;
		}
		patch_t res;
		char *cont;
		res.start = strtoull(key + 1, &cont, 10);
		if (*cont != ',') {
			log_printf("invalid patch key: %s\n", key);
			continue;
		}
		res.end = strtoull(cont + 1, NULL, 10);
		res.rep = json_string_value(value);
		patches.push_back(res);
	}
	std::sort(patches.begin(), patches.end(), [](auto& a, auto& b) { return a.start < b.start; });

	// Apply patches
	// ----------
	char *content_patched = strdup("");
	uintptr_t content_pos = 0;
	auto add_patched_content = [&content_patched](const char *text, size_t length) {
		content_patched = (char*)realloc(content_patched, strlen(content_patched) + length + 1);
		strncat(content_patched, text, length);
	};

	for (auto& p : patches) {
		if (content_pos > p.start) {
			log_printf("Overlapping patches @%u, skipping\n", p.start);
			continue;
		}
		if (strlen(content) < p.end) {
			log_printf("patch exceeds document @%u, skipping\n", p.start);
			continue;
		}
		add_patched_content(content + content_pos, p.start - content_pos);

		// Apply string references (e.g. [@ref])
		// ----------
		const char *text = p.rep;
		const char *string_offset;
		while ((string_offset = strstr(text, "[@"))) {
			const char *string_end = strchr(string_offset, ']');
			if (string_end == NULL) {
				log_printf("Unterminated string reference in patch: %s\n", string_offset);
				add_patched_content(text, string_offset - text + 1);
				text = string_offset + 1;
				continue;
			}

			char *string_name = (char*)malloc(string_end - string_offset - 1);
			memcpy(string_name, string_offset + 2, string_end - string_offset - 2);
			string_name[string_end - string_offset - 2] = 0;
			const char *string_rep = json_object_get_string(strings, string_name);
			free(string_name);
			if (string_rep == NULL) {
				log_printf("String reference not found: %.*s\n", string_end - string_offset - 2, string_offset + 2);
				add_patched_content(text, string_offset - text + 1);
				text = string_offset + 1;
				continue;
			}

			add_patched_content(text, string_offset - text);
			add_patched_content(string_rep, strlen(string_rep));
			text = string_end + 1;
		}
		add_patched_content(text, strlen(text));
		content_pos = p.end + 1;
	}
	add_patched_content(content + content_pos, strlen(content) - content_pos);

	SAFE_FREE(filename_js);
	return content_patched;
}

void script_mod_init(void)
{
	jsondata_game_add("strings.js");
}
