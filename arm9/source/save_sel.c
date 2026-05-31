#include <nds.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <dirent.h>

#include "message.h"
#include "GBA_ini.h"
#include "tarosa/tarosa_Graphic.h"
#include "tarosa/tarosa_Shinofont.h"
#include "font_bridge.h"

extern uint16* MainScreen;
extern uint16* SubScreen;
extern int GBAmode;

char   savnam[6][256];
char   savext[6][4];
bool   savexist[6];

static char full_raw[6][256];

static void truncate_utf8(char* dst, const char* src, size_t max_bytes) {
	if (max_bytes == 0) { *dst = 0; return; }
	size_t i = 0;
	while (*src && i < max_bytes - 1) {
		int bytes = 1;
		if ((*src & 0x80) == 0) bytes = 1;
		else if ((*src & 0xE0) == 0xC0) bytes = 2;
		else if ((*src & 0xF0) == 0xE0) bytes = 3;
		else if ((*src & 0xF8) == 0xF0) bytes = 4;
		else { src++; continue; }
		if (i + bytes >= max_bytes) break;
		for (int j = 0; j < bytes; j++) dst[i++] = *src++;
	}
	dst[i] = '\0';
}

static int calc_pixel_width(const char* str) {
	int width = 0;
	while (*str) {
		if ((*str & 0xC0) != 0x80) width += 8;
		str++;
	}
	return width;
}

static void get_scrolled_string(char* dst, size_t dstsize, const char* src,
	int offset_pixels, int max_pixels) {
	if (offset_pixels <= 0) {
		char* d = dst;
		int used = 0;
		while (*src && (d - dst) < dstsize - 1) {
			int bytes = 1;
			if ((*src & 0x80) == 0) bytes = 1;
			else if ((*src & 0xE0) == 0xC0) bytes = 2;
			else if ((*src & 0xF0) == 0xE0) bytes = 3;
			else if ((*src & 0xF8) == 0xF0) bytes = 4;
			else { src++; continue; }
			if (used + 8 > max_pixels) break;
			for (int i = 0; i < bytes; i++) *d++ = *src++;
			used += 8;
		}
		*d = '\0';
		return;
	}
	int skip = offset_pixels;
	const char* p = src;
	while (*p && skip >= 8) {
		int bytes = 1;
		if ((*p & 0x80) == 0) bytes = 1;
		else if ((*p & 0xE0) == 0xC0) bytes = 2;
		else if ((*p & 0xF0) == 0xE0) bytes = 3;
		else if ((*p & 0xF8) == 0xF0) bytes = 4;
		else { p++; continue; }
		p += bytes;
		skip -= 8;
	}
	char* d = dst;
	int used = 0;
	while (*p && (d - dst) < dstsize - 1) {
		int bytes = 1;
		if ((*p & 0x80) == 0) bytes = 1;
		else if ((*p & 0xE0) == 0xC0) bytes = 2;
		else if ((*p & 0xF0) == 0xE0) bytes = 3;
		else if ((*p & 0xF8) == 0xF0) bytes = 4;
		else { p++; continue; }
		if (used + 8 > max_pixels) break;
		for (int i = 0; i < bytes; i++) *d++ = *p++;
		used += 8;
	}
	*d = '\0';
}

static void pixel_truncate(char* out, size_t outsize, const char* in,
	int font_width, int max_pixels) {
	char* d = out;
	int pixel_used = 0;
	while (*in && (d - out) < outsize - 1) {
		int bytes = 1;
		if ((*in & 0x80) == 0) bytes = 1;
		else if ((*in & 0xE0) == 0xC0) bytes = 2;
		else if ((*in & 0xF0) == 0xE0) bytes = 3;
		else if ((*in & 0xF8) == 0xF0) bytes = 4;
		else { in++; continue; }
		if (pixel_used + font_width > max_pixels) break;
		for (int i = 0; i < bytes; i++) *d++ = *in++;
		pixel_used += font_width;
	}
	*d = '\0';
}

