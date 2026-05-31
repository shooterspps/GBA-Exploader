/*---------------------------------------------------------------------------------
	$Id: template.c,v 1.4 2005/09/17 23:15:13 wntrmute Exp $

	Basic Hello World

	$Log: template.c,v $
	Revision 1.4  2005/09/17 23:15:13  wntrmute
	corrected iprintAt in templates

	Revision 1.3  2005/09/05 00:32:20  wntrmute
	removed references to IPC struct
	replaced with API functions

	Revision 1.2  2005/08/31 01:24:21  wntrmute
	updated for new stdio support

	Revision 1.1  2005/08/03 06:29:56  wntrmute
	added templates
---------------------------------------------------------------------------------*/
#include "nds.h"
#include <nds/arm9/console.h>
#include <nds/ndstypes.h>
#include <nds/fifocommon.h>
#include <nds/fifomessages.h>

#include <fat.h>
#include <sys/dir.h>
#include <nds/arm9/dldi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tarosa/tarosa_Graphic.h"
#include "tarosa/tarosa_Shinofont.h"

#include "ret_menu9_gen.h"
#include "dsCard.h"

#include "GBA_ini.h"
#include "ctrl_tbl.h"
#include "skin.h"
#include "message.h"
#include "tonccpy.h"

// ---- Chinese font bridge ----
#include "font_bridge.h"
extern const u8 misaki_gothic_8x8_bin[];
extern const u8 misaki_gothic_8x8_bin_end[];
extern uint16* MainScreen;
extern uint16* SubScreen;

#define BG_256_COLOR (BIT(7))

#define VERSTRING "v0.67"

int	numFiles = 0;
int	numGames = 0;

char curpath[256];

int	sortfile[200];

struct GBA_File fs[200];
char tbuf[512];
char filename[512];

u8* rwbuf;

int	GBAmode;
bool softReset;

u16* gbar = NULL;
int	oldper;

extern bool checkSRAM_cnf();
extern int checkSRAM(char* name);
extern int carttype;
extern bool isSuperCard;
extern bool is3in1Plus;
extern bool isOmega;
extern bool isOmegaDE;
extern u16 gl_ingame_RTC_open_status;
extern void SetSDControl(u16 control);
extern bool ret_menu_chk(void);
extern void setGBAmode(int sel);
extern void getGBAmode(void);
extern int writeFileToNor(int sel);
extern int writeFileToRam(int sel);
extern void writeSramToFile(char* name);
extern void writeSramFromFile(char* name);
extern void SRAMdump(int cmd);
extern bool checkBackup(void);
extern bool checkFlashID(void);
extern u32 SaveType;
extern u32 SaveSize;
extern u8 SaveVer[];
extern int PatchCnt;
extern u32 PatchType[];
extern u32 PatchAddr[];
extern void setcurpath(void);
extern void getcurpath(void);
extern void FileListGBA(void);
extern int save_sel(int mod, char* name);
extern void setLang(void);
extern int runNDSFile(char tbuf[], char* iniPath, char* curPathName, char* ndsName, char* savName, bool isHomebrew);

static int g_scroll_offset = 0;
static int g_scroll_max = 0;
static int g_last_sel = -1;
static int g_scroll_active = 0;
static int g_scroll_redraw = 0;
static int g_scroll_fraction = 0;
static int g_scroll_pause_timer = 0;

void fontPrint(u16* screen, int px, int py, const char* str, u16 fg, u16 bg) {
	if (screen == SubScreen) {
		if (isFontLoaded()) {
			fontPrintSub(SubScreen, px, py, str, fg, bg);
		}
		else {
			ShinoPrint_SUB(SubScreen, px, py, (u8*)str, fg, bg, 1);
		}
		return;
	}

	u16 fgColor = (fg > 15) ? fg : BG_PALETTE[fg];
	u16 bgColor = (bg > 15) ? bg : BG_PALETTE[bg];

	if (isFontLoaded()) {
		fontPrintC(screen, px, py, str, fgColor, bgColor);
	}
	else {
		ShinoPrint(MainScreen, px, py, (u8*)str, fg, bg, 1);
	}
}

u32 inp_key() {
	u32	ky;

	while (1) {
		swiWaitForVBlank();
		scanKeys();
		ky = keysDown();
		if (ky & KEY_A)break;
		if (ky & KEY_B)break;
	}
	while (1) {
		swiWaitForVBlank();
		scanKeys();
		if (keysHeld() != ky)break;
	}
	return(ky);
}

void turn_off(bool softReset) {
	if (softReset) {
		if (!ret_menu9_Gen())systemShutDown();
	}
	else {
		systemShutDown();
	}
	while (1)swiWaitForVBlank();
}

void gba_frame(int Sel) {
	int	ret;
	int mode = 3; // old mode == 2

	if (Sel != -1) {
		int nameLength = strlen(fs[Sel].filename);
		if (nameLength > 4) {
			if (((fs[Sel].filename[(nameLength - 4)] == '.') &&
				(fs[Sel].filename[(nameLength - 3)] == 'G') &&
				(fs[Sel].filename[(nameLength - 2)] == 'B') &&
				(fs[Sel].filename[(nameLength - 1)] == 'A')
				) || (
					(fs[Sel].filename[(nameLength - 4)] == '.') &&
					(fs[Sel].filename[(nameLength - 3)] == 'g') &&
					(fs[Sel].filename[(nameLength - 2)] == 'b') &&
					(fs[Sel].filename[(nameLength - 1)] == 'a')
					)
				) {
				fs[Sel].filename[(nameLength - 3)] = 'b';
				fs[Sel].filename[(nameLength - 2)] = 'm';
				fs[Sel].filename[(nameLength - 1)] = 'p';
				sprintf(tbuf, "%s/%s", ini.sign_dir, fs[Sel].filename);
				if (access(tbuf, F_OK) == 0) {
					ret = LoadSkin(mode, tbuf);
					if (ret)return;
				}
			}
		}
	}

	if (access("/gbaframe.bmp", F_OK) == 0) {
		ret = LoadSkin(mode, "/gbaframe.bmp");
		if (ret)return;
	}

	sprintf(tbuf, "%s/gbaframe.bmp", ini.sign_dir);
	if (access(tbuf, F_OK) == 0) {
		ret = LoadSkin(mode, tbuf);
		if (ret)return;
	}

	if (access("/_system_/gbaframe.bmp", F_OK) == 0) {
		ret = LoadSkin(mode, "/_system_/gbaframe.bmp");
		if (ret)return;
	}

	if (access("/ttmenu/gbaframe.bmp", F_OK) == 0) {
		ret = LoadSkin(mode, "/ttmenu/gbaframe.bmp");
		if (ret)return;
	}
}

