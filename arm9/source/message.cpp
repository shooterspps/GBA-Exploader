#include <stdio.h>
#include <nds.h>

#include "message.h"

char* errmsg[16];
char* cnfmsg[11];
char* cnfmsg2[3];
char* barmsg[6];
char* cmd_m[4];
char* t_msg[22];
char* savmsg[6];

static const char* errmsg_c[16] = {
	"FAT初始化失败",                       // 0
	"请应用正确的DLDI补丁",               // 1
	"未找到Slot2扩展卡",                  // 2
	"请从头重新执行",                       // 3
	"未找到SRAM保存数据",                 // 4
	"无法处理SAV文件",                    // 5
	"无法加载超过32MB的文件",             // 6
	"所选文件过大",                         // 7
	"无法加载超过16MB的文件",             // 8
	"Flash 1Mb保存类型",                   // 9
	"无法用[EXP128K]处理",                // 10
	"未找到SAV文件！",                    // 11
	"是否删除了SAV文件？",                // 12
	"(A):确认",                            // 13
	"不支持DSi/3DS主机！",                // 14
	"仅兼容DS/DS Lite"                     // 15
};

static const char* cnfmsg_c[11] = {
	"(A):确定  (B):取消",                // 0
	"将SRAM内存区存档数据",             // 1
	"导出为SAV文件",                      // 2
	"将SAV存档数据文件",                    // 3
	"导入到SRAM内存区",                   // 4
	"将SRAM.BIN数据文件",                 // 5
	"全部恢复到SRAM内存区",                      // 6
	"将SRAM全部内存区数据",                 // 7
	"备份为SRAM.BIN文件",                      // 8
	"可以将此Slot2扩展卡设置为",          // 9
	"GBA ExpLoader专用吗?"                  // 10
};

static const char* cnfmsg2_c[3] = {
	"(A):是   (B):否",                    // 0
	"检测到EZFlash Omega。",               // 1
	"这是最终版吗?"                        // 2
};

static const char* barmsg_c[6] = {
	"  正在擦除NOR...  ",                 // 0
	"  正在烧录到NOR...  ",                 // 1
	"  正在加载到RAM... ",                // 2
	"  正在分析ROM...  ",                 // 3
	"  正在读取SRAM...  ",                // 4
	"  正在写入SRAM...  "                 // 5
};

static const char* cmd_m_c[4] = {
	"  震动级别:弱  ",
	"  震动级别:中  ",
	"  震动级别:强  ",
	"  浏览器用RAM  "
};

static const char* t_msg_c[22] = {
	"选择中的游戏",                           // 0
	" PSRAM 模式",                          // 1
	"(A):启动游戏  (B):将SRAM导出为SAV",    // 2
	"(X):备份全部SRAM为SRAM.BIN",       // 3
	"(Y):从SRAM.BIN恢复到SRAM",         // 4
	"(R):切换模式",                        // 5
	"(L)/(R):切换模式",                    // 6
	"(L):切换模式",                        // 7
	" NOR 模式",                            // 8
	"(A):烧录游戏  (X):启动NOR中的游戏", // 9
	"(B):将SRAM导出为SAV文件",         // 10
	"(Y):将SAV文件导入到SRAM",            // 11
	"[%s]%d游戏",                       // 12
	"空或新状态",                           // 13
	"当前SRAM中的存档",                       // 14
	" == 未找到GBA文件 == ",              // 15
	"初始化中....",                         // 16
	"扩展模式",                             // 17
	"(A):设置模式并软复位",               // 18
	"(L):切换模式",                        // 19
	"(R):扩展RAM",                        // 20
	" SDRAM 模式",                          // 21
};

static const char* savmsg_c[6] = {
	" 将SAV文件导入到SRAM",                // 0
	"(A):导入所选文件",                    // 1
	"(B):不导入(新游戏)",                 // 2
	" 将SRAM导出为SAV文件",               // 3
	"(A):导出所选文件",                    // 4
	"(B):不导出(取消)"                    // 5
};

void setLangMsg() {
	int i;
	for (i = 0; i < 16; i++) errmsg[i] = (char*)errmsg_c[i];
	for (i = 0; i < 11; i++) cnfmsg[i] = (char*)cnfmsg_c[i];
	for (i = 0; i < 3; i++)  cnfmsg2[i] = (char*)cnfmsg2_c[i];
	for (i = 0; i < 6; i++)  barmsg[i] = (char*)barmsg_c[i];
	for (i = 0; i < 4; i++)  cmd_m[i] = (char*)cmd_m_c[i];
	for (i = 0; i < 22; i++) t_msg[i] = (char*)t_msg_c[i];
	for (i = 0; i < 6; i++)  savmsg[i] = (char*)savmsg_c[i];
}

static bool _isKanji1(u8 ch) {
	if ((ch >= 0x81) && (ch <= 0x9F))return true;
	if ((ch >= 0xE0) && (ch <= 0xEF))return true;
	if ((ch >= 0xFA) && (ch <= 0xFB))return true;
	return false;
}

char* jstrncpy(char* s1, char* s2, size_t n) {
	bool kan = false;
	char* p = s1;
	while (n) {
		n--;
		kan = _isKanji1((u8)*s2);
		if (!(*s1++ = *s2++))break;
	}
	if (kan)*(s1 - 1) = '\0';
	while (n--)*s1++ = '\0';
	*s1 = '\0';
	return(p);
}