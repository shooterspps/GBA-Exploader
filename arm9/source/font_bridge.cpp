#include "font_bridge.h"
#include "font.h"

Font* g_font = nullptr;

extern "C" {

	void initFont(const char* path) {
		if (!g_font) {
			g_font = new Font(path);
		}
	}

	void initFontMem(const u8* data, size_t size) {
		if (!g_font) {
			g_font = new Font();
			if (!g_font->loadFromMemory(data, size)) {
				delete g_font;
				g_font = nullptr;
			}
		}
	}

	bool isFontLoaded(void) {
		return g_font != nullptr && g_font->isLoaded();
	}

	void fontPrintC(u16* screen, int px, int py, const char* str, u16 fg, u16 bg) {
		if (g_font && g_font->isLoaded()) {
			g_font->printString(screen, px, py, str, fg, bg);
		}
	}

	void fontPrintSub(u16* screen, int px, int py, const char* str, u16 fg, u16 bg) {
		if (g_font && g_font->isLoaded()) {
			g_font->drawTileSub(screen, px, py, str, fg, bg);
		}
	}

} // extern "C"