static void resetToSlot2() {
	vu32 vr;
	*((vu32*)0x027FFE08) = (u32)0xE59FF014;
	*((vu32*)0x027FFE24) = (u32)0x027FFE08;
	*((vu32*)0x027FFE34) = (u32)0x080000C0;

	sysSetCartOwner(BUS_OWNER_ARM7);

	fifoSendValue32(FIFO_USER_02, 1);

	for (vr = 0; vr < 0x20000; vr++);

	DC_FlushAll();
	DC_InvalidateAll();
	swiSoftReset();
}

void gbaMode(int sel) {

	if (strncmp(GBA_HEADER.gamecode, "PASS", 4) == 0)resetToSlot2();

	videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
	videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
	vramSetBankC(VRAM_C_SUB_BG_0x06200000);
	vramSetBankD(VRAM_D_LCD);
	REG_BG3CNT = BG_BMP16_256x256 | BG_BMP_BASE(0) | BG_WRAP_OFF;
	REG_BG3PA = 1 << 8;
	REG_BG3PB = 0;
	REG_BG3PC = 0;
	REG_BG3PD = 1 << 8;
	REG_BG3X = 0;
	REG_BG3Y = 0;
	toncset((void*)BG_BMP_RAM(0), 0, 0x18000);
	toncset((void*)BG_BMP_RAM(8), 0, 0x18000);
	swiWaitForVBlank();

	if (PersonalData->gbaScreen) { lcdMainOnBottom(); }
	else { lcdMainOnTop(); }

	gba_frame(sel);

	sysSetBusOwners(ARM7_OWNS_CARD, ARM7_OWNS_ROM);
	fifoSendValue32(FIFO_USER_01, 1);
	REG_IME = 0;
	irqDisable(IRQ_VBLANK);
	while (1)swiWaitForVBlank();
}

void err_cnf(int n1, int n2) {
	int	len;
	int	x1, x2;
	int	y1, y2;
	int	xi, yi;
	u16* gback;
	int	gsiz;
	int uiBoxColor = (GBAmode == 0) ? 5 : 3;
	len = strlen(errmsg[n1]);
	if (len < strlen(errmsg[n2]))len = strlen(errmsg[n2]);
	if (len < 10)	len = 10;

	x1 = (256 - len * 8) / 2 - 4;
	y1 = 4 * 12 - 6;
	x2 = x1 + len * 8 + 9;
	y2 = 8 * 12 + 3;

	gsiz = (x2 - x1 + 1) * (y2 - y1 + 1);
	gback = (u16*)malloc(sizeof(u16*) * gsiz);
	for (yi = y1; yi < y2 + 1; yi++) {
		for (xi = x1; xi < x2 + 1; xi++) {
			gback[(xi - x1) + (yi - y1) * (x2 + 1 - x1)] = Point_SUB(SubScreen, xi, yi);
		}
	}

	DrawBox_SUB(SubScreen, x1, y1, x2, y2, uiBoxColor, 0);
	DrawBox_SUB(SubScreen, x1 + 1, y1 + 1, x2 - 1, y2 - 1, 2, 1);
	DrawBox_SUB(SubScreen, x1 + 2, y1 + 2, x2 - 2, y2 - 2, uiBoxColor, 0);

	fontPrintSub(SubScreen, x1 + 6, y1 + 6, errmsg[n1], 1, 2);
	fontPrintSub(SubScreen, x1 + 6, y1 + 20, errmsg[n2], 1, 2);
	fontPrintSub(SubScreen, x1 + (len / 2) * 8 - 18, y1 + 37, errmsg[13], 1, 2);

	while (!(inp_key() & KEY_A));

	for (yi = y1; yi < y2 + 1; yi++) {
		for (xi = x1; xi < x2 + 1; xi++) {
			Pixel_SUB(SubScreen, xi, yi, gback[(xi - x1) + (yi - y1) * (x2 + 1 - x1)]);
		}
	}
	free(gback);
}

int cnf_inp(int n1, int n2) {
	int	len;
	int	x1, x2;
	int	y1, y2;
	int	xi, yi;
	u16* gback;
	int	gsiz;
	u32	ky;
	int uiBoxColor = (GBAmode == 0) ? 5 : 3;
	len = strlen(cnfmsg[n1]);
	if (len < strlen(cnfmsg[n2]))len = strlen(cnfmsg[n2]);
	if (len < 20)	len = 20;

	x1 = (256 - len * 8) / 2 - 4;
	y1 = 4 * 12 - 6;
	x2 = x1 + len * 8 + 9;
	y2 = 8 * 12 + 3;

	gsiz = (x2 - x1 + 1) * (y2 - y1 + 1);
	gback = (u16*)malloc(sizeof(u16*) * gsiz);
	for (yi = y1; yi < y2 + 1; yi++) {
		for (xi = x1; xi < x2 + 1; xi++) {
			gback[(xi - x1) + (yi - y1) * (x2 + 1 - x1)] = Point_SUB(SubScreen, xi, yi);
		}
	}

	DrawBox_SUB(SubScreen, x1, y1, x2, y2, uiBoxColor, 0);
	DrawBox_SUB(SubScreen, x1 + 1, y1 + 1, x2 - 1, y2 - 1, 0, 1);
	DrawBox_SUB(SubScreen, x1 + 2, y1 + 2, x2 - 2, y2 - 2, uiBoxColor, 0);

	fontPrintSub(SubScreen, x1 + 6, y1 + 6, cnfmsg[n1], 1, 0);
	fontPrintSub(SubScreen, x1 + 6, y1 + 20, cnfmsg[n2], 1, 0);
	fontPrintSub(SubScreen, x1 + (len / 2) * 8 - 50, y1 + 37, cnfmsg[0], 1, 0);

	ky = inp_key();

	for (yi = y1; yi < y2 + 1; yi++) {
		for (xi = x1; xi < x2 + 1; xi++) {
			Pixel_SUB(SubScreen, xi, yi, gback[(xi - x1) + (yi - y1) * (x2 + 1 - x1)]);
		}
	}
	free(gback);
	return(ky);
}