void _save_list(char* name) {
	struct stat st;
	char   fname[512];
	char   path[512];
	int    i;

	strcpy(fname, name);
	fname[strlen(name) - 3] = 0;

	strcpy(savext[0], "sav");
	snprintf(path, sizeof(path), "%s/%s%s", ini.save_dir, fname, savext[0]);
	if (stat(path, &st) == 0) {
		savexist[0] = true;
		struct tm* ptime = gmtime(&st.st_mtime);
		int yy = (ptime->tm_year + 1900) % 100;
		int mm = ptime->tm_mon + 1;
		int dd = ptime->tm_mday;
		snprintf(savnam[0], sizeof(savnam[0]), " SAV %s%s(%02d%02d%02d)", fname, savext[0], yy, mm, dd);
	}
	else {
		savexist[0] = false;
		snprintf(savnam[0], sizeof(savnam[0]), " SAV  无文件 ");
	}
	strncpy(full_raw[0], savnam[0], sizeof(full_raw[0]) - 1);
	full_raw[0][sizeof(full_raw[0]) - 1] = '\0';

	for (i = 1; i < 6; i++) {
		snprintf(savext[i], sizeof(savext[i]), "sv%d", i);
		snprintf(path, sizeof(path), "%s/%s%s", ini.save_dir, fname, savext[i]);
		if (stat(path, &st) == 0) {
			savexist[i] = true;
			struct tm* ptime = gmtime(&st.st_mtime);
			int yy = (ptime->tm_year + 1900) % 100;
			int mm = ptime->tm_mon + 1;
			int dd = ptime->tm_mday;
			snprintf(savnam[i], sizeof(savnam[i]), " <%d> %s%s(%02d%02d%02d)", i, fname, savext[i], yy, mm, dd);
		}
		else {
			savexist[i] = false;
			snprintf(savnam[i], sizeof(savnam[i]), " <%d>  无文件 ", i);
		}
		strncpy(full_raw[i], savnam[i], sizeof(full_raw[i]) - 1);
		full_raw[i][sizeof(full_raw[i]) - 1] = '\0';
	}
}

static void draw_normal_entry(int idx, int start_x, int y, int max_pixels) {
	char disp[64];
	pixel_truncate(disp, sizeof(disp), full_raw[idx], 8, max_pixels);
	DrawBox_SUB(SubScreen, start_x, y, start_x + max_pixels, y + 11, 0, 1);
	fontPrintSub(SubScreen, start_x, y + 2, disp, 1, 0);
}

static void draw_highlight_entry(int idx, int start_x, int y, int max_pixels,
	int scroll_offset, int scroll_active) {
	char disp[64];
	if (scroll_active) {
		get_scrolled_string(disp, sizeof(disp), full_raw[idx], scroll_offset, max_pixels);
	}
	else {
		pixel_truncate(disp, sizeof(disp), full_raw[idx], 8, max_pixels);
	}
	DrawBox_SUB(SubScreen, start_x, y, start_x + max_pixels, y + 11, 7, 1);
	int textColor = (GBAmode == 0) ? 8 : 3;
	fontPrintSub(SubScreen, start_x, y + 2, disp, textColor, 7);
}

