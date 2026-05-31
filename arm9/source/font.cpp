#include "font.h"
#include "tonccpy.h"

#include <algorithm>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <nds.h>

extern "C" void Pixel_SUB(uint16* screen, short x, short y, uint16 color);

u8 Font::textBuf[2][256 * 192];
bool Font::mainScreen = false;

Font* font = nullptr;

bool Font::isArabic(char16_t c) {
	return c >= 0x0622 && c <= 0x064A;
}

bool Font::isStrongRTL(char16_t c) {
	return (c >= 0x0590 && c <= 0x05FF) || (c >= 0x0600 && c <= 0x06FF) || c == 0x200F;
}

bool Font::isWeak(char16_t c) {
	return c < 'A' || (c > 'Z' && c < 'a') || (c > 'z' && c < 127);
}

bool Font::isNumber(char16_t c) {
	return c >= '0' && c <= '9';
}

bool Font::isLoaded() const {
	return fontTiles != nullptr && fontMap != nullptr;
}

char16_t Font::arabicForm(char16_t current, char16_t prev, char16_t next) {
	if (isArabic(current)) {
		if ((prev >= 0x626 && prev <= 0x62E && prev != 0x627 && prev != 0x629) || (prev >= 0x633 && prev <= 0x64A && prev != 0x648)) {
			if (isArabic(next))
				return arabicPresentationForms[current - 0x622][1];
			else
				return arabicPresentationForms[current - 0x622][2];
		}
		else {
			if (isArabic(next))
				return arabicPresentationForms[current - 0x622][0];
			else
				return current;
		}
	}
	return current;
}

bool Font::parseData(const u8* ptr, size_t size) {
	const u8* end = ptr + size;

	if (size < 12 || memcmp(ptr, "RIFF", 4) != 0) return false;
	ptr += 8;

	// META section
	if (ptr + 8 > end || memcmp(ptr, "META", 4) != 0) return false;
	tileWidth = ptr[8];
	tileHeight = ptr[9];
	tonccpy(&tileCount, ptr + 10, sizeof(u16));
	if (tileWidth > TILE_MAX_WIDTH || tileHeight > TILE_MAX_HEIGHT) return false;
	u32 section_size;
	tonccpy(&section_size, ptr + 4, sizeof(u32));
	ptr += 8 + section_size;

	// CDAT section
	if (ptr + 8 > end || memcmp(ptr, "CDAT", 4) != 0) return false;
	tonccpy(&section_size, ptr + 4, sizeof(u32));
	ptr += 8;
	if (ptr + section_size > end) return false;
	fontTiles = new u8[tileHeight * tileCount];
	if (!fontTiles) return false;
	tonccpy(fontTiles, ptr, tileHeight * tileCount);
	ptr += section_size;

	// CMAP section
	if (ptr + 8 > end || memcmp(ptr, "CMAP", 4) != 0) return false;
	tonccpy(&section_size, ptr + 4, sizeof(u32));
	ptr += 8;
	if (ptr + section_size > end) return false;
	fontMap = new u16[tileCount];
	if (!fontMap) return false;
	tonccpy(fontMap, ptr, sizeof(u16) * tileCount);
	ptr += section_size;

	questionMark = getCharIndex('?');
	return true;
}

bool Font::load(const char* path) {
	if (!path) return false;
	FILE* file = fopen(path, "rb");
	if (!file) return false;
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	u8* fileBuffer = new u8[size];
	if (!fileBuffer) { fclose(file); return false; }
	fseek(file, 0, SEEK_SET);
	fread(fileBuffer, 1, size, file);
	fclose(file);

	bool result = parseData(fileBuffer, size);
	delete[] fileBuffer;
	return result;
}

bool Font::loadFromMemory(const u8* data, size_t size) {
	return parseData(data, size);
}

Font::Font() {
	tileWidth = 0;
	tileHeight = 0;
	tileCount = 0;
	questionMark = 0;
	fontTiles = nullptr;
	fontMap = nullptr;
}

Font::Font(const char* path) {
	load(path);
}

Font::~Font(void) {
	if (fontTiles) delete[] fontTiles;
	if (fontMap)   delete[] fontMap;
}

u16 Font::getCharIndex(char16_t c) {
	int left = 0, right = tileCount;
	while (left <= right) {
		int mid = left + ((right - left) / 2);
		if (fontMap[mid] == c) return mid;
		if (fontMap[mid] < c) left = mid + 1;
		else right = mid - 1;
	}
	return questionMark;
}