int cnf_inp2(int n1, int n2) {
	int	len;
	int	x1, x2;
	int	y1, y2;
	int	xi, yi;
	u16* gback;
	int	gsiz;
	u32	ky;
	int uiBoxColor = (GBAmode == 0) ? 5 : 3;
	len = strlen(cnfmsg2[n1]);
	if (len < strlen(cnfmsg2[n2]))len = strlen(cnfmsg2[n2]);
	if (len < 20)	len = 20;

	x1 = (256 - len * 8) / 2 - 4;
	y1 = 4 * 12 - 6;
	x2 = x1 + len * 8 + 9;
	y2 = 8 * 12 + 3;

	gsiz = (x2 - x1 + 1) * (y2 - y1 + 1);
	gback = (u16*)malloc(sizeof(u16*) * gsiz);
	for (yi = y1; yi < y2 + 1; yi++) {
		for (xi = x1; xi < x2 + 1; xi++) {
			gback[(xi - x1) + (yi - y1) * (x2 + 1 - x1)] = Point_SUB(SubScreen, xi, yi);
		}
	}

	DrawBox_SUB(SubScreen, x1, y1, x2, y2, uiBoxColor, 0);
	DrawBox_SUB(SubScreen, x1 + 1, y1 + 1, x2 - 1, y2 - 1, 0, 1);
	DrawBox_SUB(SubScreen, x1 + 2, y1 + 2, x2 - 2, y2 - 2, uiBoxColor, 0);

	fontPrintSub(SubScreen, x1 + 6, y1 + 6, cnfmsg2[n1], 1, 0);
	fontPrintSub(SubScreen, x1 + 6, y1 + 20, cnfmsg2[n2], 1, 0);
	fontPrintSub(SubScreen, x1 + (len / 2) * 8 - 50, y1 + 37, cnfmsg2[0], 1, 0);

	ky = inp_key();

	for (yi = y1; yi < y2 + 1; yi++) {
		for (xi = x1; xi < x2 + 1; xi++) {
			Pixel_SUB(SubScreen, xi, yi, gback[(xi - x1) + (yi - y1) * (x2 + 1 - x1)]);
		}
	}
	free(gback);
	return(ky);
}

void dsp_bar(int mod, int per) {
	int x1, x2;
	int y1, y2;
	int xi, yi;
	int gsiz;
	u16 progressFillColor = (GBAmode == 0) ? RGB15(0, 8, 31) : RGB15(5, 20, 0);
	u16 borderColor = (GBAmode == 0) ? RGB15(0, 8, 31) : RGB15(5, 20, 0);

	x1 = 49;
	y1 = 142;
	x2 = 205;
	y2 = 187;

	if (per < 0) {
		gsiz = (x2 - x1 + 1) * (y2 - y1 + 1);
		gbar = (u16*)malloc(sizeof(u16*) * gsiz);
		for (yi = y1; yi < y2 + 1; yi++)
			for (xi = x1; xi < x2 + 1; xi++)
				gbar[(xi - x1) + (yi - y1) * (x2 + 1 - x1)] = Point(MainScreen, xi, yi);

		DrawBox(MainScreen, x1, y1, x2, y2, borderColor, 0);
		DrawBox(MainScreen, x1 + 1, y1 + 1, x2 - 1, y2 - 1, RGB15(6, 6, 6), 1);
		DrawBox(MainScreen, x1 + 2, y1 + 2, x2 - 2, y2 - 2, borderColor, 0);

		if (per != -2) DrawBox(MainScreen, x1 + 28, y1 + 20, x1 + 129, y1 + 40, RGB15(6, 6, 6), 0);
		fontPrint(MainScreen, x1 + 26, y1 + 6, barmsg[mod], RGB15(30, 30, 30), RGB15(6, 6, 6));
		oldper = -1;
		return;
	}

	if (gbar == NULL) return;

	if (per != oldper) {
		oldper = per;
		if (per > 0)
			DrawBox(MainScreen, x1 + 29, y1 + 21, x1 + 28 + per, y1 + 39, progressFillColor, 1);
		if (per < 100)
			DrawBox(MainScreen, x1 + 28 + per + 1, y1 + 21, x1 + 128, y1 + 39, RGB15(30, 30, 30), 1);

		sprintf(tbuf, "%2d%%", per);
		int percentW = strlen(tbuf) * 6;
		int innerLeft = x1 + 28;
		int innerRight = x1 + 129;
		int percentX = innerLeft + ((innerRight - innerLeft) - percentW) / 2;
		ShinoPrint(MainScreen, percentX, y1 + 24, (u8*)tbuf, RGB15(3, 3, 3), 0, 0);
	}

	if (mod == -1) {
		for (yi = y1; yi < y2 + 1; yi++)
			for (xi = x1; xi < x2 + 1; xi++)
				Pixel(MainScreen, xi, yi, gbar[(xi - x1) + (yi - y1) * (x2 + 1 - x1)]);
		free(gbar);
		gbar = NULL;
	}
}

void RamClear() {
	u32* a8;
	int	i;

	a8 = (u32*)0x8000000;
	for (i = 0; i < 0x100; i++) {
		a8[i] = 0xFFFFFFFF;
	}

	*(vu32*)0x80000B4 = 0x24242400;
	*(vu32*)0x80000BC = 0x7FFFFFFF;
	*(vu32*)0x801FFFC = 0x7FFFFFFF;
	*(vu32*)0x8240000 = 0x00000000;
}

void _dsp_clear() { DrawBox_SUB(SubScreen, 0, 28, 255, 114, 0, 1); }