int save_sel(int mod, char* name) {
	int   cmd = 0;
	u32   ky, repky;
	int   i;
	int   x1, x2;
	int   y1, y2;
	int   xi, yi;
	u16* gback;
	int   gsiz;

	_save_list(name);

	if (ini.multi == 0) {
		name[strlen(name) - 3] = 0;
		strcat(name, savext[0]);
		if (mod == 0 && savexist[cmd] == 0)
			return -1;
		return 0;
	}

	int len = 32;

	int boxColor = (GBAmode == 0) ? 5 : 3;

	x1 = (256 - len * 6) / 2 - 4;
	y1 = 4 * 12;
	x2 = x1 + len * 6 + 5;
	y2 = y1 + 7 * 13 + 8;

	swiWaitForVBlank();
	DrawBox_SUB(SubScreen, 9, 137, 246, 187, 0, 1);

	gsiz = (x2 - x1 + 1) * (y2 - y1 + 1);
	gback = (u16*)malloc(sizeof(u16) * gsiz);
	for (yi = y1; yi < y2 + 1; yi++)
		for (xi = x1; xi < x2 + 1; xi++)
			gback[(xi - x1) + (yi - y1) * (x2 + 1 - x1)] = Point_SUB(SubScreen, xi, yi);

	DrawBox_SUB(SubScreen, x1, y1, x2, y2, boxColor, 0);
	DrawBox_SUB(SubScreen, x1 + 1, y1 + 1, x2 - 1, y2 - 1, 0, 1);
	DrawBox_SUB(SubScreen, x1 + 2, y1 + 2, x2 - 2, y1 + 17, boxColor, 1);
	DrawBox_SUB(SubScreen, x1 + 2, y1 + 19, x2 - 2, y2 - 2, boxColor, 0);

	fontPrintSub(SubScreen, x1 + 3, y1 + 4, savmsg[mod * 3 + 0], 1, boxColor);
	fontPrintSub(SubScreen, 7 * 6, 13 * 12 - 2, savmsg[mod * 3 + 1], 1, 0);
	fontPrintSub(SubScreen, 7 * 6, 14 * 12, savmsg[mod * 3 + 2], 1, 0);

	int start_x = x1 + 3;
	int max_pixels = (x2 - start_x) - 3;
	int line_height = 13;
	int base_y = y1 + 20;

	static int scroll_offset = 0;
	static int scroll_max = 0;
	static int last_cmd = -1;
	static int scroll_active = 0;
	static int scroll_fraction = 0;
	static int scroll_pause_timer = 0;

	last_cmd = -1;
	scroll_offset = 0;
	scroll_fraction = 0;
	scroll_pause_timer = 0;
	scroll_active = 0;
	scroll_max = 0;

	for (i = 0; i < 6; i++) {
		if (i == cmd) {
			draw_normal_entry(i, start_x, base_y + i * line_height, max_pixels);
			if (last_cmd != cmd) {
				scroll_offset = 0;
				scroll_fraction = 0;
				scroll_pause_timer = 0;
				scroll_active = 0;
				scroll_max = 0;
				int total_px = calc_pixel_width(full_raw[i]);
				if (total_px > max_pixels) {
					scroll_active = 1;
					scroll_max = total_px - max_pixels + 8;
				}
				last_cmd = cmd;
				draw_highlight_entry(i, start_x, base_y + i * line_height, max_pixels, scroll_offset, scroll_active);
			}
		}
		else {
			draw_normal_entry(i, start_x, base_y + i * line_height, max_pixels);
		}
	}

	while (1) {
		swiWaitForVBlank();
		scanKeys();

		if (scroll_active && cmd == last_cmd) {
			if (scroll_pause_timer > 0) {
				scroll_pause_timer--;
				if (scroll_pause_timer == 0) {
					scroll_offset = 0;
					scroll_active = 0;
					draw_highlight_entry(cmd, start_x, base_y + cmd * line_height, max_pixels, scroll_offset, scroll_active);
				}
			}
			else {
				scroll_fraction += 128;
				if (scroll_fraction >= 256) {
					scroll_fraction -= 256;
					scroll_offset++;
					if (scroll_offset > scroll_max) {
						scroll_offset = scroll_max;
						scroll_pause_timer = 60;
					}
					draw_highlight_entry(cmd, start_x, base_y + cmd * line_height, max_pixels, scroll_offset, scroll_active);
				}
				else {
					draw_highlight_entry(cmd, start_x, base_y + cmd * line_height, max_pixels, scroll_offset, scroll_active);
				}
			}
		}

		repky = keysDownRepeat();
		if ((repky & KEY_UP) || (repky & KEY_DOWN)) {
			int old_cmd = cmd;
			if (repky & KEY_UP) {
				if (cmd > 0) {
					draw_normal_entry(cmd, start_x, base_y + cmd * line_height, max_pixels);
					cmd--;
				}
			}
			if (repky & KEY_DOWN) {
				if (cmd < 5) {
					draw_normal_entry(cmd, start_x, base_y + cmd * line_height, max_pixels);
					cmd++;
				}
			}
			if (cmd != old_cmd) {
				scroll_offset = 0;
				scroll_fraction = 0;
				scroll_pause_timer = 0;
				scroll_active = 0;
				scroll_max = 0;
				int total_px = calc_pixel_width(full_raw[cmd]);
				if (total_px > max_pixels) {
					scroll_active = 1;
					scroll_max = total_px - max_pixels + 8;
				}
				last_cmd = cmd;
				draw_highlight_entry(cmd, start_x, base_y + cmd * line_height, max_pixels, scroll_offset, scroll_active);
			}
			continue;
		}

		ky = keysDown();
		if (ky & KEY_A) {
			if (mod == 1 || savexist[cmd]) {
				name[strlen(name) - 3] = 0;
				strcat(name, savext[cmd]);
				break;
			}
		}
		if (ky & KEY_B) {
			cmd = -1;
			name[strlen(name) - 3] = 0;
			strcat(name, savext[0]);
			break;
		}
	}

	for (yi = y1; yi < y2 + 1; yi++)
		for (xi = x1; xi < x2 + 1; xi++)
			Pixel_SUB(SubScreen, xi, yi, gback[(xi - x1) + (yi - y1) * (x2 + 1 - x1)]);
	free(gback);

	return cmd;
}