std::u16string Font::utf8to16(std::string_view text) {
	std::u16string out;
	for (uint i = 0; i < text.size();) {
		char16_t c;
		if (!(text[i] & 0x80)) {
			c = text[i++];
		}
		else if ((text[i] & 0xE0) == 0xC0) {
			c = (text[i++] & 0x1F) << 6;
			c |= text[i++] & 0x3F;
		}
		else if ((text[i] & 0xF0) == 0xE0) {
			c = (text[i++] & 0x0F) << 12;
			c |= (text[i++] & 0x3F) << 6;
			c |= text[i++] & 0x3F;
		}
		else {
			i++;
			continue;
		}
		out += c;
	}
	return out;
}

int Font::calcHeight(std::u16string_view text, int xPos) {
	int lines = 1, chars = xPos + 1;
	for (auto it = text.begin(); it != text.end(); it++) {
		if (*it == '\n' || (*it == ' ' && 256 / tileWidth - chars < 10 && text.end() - it >(256 / tileWidth - chars) && *std::find(it + 1, std::min(it + (256 / tileWidth - chars), text.end()), ' ') != ' ')) {
			lines++; chars = xPos + 1;
		}
		else if (chars > 256 / tileWidth) {
			lines++; chars = xPos + 1;
			if (*it == ' ') it++;
		}
		else chars++;
	}
	return lines;
}

void Font::printf(int xPos, int yPos, bool top, Alignment align, Palette palette, const char* format, ...) {
	char str[0x100];
	va_list va;
	va_start(va, format);
	vsniprintf(str, 0x100, format, va);
	va_end(va);
	print(xPos, yPos, top, str, align, palette);
}

ITCM_CODE void Font::print(int xPos, int yPos, bool top, std::u16string_view text, Alignment align, Palette palette, bool noWrap, bool rtl) {
	int x = xPos * tileWidth, y = yPos * tileHeight;
	if (x < 0 && align != Alignment::center) x += 256;
	if (y < 0) y += 192;

	if (!rtl) {
		for (const auto c : text) {
			if (isStrongRTL(c)) { rtl = true; break; }
		}
	}
	auto ltrBegin = text.end(), ltrEnd = text.end();

	switch (align) {
	case Alignment::left: break;
	case Alignment::center: {
		size_t newline = text.find('\n');
		while (newline != text.npos) {
			print(xPos, yPos, top, text.substr(0, newline), align, palette, rtl);
			text = text.substr(newline + 1);
			newline = text.find('\n');
			yPos++; y += tileHeight;
		}
		x = ((256 - (text.length() * tileWidth)) / 2) + x;
		break;
	}
	case Alignment::right: {
		if (!noWrap) {
			int cols = SCREEN_COLS;
			for (auto it = text.begin(); it < text.end(); ++it) {
				int idx = std::distance(text.begin(), it);
				std::u16string_view substr;
				if (idx >= cols) {
					substr = text.substr(0, idx);
					text = text.substr(idx);
				}
				else if (*it == '\n' || (*it == ' ' && (cols - idx) < 10 && std::distance(it, text.end()) > (cols - idx) && *std::find(it + 1, std::min(it + (cols - idx), text.end()), ' ') != ' ')) {
					substr = text.substr(0, idx);
					text = text.substr(idx + ((*it == ' ' || *it == '\n') ? 1 : 0));
				}
				else continue;
				print(xPos - substr.length() + 1, yPos, top, substr, Alignment::left, palette, rtl);
				yPos++; y += tileHeight;
			}
		}
		break;
	}
	}

	if (align == Alignment::right) x -= (text.length() - 1) * tileWidth;
	x -= x % tileWidth; y -= y % tileHeight;
	x += (256 % tileWidth) / 2; y += (192 % tileHeight) / 2;
	const int xStart = x;

	for (auto it = (rtl ? text.end() - 1 : text.begin()); true; it += (rtl ? -1 : 1)) {
		if (it == (rtl ? text.begin() - 1 : text.end())) {
			if (ltrBegin == text.end() || (ltrBegin == text.begin() && ltrEnd == text.end())) break;
			else { it = ltrBegin; ltrBegin = text.end(); rtl = true; }
		}
		if (it == ltrEnd && ltrBegin != text.end()) {
			if (ltrBegin == text.begin() && (!isWeak(*ltrBegin) || isNumber(*ltrBegin))) break;
			it = ltrBegin; ltrBegin = text.end(); rtl = true;
		}
		else if (rtl && !isStrongRTL(*it) && (!isWeak(*it) || isNumber(*it))) {
			ltrEnd = it + 1;
			bool allNumbers = true;
			while (!isStrongRTL(*it) && it != text.begin()) {
				if (allNumbers && !isNumber(*it) && !isWeak(*it)) allNumbers = false;
				it--;
			}
			ltrBegin = it;
			if (isStrongRTL(*it)) it++;
			if (allNumbers) while (isWeak(*it) && !isNumber(*it)) { if (it != text.begin()) ltrBegin++; it++; }
			else while (isWeak(*it)) { if (it != text.begin()) ltrBegin++; it++; }
			while ((it - 1 >= text.begin() && isNumber(*(it - 1))) || (it - 2 >= text.begin() && isWeak(*(it - 1)) && isNumber(*(it - 2)))) {
				if (it - 1 != text.begin()) ltrBegin--;
				it--;
			}
			rtl = false;
		}

		if (*it == '\n' || (*it == ' ' && align == Alignment::left && 256 - x < tileWidth * 10 && text.end() - it >(256 - x) / tileWidth && *std::find(it + 1, std::min(it + (256 - x) / tileWidth, text.end()), ' ') != ' ')) {
			x = xStart; y += tileHeight;
			if (noWrap) break; else continue;
		}
		if (x + tileWidth > 256 && align == Alignment::left) {
			x = xStart; y += tileHeight;
			if (*it == ' ') it++;
			if (noWrap) break;
		}

		u16 index;
		if (rtl) {
			switch (*it) {
			case '(': index = getCharIndex(')'); break;
			case ')': index = getCharIndex('('); break;
			case '[': index = getCharIndex(']'); break;
			case ']': index = getCharIndex('['); break;
			case '<': index = getCharIndex('>'); break;
			case '>': index = getCharIndex('<'); break;
			case u'ا':
				if (it > text.begin() && *(it - 1) == u'ل') {
					index = getCharIndex(arabicForm(u'ﻻ', it - 1 > text.begin() ? *(it - 2) : 0, it < text.end() - 1 ? *(it + 1) : 0));
					--it; break;
				}
			default:
				index = getCharIndex(arabicForm(*it, it > text.begin() ? *(it - 1) : 0, it < text.end() - 1 ? *(it + 1) : 0));
				break;
			}
		}
		else index = getCharIndex(*it);

		if (x >= 0 && x + tileWidth <= 256 && y >= 0 && y + tileHeight <= 192) {
			u8* dst = textBuf[top] + x;
			for (int i = 0; i < tileHeight; i++) {
				u8 px = fontTiles[(index * tileHeight) + i];
				for (int j = 0; j < tileWidth; j++)
					dst[(y + i) * 256 + j] = u8(palette) * 0x10 + ((px >> (7 - j)) & 1);
			}
		}
		x += tileWidth;
	}
}