int rumble_cmd() {
	int	cmd = 0;
	u32	ky, repky;
	int	i;
	int	len;
	int	x1, x2;
	int	y1, y2;
	int uiBoxColor = (GBAmode == 0) ? 5 : 3;
	len = strlen(cmd_m[0]);

	x1 = (256 - len * 8) / 2 - 4;
	y1 = 4 * 12 - 6;
	x2 = x1 + len * 8 + 5;
	y2 = 8 * 12 + 2;

	ColorSwap_SUB(SubScreen, 0, 0, 255, 192, 3, 5);
	_dsp_clear();
	DrawBox_SUB(SubScreen, 9, 137, 246, 187, 0, 1);

	DrawBox_SUB(SubScreen, 75, 115, 181, 136, 1, 0);
	DrawBox_SUB(SubScreen, 76, 116, 180, 135, uiBoxColor, 1);
	DrawBox_SUB(SubScreen, 77, 117, 179, 134, 0, 0);

	fontPrintSub(SubScreen, 15 * 6, 10 * 12, t_msg[17], 1, uiBoxColor);
	fontPrintSub(SubScreen, 2 * 6, 12 * 12 + 6, t_msg[18], 1, 0);
	fontPrintSub(SubScreen, 2 * 6, 14 * 12 + 6, t_msg[19], 1, 0);

	DrawBox_SUB(SubScreen, x1, y1, x2, y2, 5, 1);
	DrawBox_SUB(SubScreen, x1 + 1, y1 + 1, x2 - 1, y2 - 1, 0, 1);
	DrawBox_SUB(SubScreen, x1 + 2, y1 + 2, x2 - 2, y2 - 2, 5, 0);

	fontPrintSub(SubScreen, x1 + 3, y1 + 3, cmd_m[0], 2, 3);
	for (i = 1; i < 4; i++) {
		fontPrintSub(SubScreen, x1 + 3, y1 + 3 + i * 13, cmd_m[i], 1, 0);
	}

	while (1) {
		swiWaitForVBlank();
		scanKeys();
		repky = keysDownRepeat();
		if ((repky & KEY_UP) || (repky & KEY_DOWN)) {
			if (repky & KEY_UP) {
				if (cmd > 0) {
					fontPrintSub(SubScreen, x1 + 3, y1 + 3 + cmd * 13, cmd_m[cmd], 1, 0);
					cmd--;
					fontPrintSub(SubScreen, x1 + 3, y1 + 3 + cmd * 13, cmd_m[cmd], 2, 3);
				}
			}
			if (repky & KEY_DOWN) {
				if (cmd < 3) {
					fontPrintSub(SubScreen, x1 + 3, y1 + 3 + cmd * 13, cmd_m[cmd], 1, 0);
					cmd++;
					fontPrintSub(SubScreen, x1 + 3, y1 + 3 + cmd * 13, cmd_m[cmd], 2, 3);
				}
			}
			continue;
		}

		ky = keysDown();

		if (ky & KEY_A)	break;
		if (ky & KEY_L) {
			GBAmode = 1;
			if (isOmega)GBAmode = 0;
			setGBAmode(-1);
			if (GBAmode == 0) BG_PALETTE_SUB[5] = RGB15(0, 8, 31);
			else BG_PALETTE_SUB[5] = RGB15(5, 20, 0);
			cmd = -1;
			break;
		}
		if (ky & KEY_START) {
			if (softReset) {
				cmd = 99;
				SetRompage(0);
				SetRampage(16);
				break;
			}
		}
	}

	if (cmd != -1)	return(cmd);
	return(-1);
}

void _gba_dsp(int no, int mod, int x, int y) {
	int sn = sortfile[no];
	char nameBuf[256];
	u16 highlightTextColor = (GBAmode == 0) ? RGB15(0, 8, 31) : RGB15(5, 20, 0);

	strncpy(nameBuf, fs[sn].filename, sizeof(nameBuf) - 1);
	nameBuf[sizeof(nameBuf) - 1] = '\0';

	if (mod == 1) {
		if (fs[sn].type & S_IFDIR) {
			snprintf(tbuf, sizeof(tbuf), " %-35s <DIR>", nameBuf);
			g_scroll_active = 0;
		}
		else {
			snprintf(tbuf, sizeof(tbuf), " %-31s", nameBuf);
			if (!g_scroll_redraw) {
				int max_pixels = 256 - x * 6;
				int total_pixels = 0;
				const char* p = tbuf;
				while (*p) {
					int clen = 1;
					if ((*p & 0x80) == 0) clen = 1;
					else if ((*p & 0xE0) == 0xC0) clen = 2;
					else if ((*p & 0xF0) == 0xE0) clen = 3;
					else if ((*p & 0xF8) == 0xF0) clen = 4;
					total_pixels += 8;
					for (int i = 0; i < clen && *p; i++) p++;
				}
				if (total_pixels > max_pixels) {
					g_scroll_active = 1;
					g_scroll_max = total_pixels - max_pixels;
				}
				else {
					g_scroll_active = 0;
					g_scroll_max = 0;
				}
			}
		}

		int max_pixels = 256 - x * 6;
		if (g_scroll_active) {
			const char* draw_start = tbuf;
			int skip_pixels = g_scroll_offset;
			while (skip_pixels >= 8 && *draw_start) {
				int clen = 1;
				if ((*draw_start & 0x80) == 0) clen = 1;
				else if ((*draw_start & 0xE0) == 0xC0) clen = 2;
				else if ((*draw_start & 0xF0) == 0xE0) clen = 3;
				else if ((*draw_start & 0xF8) == 0xF0) clen = 4;
				for (int i = 0; i < clen && *draw_start; i++) draw_start++;
				skip_pixels -= 8;
			}
			const char* end = draw_start;
			int cur_w = 0;
			while (*end) {
				int clen = 1;
				if ((*end & 0x80) == 0) clen = 1;
				else if ((*end & 0xE0) == 0xC0) clen = 2;
				else if ((*end & 0xF0) == 0xE0) clen = 3;
				else if ((*end & 0xF8) == 0xF0) clen = 4;
				if (cur_w + 8 > max_pixels) break;
				cur_w += 8;
				for (int i = 0; i < clen && *end; i++) end++;
			}
			int len = end - draw_start;
			strncpy(tbuf, draw_start, len);
			tbuf[len] = '\0';
		}
		else {
			char* p = tbuf, * last = tbuf;
			int cur_w = 0;
			while (*p) {
				int clen = 1;
				if ((*p & 0x80) == 0) clen = 1;
				else if ((*p & 0xE0) == 0xC0) clen = 2;
				else if ((*p & 0xF0) == 0xE0) clen = 3;
				else if ((*p & 0xF8) == 0xF0) clen = 4;
				if (cur_w + 8 > max_pixels) break;
				cur_w += 8;
				for (int i = 0; i < clen && *p; i++) p++;
				last = p;
			}
			*last = '\0';
		}

		DrawBox(MainScreen, x * 6, y * 12 - 2, 255, y * 12 + 9, RGB15(3, 3, 3), 1);
		fontPrint(MainScreen, x * 6, y * 12, tbuf, highlightTextColor, RGB15(3, 3, 3));

		if (g_scroll_redraw) return;

		if (GBAmode == 0) {
			DrawBox_SUB(SubScreen, 6, 32, 249, 76, 5, 0);
			DrawBox_SUB(SubScreen, 8, 34, 247, 74, 5, 0);
		}
		else {
			DrawBox_SUB(SubScreen, 6, 32, 249, 76, 3, 0);
			DrawBox_SUB(SubScreen, 8, 34, 247, 74, 3, 0);
		}
		DrawBox_SUB(SubScreen, 9, 4 * 12, 246, 6 * 12 - 1, 0, 1);
		fontPrintSub(SubScreen, 2 * 6, 3 * 12, t_msg[0], 1, 0);

		if (!(fs[sn].type & S_IFDIR)) {
			int subMaxPixels = 246 - 3 * 6;
			char* p = nameBuf, * last = nameBuf;
			int cur_w = 0;
			while (*p) {
				int clen = 1;
				if ((*p & 0x80) == 0) clen = 1;
				else if ((*p & 0xE0) == 0xC0) clen = 2;
				else if ((*p & 0xF0) == 0xE0) clen = 3;
				else if ((*p & 0xF8) == 0xF0) clen = 4;
				if (cur_w + 8 > subMaxPixels) break;
				cur_w += 8;
				for (int i = 0; i < clen && *p; i++) p++;
				last = p;
			}
			*last = '\0';
			DrawBox_SUB(SubScreen, 3 * 6, 4 * 12, 246, 4 * 12 + 11, 0, 1);
			fontPrintSub(SubScreen, 3 * 6, 4 * 12, nameBuf, 1, 0);

			snprintf(tbuf, sizeof(tbuf), "容量:%dKB (%s %s)",
				(int)fs[sn].filesize / 1024, fs[sn].gametitle, fs[sn].gamecode);
			p = tbuf; last = tbuf; cur_w = 0;
			int subMaxPixelsCap = 246 - 4 * 6 - 1;
			while (*p) {
				int clen = 1;
				if ((*p & 0x80) == 0) clen = 1;
				else if ((*p & 0xE0) == 0xC0) clen = 2;
				else if ((*p & 0xF0) == 0xE0) clen = 3;
				else if ((*p & 0xF8) == 0xF0) clen = 4;
				if (cur_w + 8 > subMaxPixelsCap) break;
				cur_w += 8;
				for (int i = 0; i < clen && *p; i++) p++;
				last = p;
			}
			*last = '\0';
			DrawBox_SUB(SubScreen, 4 * 6, 5 * 12, 246, 5 * 12 + 11, 0, 1);
			fontPrintSub(SubScreen, 4 * 6, 5 * 12, tbuf, 1, 0);
		}
	}
	else {
		if (fs[sn].type & S_IFDIR) {
			snprintf(tbuf, sizeof(tbuf), " %-35s <DIR>", nameBuf);
		}
		else {
			snprintf(tbuf, sizeof(tbuf), " %-31s", nameBuf);
		}
		int max_pixels = 256 - x * 6;
		char* p = tbuf, * last = tbuf;
		int cur_w = 0;
		while (*p) {
			int clen = 1;
			if ((*p & 0x80) == 0) clen = 1;
			else if ((*p & 0xE0) == 0xC0) clen = 2;
			else if ((*p & 0xF0) == 0xE0) clen = 3;
			else if ((*p & 0xF8) == 0xF0) clen = 4;
			if (cur_w + 8 > max_pixels) break;
			cur_w += 8;
			for (int i = 0; i < clen && *p; i++) p++;
			last = p;
		}
		*last = '\0';
		DrawBox(MainScreen, x * 6, y * 12 - 2, 255, y * 12 + 11, RGB15(6, 6, 6), 1);
		fontPrint(MainScreen, x * 6, y * 12, tbuf, RGB15(30, 30, 30), RGB15(6, 6, 6));
	}
}

