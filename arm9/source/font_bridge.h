#ifndef FONT_BRIDGE_H
#define FONT_BRIDGE_H

#include <nds/ndstypes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

	void initFont(const char* path);
	void initFontMem(const u8* data, size_t size);
	bool isFontLoaded(void);
	void fontPrintC(u16* screen, int px, int py, const char* str, u16 fg, u16 bg);

	void fontPrintSub(u16* screen, int px, int py, const char* str, u16 fg, u16 bg);

#ifdef __cplusplus
}
#endif

#endif