void Font::printString(u16* screen, int x, int y, const char* str, u16 fg, u16 bg) {
	if (!str || !screen || !fontTiles || !fontMap) return;
	int curX = x, curY = y;
	while (*str) {
		char16_t c;
		if (!(*str & 0x80)) c = *str++;
		else if ((*str & 0xE0) == 0xC0) { c = (*str++ & 0x1F) << 6; c |= (*str++ & 0x3F); }
		else if ((*str & 0xF0) == 0xE0) { c = (*str++ & 0x0F) << 12; c |= (*str++ & 0x3F) << 6; c |= (*str++ & 0x3F); }
		else { str++; continue; }
		u16 index = getCharIndex(c);
		for (int row = 0; row < tileHeight; row++) {
			if (curY + row < 0 || curY + row >= 192) continue;
			u8 tileRow = fontTiles[index * tileHeight + row];
			for (int col = 0; col < tileWidth; col++) {
				if (curX + col < 0 || curX + col >= 256) continue;
				u16 color = ((tileRow >> (7 - col)) & 1) ? fg : bg;
				screen[(curY + row) * 256 + (curX + col)] = color;
			}
		}
		curX += tileWidth;
	}
}

void Font::drawTileSub(u16* screen, int x, int y, const char* str, u16 fg, u16 bg) {
	if (!screen || !str || !fontTiles) return;
	int curX = x, curY = y;
	while (*str) {
		char16_t c;
		if (!(*str & 0x80)) c = *str++;
		else if ((*str & 0xE0) == 0xC0) { c = (*str++ & 0x1F) << 6; c |= (*str++ & 0x3F); }
		else if ((*str & 0xF0) == 0xE0) { c = (*str++ & 0x0F) << 12; c |= (*str++ & 0x3F) << 6; c |= (*str++ & 0x3F); }
		else { str++; continue; }
		u16 index = getCharIndex(c);
		for (int row = 0; row < tileHeight; row++) {
			u8 tileRow = fontTiles[index * tileHeight + row];
			for (int col = 0; col < tileWidth; col++) {
				bool bit = (tileRow >> (7 - col)) & 1;
				Pixel_SUB(screen, curX + col, curY + row, bit ? fg : bg);
			}
		}
		curX += tileWidth;
	}
}