void _gba_sel_dsp(int no, int yc, int mod) {
	int	x, y;
	int	st, i;
	int	len;
	y = 1;
	x = 0;
	int uiBoxColor = (GBAmode == 0) ? 5 : 3;
	if (mod == 0) {
		_dsp_clear();

		DrawBox_SUB(SubScreen, 75, 115, 181, 136, 1, 0);
		DrawBox_SUB(SubScreen, 76, 116, 180, 135, uiBoxColor, 1);
		DrawBox_SUB(SubScreen, 77, 117, 179, 134, 0, 0);

		if (GBAmode == 0) {
			if (carttype < 4) {
				fontPrintSub(SubScreen, 100 - 15, 10 * 12, t_msg[1], 1, uiBoxColor);
			}
			else {
				fontPrintSub(SubScreen, 100 - 15, 10 * 12, t_msg[21], 1, uiBoxColor);
			}

			DrawBox_SUB(SubScreen, 2 * 6, 11 * 12 + 6, 246, 11 * 12 + 17, 0, 1);
			fontPrintSub(SubScreen, 2 * 6, 11 * 12 + 8, t_msg[2], 1, 0);
			DrawBox_SUB(SubScreen, 2 * 6, 12 * 12 + 6, 246, 12 * 12 + 17, 0, 1);
			fontPrintSub(SubScreen, 2 * 6, 12 * 12 + 8, t_msg[3], 1, 0);
			DrawBox_SUB(SubScreen, 2 * 6, 13 * 12 + 6, 246, 13 * 12 + 17, 0, 1);
			fontPrintSub(SubScreen, 2 * 6, 13 * 12 + 8, t_msg[4], 1, 0);
			if (carttype < 3) {
				DrawBox_SUB(SubScreen, 2 * 6, 14 * 12 + 6, 246, 14 * 12 + 17, 0, 1);
				fontPrintSub(SubScreen, 2 * 6, 14 * 12 + 8, t_msg[5], 1, 0);
			}
			else {
				if (softReset) {
					DrawBox_SUB(SubScreen, 2 * 6, 14 * 12 + 6, 246, 14 * 12 + 17, 0, 1);
					fontPrintSub(SubScreen, 2 * 6, 14 * 12 + 8, t_msg[20], 1, 0);
				}
				else {
					DrawBox_SUB(SubScreen, 2 * 6, 14 * 12 + 6, 246, 14 * 12 + 17, 0, 1);
					fontPrintSub(SubScreen, 2 * 6, 14 * 12 + 6, "                          ", 1, 0);
				}
			}
		}
		else {
			if (softReset) {
				DrawBox_SUB(SubScreen, 2 * 6, 14 * 12 + 6, 246, 14 * 12 + 17, 0, 1);
				fontPrintSub(SubScreen, 2 * 6, 14 * 12 + 8, t_msg[6], 1, 0);
			}
			else {
				DrawBox_SUB(SubScreen, 2 * 6, 14 * 12 + 6, 246, 14 * 12 + 17, 0, 1);
				fontPrintSub(SubScreen, 2 * 6, 14 * 12 + 8, t_msg[7], 1, 0);
			}
			fontPrintSub(SubScreen, 108 - 15, 10 * 12, t_msg[8], 1, uiBoxColor);
			DrawBox_SUB(SubScreen, 2 * 6, 11 * 12 + 6, 246, 11 * 12 + 17, 0, 1);
			fontPrintSub(SubScreen, 2 * 6, 11 * 12 + 8, t_msg[9], 1, 0);
			DrawBox_SUB(SubScreen, 2 * 6, 12 * 12 + 6, 246, 12 * 12 + 17, 0, 1);
			fontPrintSub(SubScreen, 2 * 6, 12 * 12 + 8, t_msg[10], 1, 0);
			DrawBox_SUB(SubScreen, 2 * 6, 13 * 12 + 6, 246, 13 * 12 + 17, 0, 1);
			fontPrintSub(SubScreen, 2 * 6, 13 * 12 + 8, t_msg[11], 1, 0);
		}

		ClearBG(MainScreen, RGB15(6, 6, 6));
		DrawBox(MainScreen, 0, 0, 255, 11, RGB15(0, 0, 0), 1);
		sprintf(tbuf, t_msg[12], curpath, numGames);
		len = strlen(tbuf);
		if (len > 40)	len -= 40;
		else		len = 0;
		fontPrint(MainScreen, 0, 0, tbuf + len, RGB15(30, 30, 30), RGB15(0, 0, 0));

		{
			char rightStr[] = "中文版:shooterspps";
			u16 rightStrColor = (GBAmode == 0) ? RGB15(0, 8, 31) : RGB15(5, 20, 0);
			int charCount = 0;
			for (char* p = rightStr; *p; p++) {
				if ((*p & 0xC0) != 0x80) charCount++;
			}
			int rightX = 256 - charCount * 8;
			fontPrint(MainScreen, rightX, 0, rightStr, rightStrColor, RGB15(0, 0, 0));
		}

		DrawBox_SUB(SubScreen, 6, 80, 249, 111, uiBoxColor, 0);
		DrawBox_SUB(SubScreen, 8, 82, 247, 109, uiBoxColor, 0);

		checkSRAM(filename);
		len = strlen(filename);
		if (len == 0) {
			sprintf(filename, t_msg[13]);
			len = 20;
		}

		sprintf(tbuf, "< %s >", filename);
		int charCount = 0;
		for (char* p = tbuf; *p; p++) {
			if ((*p & 0xC0) != 0x80) charCount++;
		}
		int maxPixelW = 246 - 6 - 4;
		int maxChars = maxPixelW / 8;
		if (charCount > maxChars) {
			int cnt = 0;
			char* p = tbuf;
			while (*p) {
				if ((*p & 0xC0) != 0x80) {
					if (cnt == maxChars) {
						*p = '\0';
						break;
					}
					cnt++;
				}
				p++;
			}
			charCount = maxChars;
		}
		int pixelW = charCount * 8;
		int startX = (256 - pixelW) / 2;
		if (startX < 6) startX = 6;
		int drawX = startX + 2;
		int clearW = pixelW - 4;
		if (clearW < 0) clearW = 0;
		if (drawX + clearW > 246) {
			drawX = 246 - clearW;
			if (drawX < 6) drawX = 6;
		}

		fontPrintSub(SubScreen, 2 * 6, 7 * 12, t_msg[14], 1, 0);
		DrawBox_SUB(SubScreen, drawX, 8 * 12, drawX + clearW - 1, 8 * 12 + 11, 0, 1);
		fontPrintSub(SubScreen, drawX, 8 * 12, tbuf, 1, 0);
	}

	st = no - yc;
	for (i = 0; i < 15; i++) {
		if (i + st < numFiles) {
			if (i == yc) { _gba_dsp(i + st, 1, x, y + i); }
			else { _gba_dsp(i + st, 0, x, y + i); }
		}
	}
}

int gba_sel() {
	int	cmd = -1;
	int	sel;
	u32	ky, repky;
	int	yc;
	int	x, y;
	int	cn;
	int	ret = 0;

	y = 1;
	x = 0;
	sel = 0;
	yc = 0;

	int	ii;

	cn = 1;
	if (softReset)	cn++;

	_gba_sel_dsp(sel, yc, 0);

	while (1) {
		swiWaitForVBlank();
		scanKeys();

		if (sel != g_last_sel) {
			g_scroll_offset = 0;
			g_scroll_fraction = 0;
			g_scroll_pause_timer = 0;
			g_last_sel = sel;
		}
		if (g_scroll_active && g_scroll_max > 0) {
			if (g_scroll_pause_timer > 0) {
				g_scroll_pause_timer--;
				if (g_scroll_pause_timer == 0) {
					g_scroll_offset = 0;
					g_scroll_active = 0;
				}
			}
			else {
				g_scroll_fraction += 128;
				while (g_scroll_fraction >= 256) {
					g_scroll_fraction -= 256;
					g_scroll_offset++;
					if (g_scroll_offset > g_scroll_max) {
						g_scroll_offset = g_scroll_max;
						g_scroll_pause_timer = 60;
						break;
					}
				}
			}
			g_scroll_redraw = 1;
			_gba_dsp(sel, 1, x, y + yc);
			g_scroll_redraw = 0;
		}

		repky = keysDownRepeat();

		if ((repky & KEY_UP) || (repky & KEY_DOWN)) {
			if (repky & KEY_UP) {
				if (sel > 0) {
					if (yc == 0) {
						sel--;
						_gba_sel_dsp(sel, yc, 1);
					}
					else {
						_gba_dsp(sel, 0, x, y + yc);
						yc--;
						sel--;
						_gba_dsp(sel, 1, x, y + yc);
					}
				}
			}
			if (repky & KEY_DOWN) {
				if (sel < numFiles - 1) {
					if (yc == 14) {
						sel++;
						_gba_sel_dsp(sel, yc, 1);
					}
					else {
						_gba_dsp(sel, 0, x, y + yc);
						yc++;
						sel++;
						_gba_dsp(sel, 1, x, y + yc);
					}
				}
			}
			continue;
		}

		if (repky & KEY_LEFT) {
			if (sel > 0) {
				int old_sel = sel;
				int new_sel = sel - 5;
				if (new_sel < 0) new_sel = 0;
				int st = sel - yc;
				yc = new_sel - st;
				if (yc < 0) {
					st += yc;
					if (st < 0) st = 0;
					yc = 0;
				}
				sel = st + yc;
				if (sel != old_sel) {
					_gba_sel_dsp(sel, yc, 1);
				}
			}
			continue;
		}
		if (repky & KEY_RIGHT) {
			if (sel < numFiles - 1) {
				int old_sel = sel;
				int new_sel = sel + 5;
				if (new_sel >= numFiles) new_sel = numFiles - 1;
				int st = sel - yc;
				yc = new_sel - st;
				if (yc > 14) {
					st += (yc - 14);
					if (st > numFiles - 15) st = numFiles - 15;
					if (st < 0) st = 0;
					yc = 14;
				}
				sel = st + yc;
				if (sel != old_sel) {
					_gba_sel_dsp(sel, yc, 1);
				}
			}
			continue;
		}

		ky = keysDown();

		if (ky & KEY_L && !isSuperCard) {
			if (GBAmode > 0) {
				GBAmode--;
				if ((GBAmode == 1) && isOmega)GBAmode--;
				setGBAmode(-1);
				if (GBAmode == 0) BG_PALETTE_SUB[5] = RGB15(0, 8, 31);
				else BG_PALETTE_SUB[5] = RGB15(5, 20, 0);
				_gba_sel_dsp(sel, yc, 0);
			}
		}
		if (ky & KEY_R && !isSuperCard) {
			if (softReset && (carttype > 2)) {
				cmd = 3;
				break;
			}
			else if (GBAmode < cn && carttype <= 2) {
				GBAmode++;
				if ((GBAmode == 1) && isOmega)GBAmode++;
				setGBAmode(-1);
				if (GBAmode == 0) BG_PALETTE_SUB[5] = RGB15(0, 8, 31);
				else BG_PALETTE_SUB[5] = RGB15(5, 20, 0);
				if (GBAmode == 2) {
					_gba_dsp(sel, 0, x, y + yc);
					cmd = -1;
					break;
				}
				else {
					_gba_sel_dsp(sel, yc, 0);
				}
			}
		}

		if (ky & KEY_START) {
			if (softReset && !isOmega) {
				cmd = 99;
				if (carttype == 1) {
					SetRompage(0);
					SetRampage(16);
				}
				break;
			}
			else if (isOmega && (GBAmode == 0)) {
				SetRompage(0x8002);
				gbaMode(-1);
			}
		}
		if (ky & KEY_SELECT) {
			if (softReset && (GBAmode == 0)) {
				if (!(fs[sortfile[sel]].type & S_IFDIR) && (fs[sortfile[sel]].isNDSFile != 1))ret = writeFileToRam(sortfile[sel]);
				if (ret != 0) {
					_gba_sel_dsp(sel, yc, 0);
					err_cnf(7, 8);
				}
				else {
					turn_off(softReset);
				}
			}
		}

		if (ky & KEY_X) {
			if (GBAmode == 1) {
				SetRompage(0);
				SetRampage(16);
				gbaMode(-1);
			}
			else {
				if (cnf_inp(7, 8) & KEY_A)SRAMdump(0);
			}
		}

		if (ky & KEY_Y) {
			if (GBAmode == 1) {
				if (checkSRAM(filename)) {
					if (save_sel(0, filename) >= 0) {
						dsp_bar(5, -1);
						swiWaitForVBlank();
						dsp_bar(5, 50);
						writeSramFromFile(filename);
						dsp_bar(5, 100);
						for (int I = 0; I < 50; I++)swiWaitForVBlank();
						dsp_bar(-1, 100);
					}
					_gba_sel_dsp(sel, yc, 0);
				}
				else {
					err_cnf(4, 5);
				}
			}
			else {
				if (cnf_inp(5, 6) & KEY_A) {
					SRAMdump(1);
					_gba_sel_dsp(sel, yc, 0);
				}
			}
		}

		if (ky & KEY_A) {
			if (fs[sortfile[sel]].type & S_IFDIR) {
				if (!strcmp(fs[sortfile[sel]].filename, "..")) {
					for (ii = strlen(curpath) - 2; ii >= 0; ii--) {
						if (curpath[ii] == '/') {
							curpath[ii + 1] = 0;
							break;
						}
					}
				}
				else {
					strcat(curpath, fs[sortfile[sel]].filename);
					strcat(curpath, "/");
				}
				FileListGBA();
				setcurpath();
				cmd = -1;
				break;
			}
			if (fs[sortfile[sel]].isNDSFile == 1) {
				runNDSFile(tbuf, ini.save_dir, curpath, fs[sortfile[sel]].filename, fs[sortfile[sel]].ndssavfilename, (fs[sortfile[sel]].isHomebrewNDS == 1));
				cmd = -1;
				break;
			}
			if (GBAmode == 0) { ret = writeFileToRam(sortfile[sel]); }
			else { ret = writeFileToNor(sortfile[sel]); }
			if (ret != 0) {
				if (ret == 2) {
					err_cnf(9, 10);
				}
				else {
					if (GBAmode == 0 && carttype < 3) { err_cnf(7, 8); }
					else { err_cnf(7, 6); }
				}
			}
			else {
				if (GBAmode == 0) {
					gbaMode(sortfile[sel]);
				}
			}
			_gba_sel_dsp(sel, yc, 0);
		}
		if (ky & KEY_B) {
			if (checkSRAM(filename)) {
				if (save_sel(1, filename) >= 0) {
					dsp_bar(4, -1);
					swiWaitForVBlank();
					dsp_bar(4, 50);
					writeSramToFile(filename);
					dsp_bar(4, 100);
					for (int I = 0; I < 50; I++)swiWaitForVBlank();
					dsp_bar(-1, 100);
				}
				_gba_sel_dsp(sel, yc, 0);
			}
			else {
				err_cnf(4, 5);
			}
		}
	}
	return(cmd);
}

void mainloop(void) {
	int	cmd;
	keysSetRepeat(20, 6);

	setLangMsg();
	if (isDSiMode()) { err_cnf(14, 15); turn_off(0); }
	if (!fatInitDefault()) { err_cnf(0, 1); turn_off(0); }
	initFontMem(misaki_gothic_8x8_bin, misaki_gothic_8x8_bin_end - misaki_gothic_8x8_bin);
	if (!isFontLoaded()) {
		ShinoPrint_SUB(SubScreen, 2 * 6, 9 * 12, (u8*)"Font error", 1, 0, 0);
	}

	DrawBox_SUB(SubScreen, 20, 3, 235, 27, 1, 0);
	DrawBox_SUB(SubScreen, 21, 4, 234, 26, 5, 1);
	DrawBox_SUB(SubScreen, 22, 5, 233, 25, 0, 0);

	ShinoPrint_SUB(SubScreen, 9 * 6 - 10, 1 * 12 - 2, (u8*)"GBA ExpLoader", 1, 0, 0);
	ShinoPrint_SUB(SubScreen, 34 * 6 - 12, 12, (u8*)VERSTRING, 1, 0, 0);

	DrawBox_SUB(SubScreen, 6, 125, 249, 190, 5, 0);
	DrawBox_SUB(SubScreen, 8, 127, 247, 188, 5, 0);

	checkFlashID();
	if (isOmega && (cnf_inp2(1, 2) & KEY_A)) isOmegaDE = true;

	switch (carttype) {
	default:
		err_cnf(2, 3);
		turn_off(softReset);
		break;
	case 0:
		err_cnf(2, 3);
		turn_off(softReset);
		break;
	case 1:
		if (is3in1Plus) {
			ShinoPrint_SUB(SubScreen, 23 * 6 - 10, 1 * 12 - 2, (u8*)"[3in1Pls]", 1, 0, 0);
		}
		else if (isOmega && !isOmegaDE) {
			ShinoPrint_SUB(SubScreen, 23 * 6 - 10, 1 * 12 - 2, (u8*)"[ Omega ]", 1, 0, 0);
		}
		else if (isOmegaDE) {
			ShinoPrint_SUB(SubScreen, 23 * 6 - 10, 1 * 12 - 2, (u8*)"[  DE  ]", 1, 0, 0);
		}
		else {
			ShinoPrint_SUB(SubScreen, 23 * 6 - 10, 1 * 12 - 2, (u8*)" [ 3in1 ]", 1, 0, 0);
		}
		break;
	case 2:
		ShinoPrint_SUB(SubScreen, 23 * 6 - 10, 1 * 12 - 2, (u8*)"[New3in1]", 1, 0, 0); break;
	case 3:
		SetRompage(0x300);
		ShinoPrint_SUB(SubScreen, 23 * 6 - 10, 1 * 12 - 2, (u8*)"  [ EZ4 ]", 1, 0, 0);
		break;
	case 4: ShinoPrint_SUB(SubScreen, 23 * 6 - 10, 1 * 12 - 2, (u8*)"[EXP256K]", 1, 0, 0); break;
	case 5: ShinoPrint_SUB(SubScreen, 23 * 6 - 10, 1 * 12 - 2, (u8*)"[EXP128K]", 1, 0, 0); break;
	case 6:
		if (isSuperCard) {
			ShinoPrint_SUB(SubScreen, 23 * 6 - 10, 1 * 12 - 2, (u8*)"[ SC ]", 1, 0, 0);
		}
		else {
			ShinoPrint_SUB(SubScreen, 23 * 6 - 10, 1 * 12 - 2, (u8*)"[ M3/G6 ]", 1, 0, 0);
		}
		break;
	}
	fontPrintSub(SubScreen, 9 * 6 + 30, 5 * 12, t_msg[16], 1, 0);

	if (isSuperCard) {
		softReset = false;
	}
	else {
		softReset = ret_menu_chk();
	}

	*(vu8*)0x027FFC35 = 0x01;

	rwbuf = (u8*)malloc(0x100000 + 1024);

	GBA_ini();

	if (!checkSRAM_cnf() && (carttype != 5) && (cnf_inp(9, 10) & KEY_B))turn_off(softReset);

	getcurpath();
	FileListGBA();

	_dsp_clear();

	GBAmode = 0;
	if (checkSRAM(filename) && checkBackup()) {
		dsp_bar(4, -1);
		dsp_bar(4, 0);
		for (int I = 0; I < 30; I++)swiWaitForVBlank();
		if (save_sel(1, filename) >= 0) {
			writeSramToFile(filename);
			dsp_bar(4, 50);
			for (int I = 0; I < 30; I++)swiWaitForVBlank();
			dsp_bar(4, 100);
			for (int I = 0; I < 30; I++)swiWaitForVBlank();
			dsp_bar(-1, 100);
		}
		else {
			dsp_bar(-1, 100);
		}
	}

	getGBAmode();
	if ((GBAmode == 2) && !softReset)GBAmode = 0;
	if (carttype > 2)GBAmode = 0;

	if (GBAmode != 0) {
		BG_PALETTE_SUB[5] = RGB15(5, 20, 0);
	}
	else {
		BG_PALETTE_SUB[5] = RGB15(0, 8, 31);
	}

	cmd = -1;
	while (cmd == -1) {
		if (GBAmode == 2) {
			cmd = rumble_cmd();
		}
		else {
			cmd = gba_sel();
		}
	}

	*(vu8*)0x027FFC35 = 0x00;
	switch (cmd) {
	case 0:
		if (!isSuperCard)SetShake(0xF0);
		break;
	case 1:
		if (!isSuperCard)SetShake(0xF1);
		break;
	case 2:
		if (!isSuperCard)SetShake(0xF2);
		break;
	case 3:
		if ((carttype != 4) && !isSuperCard && !isOmega) {
			if (isOmega) {
				SetRompage(0x8002);
			}
			else {
				if (is3in1Plus) { SetRompage(0x100); }
				else { SetRompage(0x300); }
				OpenNorWrite();
			}
		}
		if (!isSuperCard && !isOmega)RamClear();
		break;
	}

	turn_off(softReset);
}

int main(void) {
	extern u64* fake_heap_end;
	*fake_heap_end = 0;

	defaultExceptionHandler();

	int	i;

	vramSetPrimaryBanks(VRAM_A_LCD, VRAM_B_LCD, VRAM_C_SUB_BG, VRAM_D_MAIN_BG);
	powerOn(POWER_ALL);

	videoSetMode(MODE_FB0 | DISPLAY_BG2_ACTIVE);
	videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);
	REG_BG0CNT_SUB = BG_256_COLOR | BG_MAP_BASE(0) | BG_TILE_BASE(1);
	uint16* map1 = (uint16*)BG_MAP_RAM_SUB(0);
	for (i = 0;i < (256 * 192 / 8 / 8);i++)map1[i] = i;
	lcdMainOnTop();
	ClearBG(MainScreen, RGB15(6, 6, 6));

	BG_PALETTE_SUB[0] = RGB15(6, 6, 6);
	BG_PALETTE_SUB[1] = RGB15(30, 30, 30);
	BG_PALETTE_SUB[2] = RGB15(29, 0, 0);
	BG_PALETTE_SUB[3] = RGB15(5, 20, 0);
	BG_PALETTE_SUB[4] = RGB15(0, 31, 31);
	BG_PALETTE_SUB[5] = RGB15(0, 8, 31);
	BG_PALETTE_SUB[6] = RGB15(31, 31, 0);
	BG_PALETTE_SUB[7] = RGB15(3, 3, 3);
	BG_PALETTE_SUB[8] = RGB15(0, 8, 31);

	ClearBG_SUB(SubScreen, 0);

	swiWaitForVBlank();

	sysSetBusOwners(BUS_OWNER_ARM9, BUS_OWNER_ARM9);

	mainloop();

	return 0;
}