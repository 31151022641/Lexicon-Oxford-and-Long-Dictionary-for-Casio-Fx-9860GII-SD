#include <stdlib.h>
#include "fxlib.h"
#include "Dictionary.h"
#include "ui_data.h"
#include <string.h>
#include <ctype.h>

#define SIGNAL_QUIT_TO_MENU -2
#define MAX_LINES 8
#define SEARCH_LEN 32
#define RECORD_SIZE 64
#define CACHE_SIZE 8
#define KEY_CHAR_XTT 30001
#define MY_KEY_STORE 14

typedef enum { MODE_INTERNAL, MODE_OALD_SD, MODE_LDOCE_SD } AppMode;

/* Globals */
AppMode currentMode = MODE_INTERNAL;
char searchBuffer[SEARCH_LEN + 1] = "";
int searchPos = 0;
int cursorIndex = 0;
int isInsertMode = 0;
int sd_dictionarySize = 0;
int idx_handle = -1;
int dat_handle = -1;
int showExamples = 1;
int showPron = 0;
int scrollSpeedLevel = 3;
int forceShowMode = 0;
char g_hl_word[128] = "";
char g_hl_pos[12] = "";
int g_hl_limit = 21;
int g_hl_line = 0;
int g_timer_tick = 0;
int isScrollingPaused = 0;
int g_searchOffset = 0;
int blinkState = 1;
int sw_alpha_lock = 1;

/* Cache for smooth scrolling */
typedef struct { int index; char disp[80]; char spos[8]; } ListCache;
static ListCache list_cache[CACHE_SIZE];
static int cache_base = -1;

static const unsigned char ICON_HELP[] = {0x7F, 0x50, ':', 'H', 'e', 'l', 'p', 0};
static const unsigned char ICON_HELP_HDR[] = {0x7F, 0x50, ':', ' ', 'H', 'e', 'l', 'p', 0};

const FONTCHARACTER oald_idx[] = {'\\', '\\', 'c', 'r', 'd', '0', '\\', 'O', 'A', 'L', 'D', '.', 'I', 'D', 'X', 0};
const FONTCHARACTER oald_dat[] = {'\\', '\\', 'c', 'r', 'd', '0', '\\', 'O', 'A', 'L', 'D', '.', 'D', 'A', 'T', 0};
const FONTCHARACTER ldoce_idx[] = {'\\', '\\', 'c', 'r', 'd', '0', '\\', 'L', 'D', 'O', 'C', 'E', '.', 'I', 'D', 'X', 0};
const FONTCHARACTER ldoce_dat[] = {'\\', '\\', 'c', 'r', 'd', '0', '\\', 'L', 'D', 'O', 'C', 'E', '.', 'D', 'A', 'T', 0};
const FONTCHARACTER oald_idm_idx[] = {'\\', '\\', 'c', 'r', 'd', '0', '\\', 'O', 'A', 'L', 'D', '_', 'I', 'D', 'M', '.', 'I', 'D', 'X', 0};
const FONTCHARACTER ldoce_idm_idx[] = {'\\', '\\', 'c', 'r', 'd', '0', '\\', 'L', 'D', 'O', 'C', '_', 'I', 'D', 'M', '.', 'I', 'D', 'X', 0};

int isIdiomMode = 0;
char g_jump_idm_text[80] = "";

static char def_buffer_storage[5120]; // 5KB
static char ctx_buffer_storage[512];
static char *defBuffer = def_buffer_storage;
static char *ctxBuffer = ctx_buffer_storage;
static int g_total_def_lines = 0;
static int g_scrollbar_ticks = 0;

/* Prototypes */
int HasRemainingLinkContent(const char* text, int pos);
int CountDefinitionLines(const char* text, int in_show_ex);
int FindIdiomLineOffset(const char* text, const char* idiom, int in_show_ex);
void SwitchListMode(int newMode);
void GetRootWord(char* dest, const char* src);
int CasioStrLen(const char* s);
int GetScrollInterval();
void CloseSDCardFiles();
int OpenSDCardFiles();
void UpdateSearchBar(int blinkState);
int ShowHelpScreen();
int SetupMenu();
int ShowDefinitionViewer(int index);
int BinarySearchSDCard(const char* targetWord, int searchPrefix, int* closest);
void ReadSDWord(int index, char* outDispWord, char* outSearchWord, char* outShortPos, unsigned int* outOffset);
int SafeSDRead(int is_dat, void* buf, int size, int pos);
void CasioSubStr(const char* src, int start, int count, char* dest);
void StripSyllableDots(char* str);
void StripSuperscript(char* str);
int GetCurrentScrollOffset(const char* word, int limit, int tick);
void DefViewTimerHandler(void);
void HelpTimerHandler(void);
void ListViewTimerHandler(void);
void PrintIPA(const char* s, int* currentLine);
void UnwrapIdiomDefinitions(char* buf);
int GetNextWrappedLine(const char* text, int startPos, int limit_chars, char* buffer, int* lineLen);
int HandleSearch(int currentIndex);
int MainMenu();
int GoToLibraryMenu(int* p_maxItems, int* p_topIndex, int* p_currentIndex);
int SpellCheckPopup(int closestIdx);
void PopupScrollTimer(void);
int JumpListToPrefix();
void DisplayList(int topIndex, int currentIndex);
int CaseInsensitiveCompare(const char* s1, const char* s2);
int CaseInsensitivePrefixCompare(const char* s1, const char* s2, int len);
unsigned int TranslateAlphaKey(unsigned int key);
int GetDistinctIdentifier_FromIdx(int index, char* outRoot, char* outPos);
int GetFirstSplitWord(int idx);
int GetNextDistinctWord(int idx);
int GetPrevDistinctWord(int idx);
void EnsureVisible(int* topIdx, int curIdx);
void DrawCustomGlyph(int col, int line, const unsigned char bitmap[8]);
void RenderDefinitionLine(const char* buffer, int is_shcut_draw, int currentLine);

/* Custom Greek 6x8 Bitmaps */
static const unsigned char GLYPH_PI_CAP[8]    = { 0x00, 0x3E, 0x22, 0x22, 0x22, 0x22, 0x36, 0x00 }; /* 0x10: Π */
static const unsigned char GLYPH_PI_SML[8]    = { 0x00, 0x00, 0x3E, 0x14, 0x14, 0x14, 0x22, 0x00 }; /* 0x11: π */
static const unsigned char GLYPH_THETA_CAP[8] = { 0x1E, 0x21, 0x21, 0x3F, 0x21, 0x21, 0x1E, 0x00 }; /* 0x12: Θ */
static const unsigned char GLYPH_THETA_SML[8] = { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x0E, 0x00 }; /* 0x13: θ */
static const unsigned char GLYPH_OMEGA_CAP[8] = { 0x1E, 0x21, 0x21, 0x21, 0x12, 0x33, 0x00, 0x00 }; /* 0x14: Ω */
static const unsigned char GLYPH_OMEGA_SML[8] = { 0x00, 0x00, 0x25, 0x25, 0x25, 0x1A, 0x00, 0x00 }; /* 0x15: ω */
static const unsigned char GLYPH_SIGMA_CAP[8] = { 0x3E, 0x20, 0x10, 0x08, 0x10, 0x20, 0x3E, 0x00 }; /* 0x16: Σ */
static const unsigned char GLYPH_SIGMA_SML[8] = { 0x00, 0x00, 0x1E, 0x21, 0x21, 0x21, 0x1E, 0x00 }; /* 0x17: σ */
static const unsigned char GLYPH_DELTA_CAP[8] = { 0x04, 0x0A, 0x0A, 0x11, 0x11, 0x21, 0x3F, 0x00 }; /* 0x18: Δ */
static const unsigned char GLYPH_DELTA_SML[8] = { 0x06, 0x09, 0x06, 0x19, 0x21, 0x21, 0x1E, 0x00 }; /* 0x19: δ */
static const unsigned char GLYPH_GAMMA_CAP[8] = { 0x3E, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38, 0x00 }; /* 0x1A: Γ */
static const unsigned char GLYPH_GAMMA_SML[8] = { 0x00, 0x00, 0x21, 0x12, 0x0C, 0x08, 0x10, 0x20 }; /* 0x1B: γ */
static const unsigned char GLYPH_LAMBDA_CAP[8]= { 0x04, 0x0A, 0x0A, 0x11, 0x11, 0x21, 0x33, 0x00 }; /* 0x1C: Λ */
static const unsigned char GLYPH_LAMBDA_SML[8]= { 0x10, 0x10, 0x09, 0x0A, 0x04, 0x0A, 0x11, 0x00 }; /* 0x1D: λ */
static const unsigned char GLYPH_ALPHA_SML[8] = { 0x00, 0x00, 0x1D, 0x22, 0x22, 0x25, 0x1A, 0x00 }; /* 0x1E: α */
static const unsigned char GLYPH_BETA_SML[8]  = { 0x10, 0x1E, 0x11, 0x1E, 0x11, 0x11, 0x1E, 0x10 }; /* 0x1F: β */

void DrawCustomGlyph(int col, int line, const unsigned char bitmap[8]) {
    int px = (col - 1) * 6;
    int py = (line - 1) * 8;
    int r, c;
    for (r = 0; r < 8; r++) {
        unsigned char row = bitmap[r];
        for (c = 0; c < 6; c++) {
            if (row & (1 << (5 - c))) {
                Bdisp_SetPoint_VRAM(px + c, py + r, 1);
            }
        }
    }
}

void RenderDefinitionLine(const char* buffer, int is_shcut_draw, int currentLine) {
    int cur_col = (is_shcut_draw == 2) ? 2 : 1;
    const char* s = buffer;
    if (is_shcut_draw == 1) {
        if (!strstr(buffer, "[IDM]") && !strstr(buffer, "[PHRV]") && !strstr(buffer, "[SYN]") &&
            !strstr(buffer, "[OPP]") && !strstr(buffer, "[DERIV]") && !strstr(buffer, "[FAMILY]")) {
            locate(1, currentLine);
            Print((unsigned char*)"\xE6\xA8");
            cur_col = 2;
        }
    }
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c == '\x02') { locate(cur_col, currentLine); Print((unsigned char*)"\xE5\x80"); cur_col += 2; }
        else if (c == '\x03') { if (currentMode != MODE_LDOCE_SD) { locate(cur_col, currentLine); Print((unsigned char*)"\xE6\x80"); cur_col += 2; } }
        else if (c == '\x06') { locate(cur_col, currentLine); Print((unsigned char*)"\xE6\xA8"); cur_col += 2; }
        else if (c == '\x07') { s++; continue; }
        else if (*s == '[' || *s == '(') {
            char close_ch = (*s == '[') ? ']' : ')';
            const char* end_b = strchr(s, close_ch);
            if (close_ch == ']') {
                const char* end_g = strchr(s, '>');
                if (!end_b || (end_g && end_g < end_b)) end_b = end_g;
            }
            if (end_b && (end_b - s) <= 32) {
                int tag_len = (int)(end_b - s + 1);
                char tag[40];
                int px = (cur_col - 1) * 6;
                int py = (currentLine - 1) * 8 + 2;
                int tag_cols;
                
                strncpy(tag, s, tag_len);
                tag[tag_len] = '\0';
                PrintMini(px, py, (unsigned char*)tag, 0);
                s += tag_len;
                tag_cols = (tag_len * 4 + 5) / 6;
                cur_col += tag_cols;
                continue;
            }
        }
        else if (c == 0x0C) {
            int px = (cur_col - 1) * 6;
            int py = (currentLine - 1) * 8 + 2;
            unsigned char sm[2];
            sm[0] = 0x0C;
            sm[1] = 0;
            PrintMini(px, py, sm, 0);
            cur_col += 1;
            s++;
            continue;
        }
        else if (c >= 0x20) {
            unsigned char b[3];
            b[0] = *s++;
            if (b[0] >= 0xE5 || b[0] == 0x7F) {
                b[1] = *s++; b[2] = 0;
            } else {
                b[1] = 0; b[2] = 0;
            }
            locate(cur_col, currentLine);
            Print(b);
            cur_col += 1;
            continue;
        }
        s++;
    }
}

void RenderLinkLine(const char* buffer, int is_sel, int currentLine) {
    int cur_c = 1;
    const char *s = buffer;
    locate(cur_c, currentLine);
    if (is_sel) {
        Print((unsigned char*)"\xE6\x9E"); /* Multi-byte large font 0xE69E (→) pointer at 0px / col 1 */
    } else {
        Print((unsigned char*)" ");         /* 1 space at 0px / col 1 for inactive links */
    }
    cur_c = 2;
    while (*s) {
        if (*s == '\x08' || *s == '\x0B') { s++; continue; }
        if (*s == '[') {
            const char* eb = strchr(s, ']');
            const char* eg = strchr(s, '>');
            if (!eb || (eg && eg < eb)) eb = eg;
            if (eb && (eb - s) <= 16) {
                int tlen = (int)(eb - s + 1);
                char tag[20];
                int px = (cur_c - 1) * 6;
                int py = (currentLine - 1) * 8 + 2;
                int tcols;
                strncpy(tag, s, tlen); tag[tlen] = '\0';
                PrintMini(px, py, (unsigned char*)tag, 0);
                s += tlen;
                tcols = (tlen * 4 + 5) / 6;
                cur_c += tcols;
                continue;
            }
        }
        if ((unsigned char)*s >= 0x20) {
            unsigned char b[3];
            b[0] = *s++;
            if ((unsigned char)b[0] >= 0xE5 || (unsigned char)b[0] == 0x7F) { b[1] = *s++; b[2] = 0; }
            else { b[1] = 0; b[2] = 0; }
            locate(cur_c, currentLine);
            Print(b);
            cur_c += 1;
            continue;
        }
        s++;
    }
}

/* Implementation */

unsigned int TranslateAlphaKey(unsigned int key) {
    if (key == KEY_CTRL_DEL || key == KEY_CTRL_AC || key == KEY_CTRL_EXE ||
        key == KEY_CTRL_ALPHA || key == 30022 ||
        key == KEY_CTRL_LEFT || key == KEY_CTRL_RIGHT ||
        key == KEY_CTRL_UP || key == KEY_CTRL_DOWN ||
        key == KEY_CTRL_PAGEUP || key == KEY_CTRL_PAGEDOWN || key == 30052 || key == 30053 ||
        key == KEY_CTRL_EXIT || key == KEY_CTRL_QUIT ||
        key == KEY_CTRL_OPTN || key == KEY_CTRL_INS ||
        key == KEY_CTRL_F1 || key == KEY_CTRL_F2 || key == 30010 || key == KEY_CTRL_SETUP ||
        key == KEY_CTRL_CATALOG || key == KEY_CHAR_IMGNRY) {
        return key;
    }
    if (sw_alpha_lock) {
        switch (key) {
            case KEY_CTRL_XTT: case KEY_CHAR_A: case 'a': return 'a'; 
            case KEY_CHAR_LOG: case KEY_CHAR_B: case 'b': return 'b'; 
            case KEY_CHAR_LN:  case KEY_CHAR_C: case 'c': return 'c'; 
            case KEY_CHAR_SIN: case KEY_CHAR_D: case 'd': return 'd'; 
            case KEY_CHAR_COS: case KEY_CHAR_E: case 'e': return 'e'; 
            case KEY_CHAR_TAN: case KEY_CHAR_F: case 'f': return 'f'; 
            case KEY_CHAR_FRAC:case KEY_CHAR_G: case 'g': return 'g'; 
            case KEY_CTRL_FD:  case KEY_CHAR_H: case 'h': return 'h'; 
            case KEY_CHAR_LPAR:case KEY_CHAR_I: case 'i': return 'i'; 
            case KEY_CHAR_RPAR:case KEY_CHAR_J: case 'j': return 'j'; 
            case KEY_CHAR_COMMA:case KEY_CHAR_K: case 'k': return 'k'; 
            case KEY_CHAR_STORE:case KEY_CHAR_L: case 'l': return 'l'; 
            case KEY_CHAR_7:   case KEY_CHAR_M: case 'm': return 'm'; 
            case KEY_CHAR_8:   case KEY_CHAR_N: case 'n': return 'n'; 
            case KEY_CHAR_9:   case KEY_CHAR_O: case 'o': return 'o'; 
            case KEY_CHAR_4:   case KEY_CHAR_P: case 'p': return 'p'; 
            case KEY_CHAR_5:   case KEY_CHAR_Q: case 'q': return 'q'; 
            case KEY_CHAR_6:   case KEY_CHAR_R: case 'r': return 'r'; 
            case KEY_CHAR_MULT:case '*': case KEY_CHAR_S: case 's': return 's'; 
            case KEY_CHAR_DIV: case '/': case KEY_CHAR_T: case 't': return 't'; 
            case KEY_CHAR_1:   case KEY_CHAR_U: case 'u': return 'u'; 
            case KEY_CHAR_2:   case KEY_CHAR_V: case 'v': return 'v'; 
            case KEY_CHAR_3:   case KEY_CHAR_W: case 'w': return 'w'; 
            case KEY_CHAR_PLUS:case '+': case KEY_CHAR_X: case 'x': return 'x'; 
            case KEY_CHAR_MINUS:case '-': case KEY_CHAR_Y: case 'y': return 'y'; 
            case KEY_CHAR_0:   case KEY_CHAR_Z: case 'z': return 'z'; 
            case KEY_CHAR_DP:  case ' ': return ' '; 
            case KEY_CHAR_PMINUS: case KEY_CHAR_RECIP: case KEY_CHAR_EXP: return '-';
            default: return key;
        }
    } else {
        switch (key) {
            case KEY_CHAR_7:   case KEY_CHAR_M: case 'm': return '7'; 
            case KEY_CHAR_8:   case KEY_CHAR_N: case 'n': return '8'; 
            case KEY_CHAR_9:   case KEY_CHAR_O: case 'o': return '9'; 
            case KEY_CHAR_4:   case KEY_CHAR_P: case 'p': return '4'; 
            case KEY_CHAR_5:   case KEY_CHAR_Q: case 'q': return '5'; 
            case KEY_CHAR_6:   case KEY_CHAR_R: case 'r': return '6'; 
            case KEY_CHAR_MULT:case '*': case KEY_CHAR_S: case 's': return '*'; 
            case KEY_CHAR_DIV: case '/': case KEY_CHAR_T: case 't': return '/'; 
            case KEY_CHAR_1:   case KEY_CHAR_U: case 'u': return '1'; 
            case KEY_CHAR_2:   case KEY_CHAR_V: case 'v': return '2'; 
            case KEY_CHAR_3:   case KEY_CHAR_W: case 'w': return '3'; 
            case KEY_CHAR_PLUS:case '+': case KEY_CHAR_X: case 'x': return '+'; 
            case KEY_CHAR_MINUS:case '-': case KEY_CHAR_Y: case 'y': return '-'; 
            case KEY_CHAR_0:   case KEY_CHAR_Z: case 'z': return '0'; 
            case KEY_CHAR_DP:  case ' ':           return '.'; 
            case KEY_CHAR_PMINUS: case KEY_CHAR_RECIP: case KEY_CHAR_EXP: return '-';
            default: return key;
        }
    }
}

int CasioStrLen(const char* s) {
    int len = 0; while (*s) { if ((unsigned char)*s == 0xE5 || (unsigned char)*s == 0xE6 || (unsigned char)*s == 0xE7 || (unsigned char)*s == 0x7F) { s += 2; } else if (*s == '\x09') { s++; continue; } else { s++; } len++; } return len;
}

void CasioSubStr(const char* src, int start, int count, char* dest) {
    int cur = 0, pos = 0; while (*src && cur < start) { if ((unsigned char)*src == 0xE5 || (unsigned char)*src == 0xE6 || (unsigned char)*src == 0x7F) { src += 2; } else { src++; } cur++; }
    cur = 0; while (*src && cur < count) { if ((unsigned char)*src == 0xE5 || (unsigned char)*src == 0xE6 || (unsigned char)*src == 0x7F) { dest[pos++] = *src++; dest[pos++] = *src++; } else { dest[pos++] = *src++; } cur++; } dest[pos] = '\0';
}

void StripSyllableDots(char* str) {
    unsigned char *s = (unsigned char*)str, *d = (unsigned char*)str; 
    while (*s) { 
        if (*s == 0xE5 && *(s+1) == 0xA7) { s += 2; } 
        else if (*s == 0xB7) { s++; }
        else if (*s == 0xE5 || *s == 0xE6 || *s == 0xE7 || *s == 0x7F) { *d++ = *s++; *d++ = *s++; } 
        else { *d++ = *s++; } 
    } 
    *d = '\0';
}

void StripSuperscript(char* str) {
    int i = strlen(str) - 1;
    while (i >= 0 && isdigit(str[i])) { str[i] = '\0'; i--; }
    if (i >= 1 && (unsigned char)str[i-1] == 0xE5 && (unsigned char)str[i] >= 0xC1 && (unsigned char)str[i] <= 0xC9) {
        str[i-1] = '\0';
        i -= 2;
        while (i >= 0 && isdigit(str[i])) { str[i] = '\0'; i--; }
    }
}

void GetRootWord(char* dest, const char* src) {
    int i;
    strcpy(dest, src);
    StripSyllableDots(dest);
    i = strlen(dest) - 1;
    while (i >= 0 && (dest[i] == ' ' || dest[i] == '\t')) {
        dest[i] = '\0';
        i--;
    }
    for (i = 0; dest[i] != '\0'; i++) {
        if (dest[i] == '(') {
            while (i > 0 && dest[i-1] == ' ') i--;
            dest[i] = '\0';
            break;
        }
    }
    StripSuperscript(dest);
    i = strlen(dest) - 1;
    while (i >= 0 && (dest[i] == ' ' || dest[i] == '\t')) {
        dest[i] = '\0';
        i--;
    }
}

int GetCurrentScrollOffset(const char* word, int limit, int tick) {
    int wordLen = CasioStrLen(word); if (wordLen > limit) { int max_off = wordLen - limit, off = tick % (max_off + 4); if (off > max_off) return 0; return off; } return 0;
}

void HelpTimerHandler(void) {
    if (g_scrollbar_ticks > 0) {
        g_scrollbar_ticks--;
        if (g_scrollbar_ticks == 0) {
            int y;
            for (y = 0; y < 64; y++) {
                Bdisp_SetPoint_VRAM(127, y, 0);
            }
            Bdisp_PutDisp_DD();
        }
    }
}

void DefViewTimerHandler(void) {
    char dispWord[128];
    char *marker;
    int px;
    char *s;
    DISPBOX hdr_box;
    int need_dd = 0;
    int y;

    if (g_scrollbar_ticks > 0) {
        g_scrollbar_ticks--;
        if (g_scrollbar_ticks == 0) {
            for (y = 0; y < 64; y++) {
                Bdisp_SetPoint_VRAM(127, y, 0);
            }
            need_dd = 1;
        }
    }

    if (CasioStrLen(g_hl_word) > g_hl_limit && !isScrollingPaused) {
        px = 0;
        g_timer_tick++;
        CasioSubStr(g_hl_word, GetCurrentScrollOffset(g_hl_word, g_hl_limit, g_timer_tick), g_hl_limit, dispWord);

        hdr_box.left = 0; hdr_box.top = 0; hdr_box.right = 126; hdr_box.bottom = 7;
        Bdisp_AreaClr_VRAM(&hdr_box);

        marker = strchr(dispWord, '\x09');
        if (marker) {
            *marker = '\0';
            locate(1, 1); PrintRev((unsigned char*)dispWord);
            px = CasioStrLen(dispWord) * 6;
            PrintMini(px, 1, (unsigned char*)(marker + 1), 1);
            px += strlen(marker + 1) * 4 + 4;
        } else {
            locate(1, 1); PrintRev((unsigned char*)dispWord);
            px = CasioStrLen(dispWord) * 6 + 4;
        }

        if (g_hl_pos[0]) {
            s = g_hl_pos;
            if (*s == '\x02') { PrintMini(px, 2, (unsigned char*)"\xE5\x80", 1); px += 12; s++; }
            else if (*s == '\x03') { PrintMini(px, 2, (unsigned char*)"\xE6\x80", 1); px += 12; s++; }
            if (*s) PrintMini(px, 2, (unsigned char*)s, 1);
        }
        need_dd = 1;
    }
    if (need_dd) {
        Bdisp_PutDisp_DD();
    }
}

void ListViewTimerHandler(void) {
    static int tick50 = 0;
    int need_disp = 0;
    char dispWord[128];
    char *m, *s;
    int px;
    DISPBOX row_box;

    tick50++;
    if (tick50 % (GetScrollInterval() / 50) == 0) {
        if (CasioStrLen(g_hl_word) > g_hl_limit && !isScrollingPaused) {
            g_timer_tick++;
            CasioSubStr(g_hl_word, GetCurrentScrollOffset(g_hl_word, g_hl_limit, g_timer_tick), g_hl_limit, dispWord);

            row_box.left = 0;
            row_box.top = (g_hl_line - 1) * 8;
            row_box.right = 127;
            row_box.bottom = g_hl_line * 8 - 1;
            Bdisp_AreaClr_VRAM(&row_box);

            m = strchr(dispWord, '\x09');
            if (m) {
                *m = '\0';
                locate(1, g_hl_line);
                PrintRev((unsigned char*)dispWord);
                px = CasioStrLen(dispWord) * 6;
                PrintMini(px, (g_hl_line - 1) * 8 + 2, (unsigned char*)(m + 1), 1);
                px += strlen(m + 1) * 4 + 4;
            } else {
                locate(1, g_hl_line);
                PrintRev((unsigned char*)dispWord);
                px = CasioStrLen(dispWord) * 6 + 4;
            }

            if (g_hl_pos[0]) {
                s = g_hl_pos;
                if (*s == '\x02' || *s == '\x03') s++;
                PrintMini(px, (g_hl_line - 1) * 8 + 2, (unsigned char*)s, 1);
            }
            need_disp = 1;
        }
    }
    if (tick50 % 8 == 0) {
        blinkState = 1 - blinkState;
        UpdateSearchBar(blinkState);
        need_disp = 1;
    }
    if (need_disp) Bdisp_PutDisp_DD();
}

void UpdateSearchBar(int blinkState) {
    char temp[64]; int cx, maxDisp, hasLeft, hasRight;
    if (cursorIndex < g_searchOffset) g_searchOffset = cursorIndex;
    while(1) { 
        hasLeft = (g_searchOffset > 0); 
        maxDisp = 19 - (hasLeft ? 1 : 0); 
        hasRight = (searchPos > g_searchOffset + maxDisp); 
        if (hasRight) maxDisp--; 
        if (cursorIndex >= g_searchOffset + maxDisp) g_searchOffset++; 
        else break; 
    }
    while (g_searchOffset > 0 && (searchPos - g_searchOffset) < (19 - (g_searchOffset > 1 ? 1 : 0))) {
        g_searchOffset--;
    }
    hasLeft = (g_searchOffset > 0);
    maxDisp = 19 - (hasLeft ? 1 : 0);
    hasRight = (searchPos > g_searchOffset + maxDisp);
    if (hasRight) maxDisp--;
    strcpy(temp, isIdiomMode ? "I:" : "S:"); if (hasLeft) strcat(temp, "\xE6\x9A");
    { char sub[64]; int i; for(i = 0; i < maxDisp && (g_searchOffset + i) < searchPos; i++) sub[i] = searchBuffer[g_searchOffset + i]; sub[i] = '\0'; strcat(temp, sub); }
    if (hasRight) strcat(temp, "\xE6\x9B"); while (CasioStrLen(temp) < 21) strcat(temp, " ");
    locate(1, 1); Print((unsigned char*)temp);
    if (sw_alpha_lock) Bdisp_SetPoint_VRAM(127, 0, 1); else Bdisp_SetPoint_VRAM(127, 0, 0);
    if (blinkState) { 
        cx = (2 + (hasLeft ? 1 : 0) + (cursorIndex - g_searchOffset)) * 6; 
        if (isInsertMode) Bdisp_DrawLineVRAM(cx, 0, cx, 7); 
        else Bdisp_DrawLineVRAM(cx, 7, cx + 5, 7); 
    }
}

int GetScrollInterval() { if (scrollSpeedLevel == 1) return 500; if (scrollSpeedLevel == 2) return 400; if (scrollSpeedLevel == 4) return 200; if (scrollSpeedLevel == 5) return 150; return 300; }

void CloseSDCardFiles() { if (idx_handle >= 0) Bfile_CloseFile(idx_handle); if (dat_handle >= 0) Bfile_CloseFile(dat_handle); idx_handle = -1; dat_handle = -1; sd_dictionarySize = 0; cache_base = -1; isIdiomMode = 0; }

int OpenSDCardFiles() {
    int retry; FONTCHARACTER *idx_file, *dat_file;
    if (idx_handle >= 0 && dat_handle >= 0) return 0;
    if (currentMode == MODE_OALD_SD) {
        idx_file = isIdiomMode ? (FONTCHARACTER*)oald_idm_idx : (FONTCHARACTER*)oald_idx;
        dat_file = (FONTCHARACTER*)oald_dat;
    } else if (currentMode == MODE_LDOCE_SD) {
        idx_file = isIdiomMode ? (FONTCHARACTER*)ldoce_idm_idx : (FONTCHARACTER*)ldoce_idx;
        dat_file = (FONTCHARACTER*)ldoce_dat;
    } else return -1;
    for (retry = 0; retry < 5; retry++) {
        idx_handle = Bfile_OpenFile(idx_file, _OPENMODE_READ);
        if (idx_handle >= 0) {
            sd_dictionarySize = Bfile_GetFileSize(idx_handle) / (isIdiomMode ? 128 : RECORD_SIZE);
            dat_handle = Bfile_OpenFile(dat_file, _OPENMODE_READ);
            if (dat_handle >= 0) return 0;
            Bfile_CloseFile(idx_handle);
            idx_handle = -1;
        }
        Sleep(100);
    }
    return -1;
}

void SwitchListMode(int newMode) {
    FONTCHARACTER *idx_file;
    isIdiomMode = newMode;
    if (idx_handle >= 0) {
        Bfile_CloseFile(idx_handle);
        idx_handle = -1;
    }
    if (currentMode == MODE_OALD_SD) {
        idx_file = isIdiomMode ? (FONTCHARACTER*)oald_idm_idx : (FONTCHARACTER*)oald_idx;
    } else if (currentMode == MODE_LDOCE_SD) {
        idx_file = isIdiomMode ? (FONTCHARACTER*)ldoce_idm_idx : (FONTCHARACTER*)ldoce_idx;
    } else {
        return;
    }
    idx_handle = Bfile_OpenFile(idx_file, _OPENMODE_READ);
    if (idx_handle >= 0) {
        sd_dictionarySize = Bfile_GetFileSize(idx_handle) / (isIdiomMode ? 128 : RECORD_SIZE);
    } else {
        sd_dictionarySize = 0;
    }
    cache_base = -1;
    searchPos = 0;
    cursorIndex = 0;
    searchBuffer[0] = '\0';
    g_searchOffset = 0;
}

void PrintIPA(const char* s, int* currentLine) {
    unsigned char buf[3]; int ipa_x = 0;
    while (*s) {
        if (ipa_x > 120) { ipa_x = 0; (*currentLine)++; if (*currentLine > MAX_LINES) break; }
        if ((unsigned char)*s == 0x49 || (unsigned char)*s == 0x40) { buf[0] = *s++; buf[1] = 0; PrintMini(ipa_x, (*currentLine - 1) * 8 + 2, buf, 0); ipa_x += 6; }
        else if ((unsigned char)*s == 0xE5 || (unsigned char)*s == 0xE6 || (unsigned char)*s == 0xE7 || (unsigned char)*s == 0x7F) { buf[0] = *s++; buf[1] = *s++; buf[2] = 0; locate(ipa_x/6 + 1, *currentLine); Print(buf); ipa_x += 6; }
        else { buf[0] = *s++; buf[1] = 0; locate(ipa_x/6 + 1, *currentLine); Print(buf); ipa_x += 6; }
    }
}

int SafeSDRead(int is_dat, void* buf, int size, int pos) {
    int handle = is_dat ? dat_handle : idx_handle, res = Bfile_ReadFile(handle, buf, size, pos);
    if (res < 0) { if (OpenSDCardFiles() == 0) return Bfile_ReadFile(is_dat ? dat_handle : idx_handle, buf, size, pos); } return res;
}

void ReadSDWord(int index, char* outDispWord, char* outSearchWord, char* outShortPos, unsigned int* outOffset) {
    if (idx_handle < 0) { if (OpenSDCardFiles() < 0) return; }
    if (isIdiomMode) {
        int pos = index * 128;
        if (outDispWord) { SafeSDRead(0, outDispWord, 76, pos); outDispWord[75] = '\0'; }
        if (outSearchWord) { SafeSDRead(0, outSearchWord, 48, pos + 76); outSearchWord[47] = '\0'; }
        if (outShortPos) { outShortPos[0] = '\0'; }
        if (outOffset) { unsigned int rv = 0; unsigned char* b = (unsigned char*)&rv; SafeSDRead(0, &rv, 4, pos + 124); *outOffset = (unsigned int)b[0] | ((unsigned int)b[1] << 8) | ((unsigned int)b[2] << 16) | ((unsigned int)b[3] << 24); }
    } else {
        int pos = index * RECORD_SIZE;
        if (outDispWord) { SafeSDRead(0, outDispWord, 24, pos); outDispWord[23] = '\0'; }
        if (outSearchWord) { SafeSDRead(0, outSearchWord, 24, pos + 24); outSearchWord[23] = '\0'; }
        if (outShortPos) { SafeSDRead(0, outShortPos, 8, pos + 48); outShortPos[7] = '\0'; }
        if (outOffset) { unsigned int rv = 0; unsigned char* b = (unsigned char*)&rv; SafeSDRead(0, &rv, 4, pos + 56); *outOffset = (unsigned int)b[0] | ((unsigned int)b[1] << 8) | ((unsigned int)b[2] << 16) | ((unsigned int)b[3] << 24); }
    }
}

int ShowHelpScreen() {
    unsigned int key; 
    int scroll = 0, i;
    int page_l = 8;
    int total_l, max_s;
    const char* help_lines[] = {
        "=== GLOBAL KEYS ===",
        "SHIFT+EXIT: Library Menu",
        "F1 / HELP : Help Screen",
        "SETUP     : Preferences",
        " ",
        "=== LIST VIEW ===",
        "A - Z     : Search words",
        "ALPHA     : Alpha-Lock",
        "DEL       : Backspace",
        "AC        : Clear search box",
        "UP / DOWN : Scroll 1 word",
        "SHIFT+U/D : Page scroll (7)",
        "LEFT/RIGHT: Jump list pages",
        "EXE       : Open Definition",
        "F1        : Fuzzy Search",
        "F2        : Words / Idioms",
        "INS       : Insert/Overwrite",
        "OPTN      : Pause scroll",
        " ",
        "=== DEFINITION VIEW ===",
        "UP / DOWN : Scroll 1 line",
        "SHIFT+U/D : Page scroll (7)",
        "LEFT/RIGHT: Prev / Next word",
        ", (COMMA) : Previous split",
        "-> (STORE): Next split",
        "(  /  )   : Toggle Pronun",
        "X,theta,T : Grammar & Links",
        "EXE       : Jump to link",
        "EXIT      : Back to List",
        " ",
        "=== GRAMMAR & LINKS ===",
        "UP / DOWN : Select item",
        "EXE       : Jump to word",
        "X,theta,T : Back to Def",
        "EXIT      : Back to List",
        " ",
        "=== SETUP MENU ===",
        "UP / DOWN : Select option",
        "EXE / F1  : Toggle setting",
        "F6        : Default settings",
        "EXIT      : Save & Close",
        " ",
        "=== ABOUT LEXICON ===",
        "Oxford Advanced Learner's",
        "Dictionary, 10th edition",
        "(C) Oxford University",
        "Press 2015",
        " ",
        "Longman Dictionary of",
        "Contemporary English",
        "(C) Pearson Education",
        "Limited 2015",
        " ",
        "Casio fx-9860G SD Edition"
    };
    total_l = sizeof(help_lines) / sizeof(help_lines[0]);
    max_s = total_l - page_l;
    g_scrollbar_ticks = 0;
    while (1) {
        Bdisp_AllClr_DDVRAM();
        for (i = 0; i < page_l; i++) {
            int line_idx = scroll + i;
            if (line_idx < total_l) {
                const char* line = help_lines[line_idx];
                int y = i * 8;
                if (line[0] != ' ') {
                    PrintMini(2, y + 1, (unsigned char*)line, 0);
                }
            }
        }
        if (g_scrollbar_ticks > 0) {
            int track_h = 64;
            int thumb_len = (page_l * track_h) / total_l;
            int thumb_y, y;
            if (thumb_len < 4) thumb_len = 4;
            if (thumb_len > 60) thumb_len = 60;
            if (max_s <= 0) thumb_y = 0;
            else if (scroll >= max_s) thumb_y = track_h - thumb_len;
            else thumb_y = (scroll * (track_h - thumb_len)) / max_s;
            for (y = 0; y < 64; y++) {
                if (y >= thumb_y && y < thumb_y + thumb_len) {
                    Bdisp_SetPoint_VRAM(127, y, 1);
                } else {
                    Bdisp_SetPoint_VRAM(127, y, 0);
                }
            }
        }
        Bdisp_PutDisp_DD(); 
        SetTimer(1, 200, HelpTimerHandler); 
        GetKey(&key); 
        KillTimer(1);
        if (key == KEY_CTRL_QUIT) { g_scrollbar_ticks = 0; return SIGNAL_QUIT_TO_MENU; }
        if (key == KEY_CTRL_EXIT || key == 30100 || key == 0x7f50 || key == KEY_CTRL_F1) { g_scrollbar_ticks = 0; return 0; }
        if (key == KEY_CTRL_UP) { if (scroll > 0) scroll--; g_scrollbar_ticks = 5; }
        else if (key == KEY_CTRL_DOWN) { if (scroll < max_s) scroll++; g_scrollbar_ticks = 5; }
        else if (key == KEY_CTRL_PAGEUP || key == 30052 || key == KEY_CTRL_LEFT) { scroll -= 7; if (scroll < 0) scroll = 0; g_scrollbar_ticks = 5; }
        else if (key == KEY_CTRL_PAGEDOWN || key == 30053 || key == KEY_CTRL_RIGHT) { scroll += 7; if (scroll > max_s) scroll = max_s; g_scrollbar_ticks = 5; }
    }
}

int SetupMenu() {
    unsigned int key; int sel = 0;
    while(1) {
        Bdisp_AllClr_DDVRAM(); locate(1, 1); if (sel == 0) PrintRev((unsigned char*)"Example: "); else Print((unsigned char*)"Example: ");
        if (showExamples) Print((unsigned char*)"ON "); else Print((unsigned char*)"OFF");
        locate(1, 2); if (sel == 1) PrintRev((unsigned char*)"Pronun : "); else Print((unsigned char*)"Pronun : ");
        if (showPron == 0) Print((unsigned char*)"HIDE"); else if (showPron == 1) Print((unsigned char*)"BrE "); else if (showPron == 2) Print((unsigned char*)"NAmE"); else Print((unsigned char*)"BOTH");
        locate(1, 3); if (sel == 2) PrintRev((unsigned char*)"Scroll : "); else Print((unsigned char*)"Scroll : ");
        { unsigned char s[2]; s[0] = '0' + scrollSpeedLevel; s[1] = 0; Print(s); }
        locate(1, 8); PrintRev((unsigned char*)"F1:TOG  F6:DFLT EXIT");
        Bdisp_PutDisp_DD(); GetKey(&key); if (key == KEY_CTRL_QUIT) return SIGNAL_QUIT_TO_MENU; if (key == 30100 || key == 0x7f50) { if (ShowHelpScreen() == SIGNAL_QUIT_TO_MENU) return SIGNAL_QUIT_TO_MENU; continue; }
        if (key == KEY_CTRL_UP && sel > 0) sel--; else if (key == KEY_CTRL_DOWN && sel < 2) sel++; else if (key == KEY_CTRL_F1 || key == KEY_CTRL_EXE) { if (sel == 0) showExamples = 1 - showExamples; else if (sel == 1) showPron = (showPron + 1) % 4; else if (sel == 2) { scrollSpeedLevel++; if (scrollSpeedLevel > 6) scrollSpeedLevel = 1; } }
        else if (key == KEY_CTRL_F6) { showExamples = 1; showPron = 0; scrollSpeedLevel = 3; } else if (key == KEY_CTRL_EXIT) return 0;
    }
}

void UnwrapIdiomDefinitions(char* buf) {
    /* Preserve separate lines for each idiom definition and example */
}

int GetNextWrappedLine(const char* text, int startPos, int limit_chars, char* buffer, int* lineLen) {
    int pos = startPos, chars = 0, lastSpace = -1, lastSpaceBufLen = -1, tlen;
    *lineLen = 0;
    buffer[0] = '\0';
    if (!text) return 0;
    tlen = strlen(text);
    if (pos >= tlen) return pos;
    if (text[pos] == '\n') return pos + 1;

    /* Skip any leading spaces caused by wrapping from previous line */
    while (pos < tlen && text[pos] == ' ') pos++;
    if (pos >= tlen || text[pos] == '\n') {
        if (pos < tlen && text[pos] == '\n') pos++;
        return pos;
    }

    while (pos < tlen && text[pos] != '\n' && chars < limit_chars) {
        if (text[pos] == ' ') {
            lastSpace = pos;
            lastSpaceBufLen = *lineLen;
        }
        if (text[pos] == '[' || text[pos] == '(') {
            char close_ch = (text[pos] == '[') ? ']' : ')';
            const char* eb = strchr(text + pos, close_ch);
            if (close_ch == ']') {
                const char* eg = strchr(text + pos, '>');
                if (!eb || (eg && eg < eb)) eb = eg;
            }
            if (eb && (eb - (text + pos)) <= 32) {
                int tlen_tag = (int)(eb - (text + pos) + 1);
                int tcols = (tlen_tag * 4 + 5) / 6;
                if (chars + tcols <= limit_chars || chars == 0) {
                    int k;
                    for (k = 0; k < tlen_tag; k++) {
                        buffer[(*lineLen)++] = text[pos++];
                    }
                    chars += tcols;
                    lastSpace = pos;
                    lastSpaceBufLen = *lineLen;
                    continue;
                } else {
                    break;
                }
            }
        }
        if ((unsigned char)text[pos] >= 0xE5 || (unsigned char)text[pos] == 0x7F) {
            if (chars + 1 > limit_chars) break;
            buffer[(*lineLen)++] = text[pos++];
            buffer[(*lineLen)++] = text[pos++];
            chars += 1;
        } else {
            buffer[(*lineLen)++] = text[pos++];
            chars++;
        }
    }

    /* If we exceeded limit and next char is part of an unbroken word, break at lastSpace */
    if (pos < tlen && text[pos] != '\n' && text[pos] != ' ' && lastSpace > startPos && lastSpaceBufLen > 0) {
        pos = lastSpace + 1;
        *lineLen = lastSpaceBufLen;
    }

    /* Trim trailing spaces from line */
    while (*lineLen > 0 && buffer[*lineLen - 1] == ' ') {
        (*lineLen)--;
    }
    buffer[*lineLen] = '\0';

    /* Consume space at break point and advance past newline if present */
    while (pos < tlen && text[pos] == ' ') pos++;
    if (pos < tlen && text[pos] == '\n') pos++;

    return pos;
}

int HasRemainingContent(const char* text, int pos, int in_ex, int showExamples) {
    int tlen;
    if (!text) return 0;
    tlen = strlen(text);
    while (pos < tlen) {
        unsigned char c = (unsigned char)text[pos];
        if (c == '\x08' || c == '\x05') {
            while (pos < tlen && text[pos] != '\n') pos++;
            if (pos < tlen && text[pos] == '\n') pos++;
            continue;
        }
        if (c == '\x01') { in_ex = 1; pos++; continue; }
        if (c == '\x04') { in_ex = 0; pos++; continue; }
        if (c == '\x06') { pos++; continue; }
        if (in_ex && !showExamples) {
            while (pos < tlen && text[pos] != '\n') pos++;
            if (pos < tlen && text[pos] == '\n') pos++;
            in_ex = 0;
            continue;
        }
        if ((c == 0x0C || c > ' ') && c != '\n' && c != '\r') return 1;
        pos++;
    }
    return 0;
}

int HasRemainingLinkContent(const char* text, int pos) {
    int tlen;
    if (!text) return 0;
    tlen = strlen(text);
    while (pos < tlen) {
        if (pos == 0 || text[pos - 1] == '\n') {
            if (text[pos] == '\x05' || text[pos] == '\x08') return 1;
        }
        pos++;
    }
    return 0;
}

int CountDefinitionLines(const char* text, int in_show_ex) {
    int pos = 0, lines = 0, tlen;
    int in_ex = 0, is_new_ex = 0, is_shortcut = 0;
    char buf[256];
    int lineLen = 0;

    if (!text) return 0;
    tlen = strlen(text);
    while (pos < tlen) {
        int limit = 21;
        if (pos == 0 || text[pos-1] == '\n') {
            if (text[pos] == '\x08' || text[pos] == '\x05') {
                while (pos < tlen && text[pos] != '\n') pos++;
                if (pos < tlen && text[pos] == '\n') pos++;
                continue;
            }
            if (text[pos] == '\x06') { is_shortcut = 1; in_ex = 0; is_new_ex = 0; pos++; }
            else if (text[pos] == '\x07') { is_shortcut = 0; in_ex = 0; is_new_ex = 0; pos++; }
            else if (text[pos] == '\x01') { in_ex = 1; is_shortcut = 0; is_new_ex = 1; pos++; }
            else if (text[pos] == '\x04') { in_ex = 0; is_shortcut = 0; is_new_ex = 0; pos++; }
            else { in_ex = 0; is_shortcut = 0; is_new_ex = 0; }
        }
        if (in_ex) {
            if (!in_show_ex) {
                while (pos < tlen && text[pos] != '\n') pos++;
                if (pos < tlen && text[pos] == '\n') pos++;
                in_ex = 0; is_new_ex = 0;
                continue;
            }
            if (is_new_ex) { limit = 28; is_new_ex = 0; }
            else { limit = 30; }
        } else if (is_shortcut) {
            limit = 19;
            if (is_shortcut == 1) is_shortcut = 2;
        }

        pos = GetNextWrappedLine(text, pos, limit, buf, &lineLen);
        if (pos > 0 && text[pos-1] == '\n') { is_new_ex = 1; is_shortcut = 0; }
        lines++;
    }
    return lines;
}

int FindIdiomLineOffset(const char* text, const char* idiom, int in_show_ex) {
    int pos = 0, lines = 0, tlen;
    int in_ex = 0, is_new_ex = 0, is_shortcut = 0;
    char buf[256];
    int lineLen = 0;
    char *match = NULL;
    const char *p_start;
    char clean_idm[32];
    int k, m_pos;

    if (!text || !idiom || !idiom[0]) return -1;
    tlen = strlen(text);

    p_start = idiom;
    if (*p_start == '(') {
        const char* p_close = strchr(p_start, ')');
        if (p_close) p_start = p_close + 1;
        while (*p_start == ' ') p_start++;
    }

    k = 0;
    while (p_start[k] && p_start[k] != '/' && p_start[k] != '(' && k < 20) {
        clean_idm[k] = p_start[k];
        k++;
    }
    clean_idm[k] = '\0';
    while (k > 0 && (clean_idm[k-1] == ' ' || clean_idm[k-1] == '\t')) clean_idm[--k] = '\0';

    if (k < 2) return -1;

    {
        char idm_tag[40];
        strcpy(idm_tag, "[IDM] ");
        strcat(idm_tag, clean_idm);
        match = strstr(text, idm_tag);
    }
    if (!match) match = strstr(text, clean_idm);
    if (!match && k >= 6) {
        clean_idm[8] = '\0';
        match = strstr(text, clean_idm);
    }

    if (!match) return -1;

    m_pos = (int)(match - text);
    if (m_pos >= 6 && strncmp(match - 6, "[IDM] ", 6) == 0) {
        m_pos -= 6;
    }

    while (pos < tlen) {
        int limit = 21;
        if (pos >= m_pos) {
            return lines;
        }
        if (pos == 0 || text[pos-1] == '\n') {
            if (text[pos] == '\x08' || text[pos] == '\x05') {
                while (pos < tlen && text[pos] != '\n') pos++;
                if (pos < tlen && text[pos] == '\n') pos++;
                continue;
            }
            if (text[pos] == '\x06') { is_shortcut = 1; in_ex = 0; is_new_ex = 0; pos++; }
            else if (text[pos] == '\x07') { is_shortcut = 0; in_ex = 0; is_new_ex = 0; pos++; }
            else if (text[pos] == '\x01') { in_ex = 1; is_shortcut = 0; is_new_ex = 1; pos++; }
            else if (text[pos] == '\x04') { in_ex = 0; is_shortcut = 0; is_new_ex = 0; pos++; }
            else { in_ex = 0; is_shortcut = 0; is_new_ex = 0; }
        }
        if (in_ex) {
            if (!in_show_ex) {
                while (pos < tlen && text[pos] != '\n') pos++;
                if (pos < tlen && text[pos] == '\n') pos++;
                in_ex = 0; is_new_ex = 0;
                continue;
            }
            if (is_new_ex) { limit = 28; is_new_ex = 0; }
            else { limit = 30; }
        } else if (is_shortcut) {
            limit = 19;
            if (is_shortcut == 1) is_shortcut = 2;
        }

        pos = GetNextWrappedLine(text, pos, limit, buf, &lineLen);
        if (pos > 0 && text[pos-1] == '\n') { is_new_ex = 1; is_shortcut = 0; }
        lines++;
    }
    return -1;
}

int ShowDefinitionViewer(int index) {
    unsigned int key; 
    int scrollOffset = 0, currentIndex = index, viewScreen = 0;
    int defScrollOffset = 0, linkScrollOffset = 0;
    int maxItems = (currentMode == MODE_INTERNAL) ? dictionarySize : sd_dictionarySize;
    char word[128], pos_str[32], ipa_br[128], ipa_nam[128], dispWord[128], *definition, *s; 
    unsigned int offset; 
    int i, j, reset_view = 1, highlight_idx = 0, has_ctx_data = 0;
    int item_positions[64], item_lines[64], item_count = 0; 
    char item_jump_words[64][32];
    int def_len;
    int in_ex = 0, is_shortcut = 0, shift_pressed = 0, is_new_ex = 0;
    
    while (1) {
        if (reset_view) {
            char c;
            if (currentMode == MODE_INTERNAL) {
                strncpy(word, dictionary[currentIndex].word, 127); word[127] = '\0';
                pos_str[0] = '\0'; ipa_br[0] = '\0'; ipa_nam[0] = '\0';
                definition = (char*)dictionary[currentIndex].definition;
                strcpy(defBuffer, definition);
                ctxBuffer[0] = '\0';
            } else if (currentMode == MODE_OALD_SD) {
                ReadSDWord(currentIndex, NULL, NULL, NULL, &offset);
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 127) word[i++] = c; word[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 31) pos_str[i++] = c; pos_str[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 127) ipa_br[i++] = c; ipa_br[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 127) ipa_nam[i++] = c; ipa_nam[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 5119) defBuffer[i++] = c; defBuffer[i] = '\0'; offset += i + 1;
                j = 0; if (i < 5118) defBuffer[i++] = '\n';
                while (SafeSDRead(1, &c, 1, offset + j) == 1 && c != '\0' && i < 5119) { defBuffer[i++] = c; j++; } defBuffer[i] = '\0'; offset += j + 1;
                while (i > 0 && (defBuffer[i-1] == '\n' || defBuffer[i-1] == '\r' || defBuffer[i-1] == ' ')) defBuffer[--i] = '\0';
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 511) ctxBuffer[i++] = c; ctxBuffer[i] = '\0'; offset += i + 1;
                while (i > 0 && (ctxBuffer[i-1] == '\n' || ctxBuffer[i-1] == '\r' || ctxBuffer[i-1] == ' ')) ctxBuffer[--i] = '\0';
            } else if (currentMode == MODE_LDOCE_SD) {
                ReadSDWord(currentIndex, NULL, NULL, NULL, &offset);
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 127) word[i++] = c; word[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 31) pos_str[i++] = c; pos_str[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 127) ipa_br[i++] = c; ipa_br[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 127) ipa_nam[i++] = c; ipa_nam[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 5119) defBuffer[i++] = c; defBuffer[i] = '\0'; offset += i + 1;
                j = 0; if (i < 5118) defBuffer[i++] = '\n';
                while (SafeSDRead(1, &c, 1, offset + j) == 1 && c != '\0' && i < 5119) { defBuffer[i++] = c; j++; } defBuffer[i] = '\0'; offset += j + 1;
                while (i > 0 && (defBuffer[i-1] == '\n' || defBuffer[i-1] == '\r' || defBuffer[i-1] == ' ')) defBuffer[--i] = '\0';
                ctxBuffer[0] = '\0';
            }
            UnwrapIdiomDefinitions(defBuffer);
            if (ipa_br[0]) { int len = strlen(ipa_br); if (len < 126) { memmove(ipa_br + 1, ipa_br, len + 1); ipa_br[0] = '/'; ipa_br[len + 1] = '/'; ipa_br[len + 2] = '\0'; } }
            if (ipa_nam[0]) { int len = strlen(ipa_nam); if (len < 126) { memmove(ipa_nam + 1, ipa_nam, len + 1); ipa_nam[0] = '/'; ipa_nam[len + 1] = '/'; ipa_nam[len + 2] = '\0'; } }
            
            if (ctxBuffer[0]) {
                char *p_lit = strstr(ctxBuffer, "(or literary)");
                if (p_lit && strlen(ctxBuffer) + 14 < 2047) {
                    char temp[2048];
                    int prefix_len = (int)(p_lit - ctxBuffer);
                    strncpy(temp, ctxBuffer, prefix_len);
                    temp[prefix_len] = '\0';
                    strcat(temp, "(old-fashioned or literary)");
                    strcat(temp, p_lit + 13);
                    strcpy(ctxBuffer, temp);
                }
            }
            
            has_ctx_data = (ctxBuffer[0] != '\0');
            if (!has_ctx_data) { 
                char *p = defBuffer; 
                while (*p) { 
                    if (*p == '\x08' || *p == '\x05' || *p == '<') { has_ctx_data = 1; break; } 
                    p++; 
                } 
            }
            
            item_count = 0; i = 0;
            while (defBuffer[i]) {
                if (defBuffer[i] == '\x08' && item_count < 64) {
                    int k = 0, jp = i + 1; item_positions[item_count] = i;
                    while (defBuffer[jp] && defBuffer[jp] != '\n' && defBuffer[jp] != '\x0B') jp++;
                    if (defBuffer[jp] == '\x0B') { jp++; while (defBuffer[jp] && defBuffer[jp] != '\n' && k < 31) item_jump_words[item_count][k++] = defBuffer[jp++]; }
                    item_jump_words[item_count][k] = '\0'; item_count++;
                }
                while (defBuffer[i] && defBuffer[i] != '\n') i++; if (defBuffer[i] == '\n') i++;
            }
            isScrollingPaused = 0; g_timer_tick = 0; scrollOffset = 0; defScrollOffset = 0; linkScrollOffset = 0; highlight_idx = 0; reset_view = 0;
            g_total_def_lines = CountDefinitionLines(defBuffer, showExamples);
            g_scrollbar_ticks = 0;
            if (g_jump_idm_text[0] != '\0') {
                int target_line = FindIdiomLineOffset(defBuffer, g_jump_idm_text, showExamples);
                if (target_line >= 0) {
                    scrollOffset = target_line;
                    defScrollOffset = target_line;
                }
                g_jump_idm_text[0] = '\0';
            }
        }
        {
            int pos = 0, currentLine = 1, visible_lines = 0, has_more = 0;
            int pos_on_line1 = 1, pos_w = 0;
            char *ps;
            Bdisp_AllClr_VRAM();
            if (pos_str[0]) {
                ps = pos_str;
                if (*ps == '\x02' || *ps == '\x03') { pos_w += 12; ps++; }
                pos_w += strlen(ps) * 4;
                if (CasioStrLen(word) * 6 + 4 + pos_w <= 126) {
                    pos_on_line1 = 1;
                    g_hl_limit = (126 - pos_w - 4) / 6;
                } else {
                    pos_on_line1 = 0;
                    g_hl_limit = 21;
                }
            } else {
                pos_on_line1 = 0;
                g_hl_limit = 21;
            }
            strcpy(g_hl_word, word);
            if (pos_on_line1 && pos_str[0]) strcpy(g_hl_pos, pos_str); else g_hl_pos[0] = '\0';
            g_hl_line = 1;

            CasioSubStr(word, GetCurrentScrollOffset(word, g_hl_limit, g_timer_tick), g_hl_limit, dispWord);
            {
                char *marker = strchr(dispWord, '\x09');
                int px = 0;
                if (marker) {
                    *marker = '\0';
                    locate(1, 1); PrintRev((unsigned char*)dispWord);
                    px = CasioStrLen(dispWord) * 6;
                    PrintMini(px, 1, (unsigned char*)(marker + 1), 1);
                    px += strlen(marker + 1) * 4 + 4;
                } else {
                    locate(1, 1); PrintRev((unsigned char*)dispWord);
                    px = CasioStrLen(dispWord) * 6 + 4;
                }
                if (pos_str[0]) {
                    if (pos_on_line1) {
                        s = pos_str;
                        if (*s == '\x02') { PrintMini(px, 2, (unsigned char*)"\xE5\x80", 1); px += 12; s++; }
                        else if (*s == '\x03') { PrintMini(px, 2, (unsigned char*)"\xE6\x80", 1); px += 12; s++; }
                        if (*s) PrintMini(px, 2, (unsigned char*)s, 1);
                        currentLine = 2;
                    } else {
                        int px2 = 0;
                        s = pos_str;
                        if (*s == '\x02') { PrintMini(px2, (2-1)*8+2, (unsigned char*)"\xE5\x80", 0); px2 += 12; s++; }
                        else if (*s == '\x03') { PrintMini(px2, (2-1)*8+2, (unsigned char*)"\xE6\x80", 0); px2 += 12; s++; }
                        if (*s) PrintMini(px2, (2-1)*8+2, (unsigned char*)s, 0);
                        currentLine = 3;
                    }
                } else {
                    currentLine = 2;
                }
            }

            if (currentMode == MODE_INTERNAL) {            if (viewScreen == 0) {
                if (showPron == 3) { 
                    if (ipa_br[0]) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"BrE", 0); currentLine++; PrintIPA(ipa_br, &currentLine); currentLine++; }
                    if (ipa_nam[0] && currentLine <= MAX_LINES) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"NAmE", 0); currentLine++; PrintIPA(ipa_nam, &currentLine); currentLine++; }
                } else {
                    int dBrE = (showPron == 1) || (forceShowMode == 1), dNAmE = (showPron == 2) || (forceShowMode == 2);
                    if (dBrE && ipa_br[0]) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"BrE", 0); currentLine++; PrintIPA(ipa_br, &currentLine); currentLine++; }
                    if (dNAmE && ipa_nam[0] && currentLine <= MAX_LINES) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"NAmE", 0); currentLine++; PrintIPA(ipa_nam, &currentLine); currentLine++; }
                }
                in_ex = 0; is_shortcut = 0; is_new_ex = 0;
                definition = defBuffer; def_len = strlen(definition);
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0, limit = 21, start_p = pos, is_f_ex = -1, is_shcut_draw = 0;
                    if (pos == 0 || definition[pos-1] == '\n') {
                        if (definition[pos] == '\x08' || definition[pos] == '\x05') { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                        if (definition[pos] == '\x06') { is_shortcut = 1; in_ex = 0; is_new_ex = 0; pos++; }
                        else if (definition[pos] == '\x07') { is_shortcut = 0; in_ex = 0; is_new_ex = 0; pos++; }
                        else if (definition[pos] == '\x01') { in_ex = 1; is_shortcut = 0; is_new_ex = 1; pos++; }
                        else if (definition[pos] == '\x04') { in_ex = 0; is_shortcut = 0; is_new_ex = 0; pos++; }
                        else { in_ex = 0; is_shortcut = 0; is_new_ex = 0; }
                    }
                    if (in_ex) { 
                        if (!showExamples) { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; in_ex = 0; is_new_ex = 0; continue; }
                        if (is_new_ex) { is_f_ex = 4; limit = 28; is_new_ex = 0; }
                        else { is_f_ex = 0; limit = 30; }
                    } else if (is_shortcut) { 
                        limit = 19;
                        if (is_shortcut == 1) { is_shcut_draw = 1; is_shortcut = 2; }
                        else { is_shcut_draw = 2; }
                    }
                    
                    pos = GetNextWrappedLine(definition, pos, limit, buffer, &lineLen);
                    if (pos > 0 && definition[pos-1] == '\n') { is_new_ex = 1; is_shortcut = 0; }
                    
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { if (HasRemainingContent(definition, start_p, in_ex, showExamples)) has_more = 1; break; }
                        if (is_f_ex >= 0) { 
                            PrintMini(is_f_ex, (currentLine-1)*8+2, (unsigned char*)buffer, 0); 
                        } else { 
                            RenderDefinitionLine(buffer, is_shcut_draw, currentLine);
                        }
                        currentLine++;
                    }
                    visible_lines++;
                }
            } else {
                int in_f_gram = 0, in_link = 0;
                definition = ctxBuffer; def_len = strlen(definition);
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0;
                    pos = GetNextWrappedLine(definition, pos, 30, buffer, &lineLen);
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { has_more = 1; break; }
                        PrintMini(0, (currentLine-1)*8+2, (unsigned char*)buffer, 0); currentLine++;
                    }
                    visible_lines++;
                }
                if (!has_more) {
                    definition = defBuffer; def_len = strlen(definition); pos = 0;
                    while (definition && pos < def_len) {
                        char buffer[256]; int lineLen = 0, start_p = pos;
                        if (pos == 0 || definition[pos-1] == '\n') {
                            if (definition[pos] == '\x05') { in_f_gram = 1; in_link = 0; pos++; }
                            else if (definition[pos] == '\x08') { in_link = 1; in_f_gram = 0; pos++; }
                            else { in_f_gram = 0; in_link = 0; while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                        }
                        pos = GetNextWrappedLine(definition, pos, 30, buffer, &lineLen);
                        if (in_link) {
                            for (j = 0; j < item_count; j++) {
                                if (item_positions[j] == start_p) { item_lines[j] = visible_lines; break; }
                            }
                        }
                        if (visible_lines >= scrollOffset) {
                            if (currentLine > 8) { if (HasRemainingLinkContent(definition, start_p)) has_more = 1; break; }
                            if (in_f_gram) { 
                                char* p = buffer; int lb;
                                if (*p == '[') p++;
                                lb = strlen(p);
                                if (lb > 0 && p[lb-1] == ']') p[lb-1] = '\0';
                                PrintMini(0, (currentLine-1)*8+2, (unsigned char*)p, 0); 
                            }
                            else if (in_link) {
                                RenderLinkLine(buffer, (highlight_idx < item_count && item_positions[highlight_idx] == start_p), currentLine);
                            }
                            currentLine++;
                        }
                        visible_lines++;
                    }
                }
                if (has_more) { locate(21, 8); Print((unsigned char*)"\xE6\xAD"); }
            }
            } else if (currentMode == MODE_OALD_SD) {            if (viewScreen == 0) {
                if (showPron == 3) { 
                    if (ipa_br[0]) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"BrE", 0); currentLine++; PrintIPA(ipa_br, &currentLine); currentLine++; }
                    if (ipa_nam[0] && currentLine <= MAX_LINES) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"NAmE", 0); currentLine++; PrintIPA(ipa_nam, &currentLine); currentLine++; }
                } else {
                    int dBrE = (showPron == 1) || (forceShowMode == 1), dNAmE = (showPron == 2) || (forceShowMode == 2);
                    if (dBrE && ipa_br[0]) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"BrE", 0); currentLine++; PrintIPA(ipa_br, &currentLine); currentLine++; }
                    if (dNAmE && ipa_nam[0] && currentLine <= MAX_LINES) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"NAmE", 0); currentLine++; PrintIPA(ipa_nam, &currentLine); currentLine++; }
                }
                in_ex = 0; is_shortcut = 0; is_new_ex = 0;
                definition = defBuffer; def_len = strlen(definition);
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0, limit = 21, start_p = pos, is_f_ex = -1, is_shcut_draw = 0;
                    if (pos == 0 || definition[pos-1] == '\n') {
                        if (definition[pos] == '\x08' || definition[pos] == '\x05') { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                        if (definition[pos] == '\x06') { is_shortcut = 1; in_ex = 0; is_new_ex = 0; pos++; }
                        else if (definition[pos] == '\x07') { is_shortcut = 0; in_ex = 0; is_new_ex = 0; pos++; }
                        else if (definition[pos] == '\x01') { in_ex = 1; is_shortcut = 0; is_new_ex = 1; pos++; }
                        else if (definition[pos] == '\x04') { in_ex = 0; is_shortcut = 0; is_new_ex = 0; pos++; }
                        else { in_ex = 0; is_shortcut = 0; is_new_ex = 0; }
                    }
                    if (in_ex) { 
                        if (!showExamples) { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; in_ex = 0; is_new_ex = 0; continue; }
                        if (is_new_ex) { is_f_ex = 4; limit = 28; is_new_ex = 0; }
                        else { is_f_ex = 0; limit = 30; }
                    } else if (is_shortcut) { 
                        limit = 19;
                        if (is_shortcut == 1) { is_shcut_draw = 1; is_shortcut = 2; }
                        else { is_shcut_draw = 2; }
                    }
                    
                    pos = GetNextWrappedLine(definition, pos, limit, buffer, &lineLen);
                    if (pos > 0 && definition[pos-1] == '\n') { is_new_ex = 1; is_shortcut = 0; }
                    
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { if (HasRemainingContent(definition, start_p, in_ex, showExamples)) has_more = 1; break; }
                        if (is_f_ex >= 0) { 
                            PrintMini(is_f_ex, (currentLine-1)*8+2, (unsigned char*)buffer, 0); 
                        } else { 
                            RenderDefinitionLine(buffer, is_shcut_draw, currentLine);
                        }
                        currentLine++;
                    }
                    visible_lines++;
                }
            } else {
                int in_f_gram = 0, in_link = 0;
                definition = ctxBuffer; def_len = strlen(definition);
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0;
                    pos = GetNextWrappedLine(definition, pos, 30, buffer, &lineLen);
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { has_more = 1; break; }
                        PrintMini(0, (currentLine-1)*8+2, (unsigned char*)buffer, 0); currentLine++;
                    }
                    visible_lines++;
                }
                if (!has_more) {
                    definition = defBuffer; def_len = strlen(definition); pos = 0;
                    while (definition && pos < def_len) {
                        char buffer[256]; int lineLen = 0, start_p = pos;
                        if (pos == 0 || definition[pos-1] == '\n') {
                            if (definition[pos] == '\x05') { in_f_gram = 1; in_link = 0; pos++; }
                            else if (definition[pos] == '\x08') { in_link = 1; in_f_gram = 0; pos++; }
                            else { in_f_gram = 0; in_link = 0; while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                        }
                        pos = GetNextWrappedLine(definition, pos, 30, buffer, &lineLen);
                        if (in_link) {
                            for (j = 0; j < item_count; j++) {
                                if (item_positions[j] == start_p) { item_lines[j] = visible_lines; break; }
                            }
                        }
                        if (visible_lines >= scrollOffset) {
                            if (currentLine > 8) { if (HasRemainingLinkContent(definition, start_p)) has_more = 1; break; }
                            if (in_f_gram) { 
                                char* p = buffer; int lb;
                                if (*p == '[') p++;
                                lb = strlen(p);
                                if (lb > 0 && p[lb-1] == ']') p[lb-1] = '\0';
                                PrintMini(0, (currentLine-1)*8+2, (unsigned char*)p, 0); 
                            }
                            else if (in_link) {
                                RenderLinkLine(buffer, (highlight_idx < item_count && item_positions[highlight_idx] == start_p), currentLine);
                            }
                            currentLine++;
                        }
                        visible_lines++;
                    }
                }
                if (has_more) { locate(21, 8); Print((unsigned char*)"\xE6\xAD"); }
            }
            } else if (currentMode == MODE_LDOCE_SD) {            if (viewScreen == 0) {
                if (showPron == 3) { 
                    if (ipa_br[0]) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"BrE", 0); currentLine++; PrintIPA(ipa_br, &currentLine); currentLine++; }
                    if (ipa_nam[0] && currentLine <= MAX_LINES) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"NAmE", 0); currentLine++; PrintIPA(ipa_nam, &currentLine); currentLine++; }
                } else {
                    int dBrE = (showPron == 1) || (forceShowMode == 1), dNAmE = (showPron == 2) || (forceShowMode == 2);
                    if (dBrE && ipa_br[0]) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"BrE", 0); currentLine++; PrintIPA(ipa_br, &currentLine); currentLine++; }
                    if (dNAmE && ipa_nam[0] && currentLine <= MAX_LINES) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"NAmE", 0); currentLine++; PrintIPA(ipa_nam, &currentLine); currentLine++; }
                }
                in_ex = 0; is_shortcut = 0; is_new_ex = 0;
                definition = defBuffer; def_len = strlen(definition);
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0, limit = 21, start_p = pos, is_f_ex = -1, is_shcut_draw = 0;
                    if (pos == 0 || definition[pos-1] == '\n') {
                        if (definition[pos] == '\x08' || definition[pos] == '\x05') { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                        if (definition[pos] == '\x06') { is_shortcut = 1; in_ex = 0; is_new_ex = 0; pos++; }
                        else if (definition[pos] == '\x07') { is_shortcut = 0; in_ex = 0; is_new_ex = 0; pos++; }
                        else if (definition[pos] == '\x01') { in_ex = 1; is_shortcut = 0; is_new_ex = 1; pos++; }
                        else if (definition[pos] == '\x04') { in_ex = 0; is_shortcut = 0; is_new_ex = 0; pos++; }
                        else { in_ex = 0; is_shortcut = 0; is_new_ex = 0; }
                    }
                    if (in_ex) { 
                        if (!showExamples) { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; in_ex = 0; is_new_ex = 0; continue; }
                        if (is_new_ex) { is_f_ex = 4; limit = 28; is_new_ex = 0; }
                        else { is_f_ex = 0; limit = 30; }
                    } else if (is_shortcut) { 
                        limit = 19;
                        if (is_shortcut == 1) { is_shcut_draw = 1; is_shortcut = 2; }
                        else { is_shcut_draw = 2; }
                    }
                    
                    pos = GetNextWrappedLine(definition, pos, limit, buffer, &lineLen);
                    if (pos > 0 && definition[pos-1] == '\n') { is_new_ex = 1; is_shortcut = 0; }
                    
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { if (HasRemainingContent(definition, start_p, in_ex, showExamples)) has_more = 1; break; }
                        if (is_f_ex >= 0) { 
                            PrintMini(is_f_ex, (currentLine-1)*8+2, (unsigned char*)buffer, 0); 
                        } else { 
                            RenderDefinitionLine(buffer, is_shcut_draw, currentLine);
                        }
                        currentLine++;
                    }
                    visible_lines++;
                }
            } else {
                int in_f_gram = 0, in_link = 0;
                definition = ctxBuffer; def_len = strlen(definition);
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0;
                    pos = GetNextWrappedLine(definition, pos, 30, buffer, &lineLen);
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { has_more = 1; break; }
                        PrintMini(0, (currentLine-1)*8+2, (unsigned char*)buffer, 0); currentLine++;
                    }
                    visible_lines++;
                }
                if (!has_more) {
                    definition = defBuffer; def_len = strlen(definition); pos = 0;
                    while (definition && pos < def_len) {
                        char buffer[256]; int lineLen = 0, start_p = pos;
                        if (pos == 0 || definition[pos-1] == '\n') {
                            if (definition[pos] == '\x05') { in_f_gram = 1; in_link = 0; pos++; }
                            else if (definition[pos] == '\x08') { in_link = 1; in_f_gram = 0; pos++; }
                            else { in_f_gram = 0; in_link = 0; while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                        }
                        pos = GetNextWrappedLine(definition, pos, 30, buffer, &lineLen);
                        if (in_link) {
                            for (j = 0; j < item_count; j++) {
                                if (item_positions[j] == start_p) { item_lines[j] = visible_lines; break; }
                            }
                        }
                        if (visible_lines >= scrollOffset) {
                            if (currentLine > 8) { if (HasRemainingLinkContent(definition, start_p)) has_more = 1; break; }
                            if (in_f_gram) { 
                                char* p = buffer; int lb;
                                if (*p == '[') p++;
                                lb = strlen(p);
                                if (lb > 0 && p[lb-1] == ']') p[lb-1] = '\0';
                                PrintMini(0, (currentLine-1)*8+2, (unsigned char*)p, 0); 
                            }
                            else if (in_link) {
                                RenderLinkLine(buffer, (highlight_idx < item_count && item_positions[highlight_idx] == start_p), currentLine);
                            }
                            currentLine++;
                        }
                        visible_lines++;
                    }
                }
                if (has_more) { locate(21, 8); Print((unsigned char*)"\xE6\xAD"); }
            }
            }
            if (viewScreen == 0 && g_scrollbar_ticks > 0) {
                int page_l = pos_on_line1 ? 7 : 6;
                if (g_total_def_lines > page_l) {
                    int track_h = 64;
                    int thumb_len = (page_l * track_h) / g_total_def_lines;
                    int max_s = g_total_def_lines - page_l;
                    int thumb_y, y;
                    if (thumb_len < 4) thumb_len = 4;
                    if (thumb_len > 60) thumb_len = 60;
                    if (max_s <= 0) thumb_y = 0;
                    else if (scrollOffset >= max_s) thumb_y = track_h - thumb_len;
                    else thumb_y = (scrollOffset * (track_h - thumb_len)) / max_s;
                    for (y = 0; y < 64; y++) {
                        if (y >= thumb_y && y < thumb_y + thumb_len) {
                            Bdisp_SetPoint_VRAM(127, y, 1);
                        } else {
                            Bdisp_SetPoint_VRAM(127, y, 0);
                        }
                    }
                }
            }
            Bdisp_PutDisp_DD();
            SetTimer(1, GetScrollInterval(), DefViewTimerHandler); GetKey(&key); KillTimer(1);

            if (key == KEY_CTRL_QUIT) return SIGNAL_QUIT_TO_MENU;
            if (key == 30100 || key == 0x7f50) { if (ShowHelpScreen() == SIGNAL_QUIT_TO_MENU) return SIGNAL_QUIT_TO_MENU; continue; }
            if (key == KEY_CTRL_EXIT) return currentIndex;
            if (key == KEY_CTRL_DOWN) {
                if (viewScreen == 1 && item_count > 0) {
                    if (highlight_idx < item_count - 1) {
                        highlight_idx++;
                        if (item_lines[highlight_idx] >= scrollOffset + (pos_on_line1 ? 7 : 6)) {
                            scrollOffset = item_lines[highlight_idx] - (pos_on_line1 ? 7 : 6) + 1;
                        }
                    } else if (has_more) {
                        scrollOffset++;
                    }
                } else if (has_more) {
                    scrollOffset++;
                    if (viewScreen == 0) g_scrollbar_ticks = 5;
                } else if (viewScreen == 0 && g_total_def_lines > (pos_on_line1 ? 7 : 6)) {
                    g_scrollbar_ticks = 5;
                }
            }
            if (key == KEY_CTRL_UP) {
                if (viewScreen == 1 && item_count > 0) {
                    if (highlight_idx > 0) {
                        highlight_idx--;
                        if (item_lines[highlight_idx] < scrollOffset) {
                            scrollOffset = item_lines[highlight_idx];
                        }
                    } else if (scrollOffset > 0) {
                        scrollOffset--;
                    }
                } else if (scrollOffset > 0) {
                    scrollOffset--;
                    if (viewScreen == 0) g_scrollbar_ticks = 5;
                } else if (viewScreen == 0 && g_total_def_lines > (pos_on_line1 ? 7 : 6)) {
                    g_scrollbar_ticks = 5;
                }
            }
            if (key == KEY_CTRL_PAGEDOWN || key == 30053) {
                if (has_more) scrollOffset += 7;
                if (viewScreen == 0 && g_total_def_lines > (pos_on_line1 ? 7 : 6)) g_scrollbar_ticks = 5;
            }
            if (key == KEY_CTRL_PAGEUP || key == 30052) {
                if (scrollOffset > 0) {
                    scrollOffset -= 7;
                    if (scrollOffset < 0) scrollOffset = 0;
                }
                if (viewScreen == 0 && g_total_def_lines > (pos_on_line1 ? 7 : 6)) g_scrollbar_ticks = 5;
            }
            if (key == KEY_CTRL_RIGHT && currentIndex < maxItems - 1) {
                char currentRoot[128], nextRoot[128], tmpWord[128], currentPos[16] = "", nextPos[16] = "";
                GetRootWord(currentRoot, word);
                if (currentMode != MODE_INTERNAL) ReadSDWord(currentIndex, NULL, NULL, currentPos, NULL);
                do {
                    currentIndex++;
                    if (currentIndex >= maxItems) break;
                    if (currentMode == MODE_INTERNAL) strncpy(tmpWord, dictionary[currentIndex].word, 127);
                    else ReadSDWord(currentIndex, tmpWord, NULL, nextPos, NULL);
                    tmpWord[127] = '\0';
                    GetRootWord(nextRoot, tmpWord);
                } while (currentIndex < maxItems - 1 && CaseInsensitiveCompare(currentRoot, nextRoot) == 0 && strcmp(currentPos, nextPos) == 0);
                reset_view = 1; viewScreen = 0; continue;
            }
            if (key == KEY_CTRL_LEFT && currentIndex > 0) {
                char currentRoot[128], prevRoot[128], tmpWord[128], currentPos[16] = "", prevPos[16] = "";
                GetRootWord(currentRoot, word);
                if (currentMode != MODE_INTERNAL) ReadSDWord(currentIndex, NULL, NULL, currentPos, NULL);
                do {
                    currentIndex--;
                    if (currentIndex < 0) break;
                    if (currentMode == MODE_INTERNAL) strncpy(tmpWord, dictionary[currentIndex].word, 127);
                    else ReadSDWord(currentIndex, tmpWord, NULL, prevPos, NULL);
                    tmpWord[127] = '\0';
                    GetRootWord(prevRoot, tmpWord);
                } while (currentIndex > 0 && CaseInsensitiveCompare(currentRoot, prevRoot) == 0 && strcmp(currentPos, prevPos) == 0);
                
                if (currentIndex > 0) {
                    char targetRoot[128], targetPos[16] = "";
                    strcpy(targetRoot, prevRoot);
                    strcpy(targetPos, prevPos);
                    while (currentIndex > 0) {
                        int checkIdx = currentIndex - 1;
                        if (currentMode == MODE_INTERNAL) strncpy(tmpWord, dictionary[checkIdx].word, 127);
                        else ReadSDWord(checkIdx, tmpWord, NULL, prevPos, NULL);
                        tmpWord[127] = '\0';
                        GetRootWord(prevRoot, tmpWord);
                        if (CaseInsensitiveCompare(targetRoot, prevRoot) == 0 && strcmp(targetPos, prevPos) == 0) currentIndex--;
                        else break;
                    }
                }
                reset_view = 1; viewScreen = 0; continue;
            }
            if (key == KEY_CHAR_XTT) {
                if (has_ctx_data) {
                    if (viewScreen == 0) {
                        defScrollOffset = scrollOffset;
                        viewScreen = 1;
                        scrollOffset = linkScrollOffset;
                    } else {
                        linkScrollOffset = scrollOffset;
                        viewScreen = 0;
                        scrollOffset = defScrollOffset;
                    }
                    g_scrollbar_ticks = 0;
                }
                continue;
            }
            if (key == 0x28) { showPron = (showPron == 1) ? 0 : 1; }
            if (key == 0x29) { showPron = (showPron == 2) ? 0 : 2; }

            if (key == 0x2C) { // KEY_CHAR_COMMA for Previous Split
                int prevIdx = currentIndex - 1;
                if (prevIdx >= 0) {
                    char currentWord[128], prevWord[128], prevRoot[128], currentRoot[128];
                    char currentPos[16] = "", prevPos[16] = "";
                    if (currentMode == MODE_INTERNAL) {
                        strncpy(currentWord, dictionary[currentIndex].word, 127);
                        strncpy(prevWord, dictionary[prevIdx].word, 127);
                    } else {
                        ReadSDWord(currentIndex, currentWord, NULL, currentPos, NULL);
                        ReadSDWord(prevIdx, prevWord, NULL, prevPos, NULL);
                    }
                    currentWord[127] = '\0'; prevWord[127] = '\0';
                    GetRootWord(currentRoot, currentWord);
                    GetRootWord(prevRoot, prevWord);
                    if (CaseInsensitiveCompare(currentRoot, prevRoot) == 0 && strcmp(currentPos, prevPos) == 0) {
                        currentIndex = prevIdx; reset_view = 1; viewScreen = 0; continue;
                    }
                }
            }
            if (key == MY_KEY_STORE) { // Store for Next Split
                int nextIdx = currentIndex + 1;
                if (nextIdx < maxItems) {
                    char currentWord[128], nextWord[128], nextRoot[128], currentRoot[128];
                    char currentPos[16] = "", nextPos[16] = "";
                    if (currentMode == MODE_INTERNAL) {
                        strncpy(currentWord, dictionary[currentIndex].word, 127);
                        strncpy(nextWord, dictionary[nextIdx].word, 127);
                    } else {
                        ReadSDWord(currentIndex, currentWord, NULL, currentPos, NULL);
                        ReadSDWord(nextIdx, nextWord, NULL, nextPos, NULL);
                    }
                    currentWord[127] = '\0'; nextWord[127] = '\0';
                    GetRootWord(currentRoot, currentWord);
                    GetRootWord(nextRoot, nextWord);
                    if (CaseInsensitiveCompare(currentRoot, nextRoot) == 0 && strcmp(currentPos, nextPos) == 0) {
                        currentIndex = nextIdx; reset_view = 1; viewScreen = 0; continue;
                    }
                }
            }
            
            if (key == 0x83 || key == 0x0083) { if (viewScreen == 1 && item_count > 0) { highlight_idx++; if (highlight_idx >= item_count) highlight_idx = 0; } }
            if (key == KEY_CTRL_EXE && viewScreen == 1 && highlight_idx < item_count) { int j_idx = BinarySearchSDCard(item_jump_words[highlight_idx], 0, 0); if (j_idx >= 0) { currentIndex = j_idx; reset_view = 1; viewScreen = 0; continue; } }
        }
    }
}

int CaseInsensitiveCompare(const char* s1, const char* s2) { unsigned char c1, c2; while (*s1 && *s2) { c1 = (unsigned char)*s1; c2 = (unsigned char)*s2; if (c1 >= 'a' && c1 <= 'z') c1 -= 32; if (c2 >= 'a' && c2 <= 'z') c2 -= 32; if (c1 != c2) return (int)c1 - (int)c2; s1++; s2++; } c1 = (unsigned char)*s1; c2 = (unsigned char)*s2; if (c1 >= 'a' && c1 <= 'z') c1 -= 32; if (c2 >= 'a' && c2 <= 'z') c2 -= 32; return (int)c1 - (int)c2; }
int CaseInsensitivePrefixCompare(const char* s1, const char* s2, int len) { int i; unsigned char c1, c2; if (len <= 0) return 0; for (i = 0; i < len; i++) { c1 = (unsigned char)s1[i]; c2 = (unsigned char)s2[i]; if (c1 >= 'a' && c1 <= 'z') c1 -= 32; if (c2 >= 'a' && c2 <= 'z') c2 -= 32; if (c1 != c2) return (int)c1 - (int)c2; if (c1 == 0) return 0; } return 0; }
int BinarySearchSDCard(const char* targetWord, int searchPrefix, int* closest) {
    int left = 0, right = sd_dictionarySize - 1, bestMatch = -1;
    char sWord[64];
    int has_dash = 0;
    const char* p = targetWord;
    char cleanTarget[64];
    int cLen = 0, pLen;

    if (idx_handle < 0) {
        if (OpenSDCardFiles() < 0) return -1;
    }
    if (!targetWord) return -1;

    while (*p == ' ' || *p == '-') {
        if (*p == '-') has_dash = 1;
        p++;
    }
    while (*p && *p != '\r' && *p != '\n' && cLen < 63) {
        cleanTarget[cLen++] = *p++;
    }
    while (cLen > 0 && (cleanTarget[cLen - 1] == ' ' || cleanTarget[cLen - 1] == '\t')) cLen--;
    cleanTarget[cLen] = '\0';
    if (cLen == 0) return -1;

    pLen = searchPrefix ? cLen : 0;
    while (left <= right) {
        int mid = left + (right - left) / 2, cmp;
        ReadSDWord(mid, NULL, sWord, NULL, NULL);
        if (searchPrefix) cmp = CaseInsensitivePrefixCompare(sWord, cleanTarget, pLen);
        else cmp = CaseInsensitiveCompare(sWord, cleanTarget);
        if (cmp == 0) {
            if (!searchPrefix) {
                char dWord[64];
                int start_m = mid, end_m = mid, check;
                while (start_m > 0) {
                    ReadSDWord(start_m - 1, NULL, sWord, NULL, NULL);
                    if (CaseInsensitiveCompare(sWord, cleanTarget) != 0) break;
                    start_m--;
                }
                while (end_m < sd_dictionarySize - 1) {
                    ReadSDWord(end_m + 1, NULL, sWord, NULL, NULL);
                    if (CaseInsensitiveCompare(sWord, cleanTarget) != 0) break;
                    end_m++;
                }
                bestMatch = start_m;
                for (check = start_m; check <= end_m; check++) {
                    ReadSDWord(check, dWord, NULL, NULL, NULL);
                    if (has_dash && dWord[0] == '-') {
                        bestMatch = check;
                        break;
                    } else if (!has_dash && dWord[0] != '-') {
                        bestMatch = check;
                        break;
                    }
                }
                break;
            } else {
                bestMatch = mid;
                right = mid - 1;
            }
        } else if (cmp < 0) left = mid + 1;
        else right = mid - 1;
    }
    if (closest) *closest = left;
    return bestMatch;
}
int JumpListToPrefix() { int left = 0, right = (currentMode == MODE_INTERNAL ? dictionarySize : sd_dictionarySize) - 1, bestMatch = -1; char sWord[64] = ""; if (right < 0 || searchPos == 0) return 0; while (left <= right) { int mid = left + (right - left) / 2, cmp; if (currentMode == MODE_INTERNAL) { strncpy(sWord, dictionary[mid].word, 23); sWord[23] = '\0'; } else ReadSDWord(mid, NULL, sWord, NULL, NULL); sWord[isIdiomMode ? 47 : 23] = '\0'; cmp = CaseInsensitivePrefixCompare(sWord, searchBuffer, searchPos); if (cmp == 0) { bestMatch = mid; right = mid - 1; } else if (cmp < 0) left = mid + 1; else right = mid - 1; } if (bestMatch >= 0) return bestMatch; return (left > right) ? right : left; }

int GetDistinctIdentifier_FromIdx(int index, char* outRoot, char* outPos) {
    char word[128];
    if (outPos) outPos[0] = '\0';
    if (isIdiomMode) {
        ReadSDWord(index, word, NULL, NULL, NULL);
        if (outPos) outPos[0] = '\0';
        strcpy(outRoot, word);
        return 1;
    }
    if (currentMode == MODE_INTERNAL) {
        strncpy(word, dictionary[index].word, 127);
        word[127] = '\0';
    } else {
        ReadSDWord(index, word, NULL, outPos, NULL);
    }
    GetRootWord(outRoot, word);
    return 1;
}

int GetFirstSplitWord(int idx) {
    char targetRoot[128], targetPos[16], curRoot[128], curPos[16];
    if (isIdiomMode) return idx;
    GetDistinctIdentifier_FromIdx(idx, targetRoot, targetPos);
    while (idx > 0) {
        GetDistinctIdentifier_FromIdx(idx - 1, curRoot, curPos);
        if (CaseInsensitiveCompare(targetRoot, curRoot) == 0 && strcmp(targetPos, curPos) == 0) idx--;
        else break;
    }
    return idx;
}

int GetNextDistinctWord(int idx) {
    int maxItems = (currentMode == MODE_INTERNAL) ? dictionarySize : sd_dictionarySize;
    char targetRoot[128], targetPos[16], curRoot[128], curPos[16];
    if (isIdiomMode) {
        if (idx < maxItems - 1) return idx + 1;
        return -1;
    }
    GetDistinctIdentifier_FromIdx(idx, targetRoot, targetPos);
    while (idx < maxItems - 1) {
        idx++;
        GetDistinctIdentifier_FromIdx(idx, curRoot, curPos);
        if (CaseInsensitiveCompare(targetRoot, curRoot) != 0 || strcmp(targetPos, curPos) != 0) return idx;
    }
    return -1;
}

int GetPrevDistinctWord(int idx) {
    char targetRoot[128], targetPos[16], curRoot[128], curPos[16];
    if (isIdiomMode) {
        if (idx > 0) return idx - 1;
        return 0;
    }
    GetDistinctIdentifier_FromIdx(idx, targetRoot, targetPos);
    while (idx > 0) {
        idx--;
        GetDistinctIdentifier_FromIdx(idx, curRoot, curPos);
        if (CaseInsensitiveCompare(targetRoot, curRoot) != 0 || strcmp(targetPos, curPos) != 0) return GetFirstSplitWord(idx);
    }
    return 0;
}

void EnsureVisible(int* topIdx, int curIdx) {
    if (isIdiomMode) {
        if (curIdx < *topIdx) *topIdx = curIdx;
        else if (curIdx >= *topIdx + 7) *topIdx = curIdx - 6;
        return;
    }
    if (curIdx < *topIdx) {
        *topIdx = curIdx;
    } else {
        int lines_needed = 0;
        int maxItems = (currentMode == MODE_INTERNAL) ? dictionarySize : sd_dictionarySize;
        int scan = *topIdx;
        while (scan != -1 && scan < curIdx) {
            scan = GetNextDistinctWord(scan);
            lines_needed++;
        }
        if (lines_needed >= 6) {
            int back_lines = 5;
            scan = curIdx;
            while (back_lines > 0 && scan > 0) {
                int p = GetPrevDistinctWord(scan);
                if (p == scan) break;
                scan = p;
                back_lines--;
            }
            *topIdx = scan;
        }
    }
}

void DisplayList(int topIndex, int currentIndex) {
    int i, maxItems = (currentMode == MODE_INTERNAL) ? dictionarySize : sd_dictionarySize; 
    int listIdx, first_cur;
    char *s;
    Bdisp_AllClr_VRAM(); if (maxItems <= 0) { locate(1, 4); Print((unsigned char*)"No Data Found!"); return; }
    g_hl_word[0] = '\0';
    
    if (currentMode == MODE_OALD_SD) {
        if (cache_base == -1 || currentIndex < cache_base || currentIndex >= cache_base + CACHE_SIZE) {
            cache_base = currentIndex - (CACHE_SIZE / 2); if (cache_base < 0) cache_base = 0; if (cache_base > maxItems - CACHE_SIZE) cache_base = maxItems - CACHE_SIZE;
            for (i = 0; i < CACHE_SIZE && (cache_base + i) < maxItems; i++) { ReadSDWord(cache_base + i, list_cache[i].disp, NULL, list_cache[i].spos, NULL); list_cache[i].index = cache_base + i; }
        }
    } else if (currentMode == MODE_LDOCE_SD) {
        if (cache_base == -1 || currentIndex < cache_base || currentIndex >= cache_base + CACHE_SIZE) {
            cache_base = currentIndex - (CACHE_SIZE / 2); if (cache_base < 0) cache_base = 0; if (cache_base > maxItems - CACHE_SIZE) cache_base = maxItems - CACHE_SIZE;
            for (i = 0; i < CACHE_SIZE && (cache_base + i) < maxItems; i++) { ReadSDWord(cache_base + i, list_cache[i].disp, NULL, list_cache[i].spos, NULL); list_cache[i].index = cache_base + i; }
        }
    }
    
    listIdx = topIndex;
    first_cur = GetFirstSplitWord(currentIndex);
    for (i = 0; i < MAX_LINES - 1 && listIdx < maxItems && listIdx >= 0; i++) {
        char dWord[128], short_pos[12], dispWord[64]; int limit, pos_w = 0;
        int current_disp_mode = currentMode;
        char *ps;
        
        if (current_disp_mode == MODE_INTERNAL) { 
            strncpy(dWord, dictionary[listIdx].word, 127); dWord[127]='\0'; short_pos[0] = '\0'; 
        } else if (current_disp_mode == MODE_OALD_SD) { 
            int ci = listIdx - cache_base; if (ci >= 0 && ci < CACHE_SIZE) { strcpy(dWord, list_cache[ci].disp); strcpy(short_pos, list_cache[ci].spos); } else ReadSDWord(listIdx, dWord, NULL, short_pos, NULL); 
        } else if (current_disp_mode == MODE_LDOCE_SD) {
            int ci = listIdx - cache_base; if (ci >= 0 && ci < CACHE_SIZE) { strcpy(dWord, list_cache[ci].disp); strcpy(short_pos, list_cache[ci].spos); } else ReadSDWord(listIdx, dWord, NULL, short_pos, NULL); 
        }
        
        StripSyllableDots(dWord);
        if (isIdiomMode) short_pos[0] = '\0';
        if (short_pos[0]) {
            ps = short_pos;
            if (*ps == '\x02' || *ps == '\x03') ps++;
            pos_w = strlen(ps) * 4;
            limit = (127 - pos_w - 4) / 6;
            if (limit > 21) limit = 21;
            if (limit < 1) limit = 1;
        } else {
            limit = 21;
        }
        locate(1, i + 2);
        
        if (listIdx == first_cur) {
            g_hl_limit = limit;
            if (isIdiomMode) {
                strncpy(g_hl_word, dWord, 127); g_hl_word[127] = '\0';
            } else if (current_disp_mode == MODE_INTERNAL) { 
                strncpy(g_hl_word, dWord, 127); g_hl_word[127] = '\0'; 
            } else if (current_disp_mode == MODE_OALD_SD) { 
                unsigned int off; char c; int j = 0; ReadSDWord(listIdx, NULL, NULL, NULL, &off); while (SafeSDRead(1, &c, 1, off + j) == 1 && c != '\0' && j < 127) g_hl_word[j++] = c; g_hl_word[j] = '\0'; StripSyllableDots(g_hl_word); 
            } else if (current_disp_mode == MODE_LDOCE_SD) { 
                unsigned int off; char c; int j = 0; ReadSDWord(listIdx, NULL, NULL, NULL, &off); while (SafeSDRead(1, &c, 1, off + j) == 1 && c != '\0' && j < 127) g_hl_word[j++] = c; g_hl_word[j] = '\0'; StripSyllableDots(g_hl_word); 
            }
            
            CasioSubStr(g_hl_word, GetCurrentScrollOffset(g_hl_word, g_hl_limit, g_timer_tick), g_hl_limit, dispWord);
            {
                char *m = strchr(dispWord, '\x09');
                int px = 0;
                if (m) {
                    *m = '\0'; PrintRev((unsigned char*)dispWord);
                    px = CasioStrLen(dispWord) * 6;
                    PrintMini(px, (i + 1) * 8 + 2, (unsigned char*)(m + 1), 1);
                    px += strlen(m + 1) * 4 + 4;
                } else {
                    PrintRev((unsigned char*)dispWord);
                    px = CasioStrLen(dispWord) * 6 + 4;
                }
                if (short_pos[0]) { 
                    s = short_pos;
                    if (*s == '\x02' || *s == '\x03') s++;
                    PrintMini(px, (i + 1) * 8 + 2, (unsigned char*)s, 1);
                    strcpy(g_hl_pos, short_pos); 
                } else g_hl_pos[0] = '\0'; g_hl_line = i + 2;
            }
        } else { 
            CasioSubStr(dWord, 0, limit, dispWord);
            {
                char *m = strchr(dispWord, '\x09');
                int px = 0;
                if (m) {
                    *m = '\0'; Print((unsigned char*)dispWord);
                    px = CasioStrLen(dispWord) * 6;
                    PrintMini(px, (i + 1) * 8 + 2, (unsigned char*)(m + 1), 0);
                    px += strlen(m + 1) * 4 + 4;
                } else {
                    Print((unsigned char*)dispWord);
                    px = CasioStrLen(dispWord) * 6 + 4;
                }
                if (short_pos[0]) { 
                    s = short_pos;
                    if (*s == '\x02' || *s == '\x03') s++;
                    PrintMini(px, (i + 1) * 8 + 2, (unsigned char*)s, 0);
                }
            }
        }
        
        listIdx = GetNextDistinctWord(listIdx);
        if (listIdx == -1) break;
    }
}

static int IsVowelChar(unsigned char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    return (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y');
}

int LevDist(const char *s, int ls, const char *t, int lt) {
    int d[33][33]; int i, j, sub_cost, del_cost, ins_cost, min_c; 
    unsigned char c1, c2, p1, p2, prev_c1 = 0, prev_c2 = 0;
    if (ls > 32) ls = 32; if (lt > 32) lt = 32;
    if (ls == 0) return lt * 10; if (lt == 0) return ls * 10;
    for (i = 0; i <= ls; i++) d[i][0] = i * 10; 
    for (j = 0; j <= lt; j++) d[0][j] = j * 10;
    for (i = 1; i <= ls; i++) { 
        c1 = (unsigned char)s[i-1]; if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        prev_c1 = (i > 1) ? (unsigned char)s[i-2] : 0;
        if (prev_c1 >= 'a' && prev_c1 <= 'z') prev_c1 -= 32;
        for (j = 1; j <= lt; j++) { 
            c2 = (unsigned char)t[j-1]; if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
            prev_c2 = (j > 1) ? (unsigned char)t[j-2] : 0;
            if (prev_c2 >= 'a' && prev_c2 <= 'z') prev_c2 -= 32;

            if (c1 == c2) {
                sub_cost = 0;
            } else if (IsVowelChar(c1) && IsVowelChar(c2)) {
                sub_cost = 4; /* Discount for vowel swaps (e.g. definately -> definitely) */
            } else {
                sub_cost = 10; /* Standard consonant substitution */
            }

            /* Deletion cost: discount if deleting duplicate letter (e.g. double consonant slip) */
            del_cost = (prev_c1 == c1) ? 4 : 10;

            /* Insertion cost: discount if inserting duplicate letter (e.g. missing second consonant) */
            ins_cost = (prev_c2 == c2) ? 4 : 10;

            min_c = d[i-1][j] + del_cost;
            if (d[i][j-1] + ins_cost < min_c) min_c = d[i][j-1] + ins_cost; 
            if (d[i-1][j-1] + sub_cost < min_c) min_c = d[i-1][j-1] + sub_cost; 
            d[i][j] = min_c;

            /* Adjacent Transposition (Damerau) */
            if (i > 1 && j > 1) {
                p1 = prev_c1;
                p2 = prev_c2;
                if (c1 == p2 && p1 == c2) {
                    if (d[i-2][j-2] + 8 < d[i][j]) d[i][j] = d[i-2][j-2] + 8;
                }
            }
        } 
    }
    return d[ls][lt];
}

void PopupScrollTimer(void) { 
    g_timer_tick++; 
    if (CasioStrLen(g_hl_word) > 16) {
        char dp[64], line[80]; int mo = CasioStrLen(g_hl_word) - 16, off = g_timer_tick % (mo + 4); if (off > mo) off = 0;
        line[0] = '1' + g_hl_line; line[1] = ':'; line[2] = ' '; line[3] = '\0';
        CasioSubStr(g_hl_word, off, 16, dp); strcat(line, dp);
        locate(2, 3 + g_hl_line); PrintRev((unsigned char*)line); Bdisp_PutDisp_DD();
    }
}

typedef struct { int cIdx; char base[80]; char root[80]; int dist; int score; } Score;

static void AddSpellCandidate(int cIdx, const char* searchBuf, int s_len, int maxItems, Score* scores, int* num_scores) {
    char rootWord[80], base[80], posTag[16] = "";
    int d_i, r_len, len_diff, first_diff;
    if (cIdx < 0 || cIdx >= maxItems || *num_scores >= 46) return;
    for (d_i = 0; d_i < *num_scores; d_i++) {
        if (scores[d_i].cIdx == cIdx) return;
    }
    GetDistinctIdentifier_FromIdx(cIdx, rootWord, posTag);
    if (isIdiomMode) posTag[0] = '\0';
    strcpy(base, rootWord);
    if (!isIdiomMode && posTag[0]) {
        char cleanPos[16];
        strcpy(cleanPos, posTag);
        if (cleanPos[0] == '\x02' || cleanPos[0] == '\x03') memmove(cleanPos, cleanPos + 1, strlen(cleanPos));
        strcat(base, " ");
        strcat(base, cleanPos);
    }
    scores[*num_scores].cIdx = cIdx;
    strcpy(scores[*num_scores].base, base);
    strcpy(scores[*num_scores].root, rootWord);

    if (isIdiomMode) {
        const char *sub = strstr(rootWord, searchBuf);
        if (sub) {
            int pfx_len = (int)(sub - rootWord);
            scores[*num_scores].dist = pfx_len * 2;
            scores[*num_scores].score = pfx_len * 5;
            (*num_scores)++;
            return;
        }
    }

    scores[*num_scores].dist = LevDist(searchBuf, s_len, rootWord, CasioStrLen(rootWord));
    r_len = CasioStrLen(rootWord);
    len_diff = (r_len > s_len) ? (r_len - s_len) : (s_len - r_len);
    first_diff = (r_len > 0 && s_len > 0 && 
                  ((rootWord[0] >= 'a' ? rootWord[0] - 32 : rootWord[0]) == 
                   (searchBuf[0] >= 'a' ? searchBuf[0] - 32 : searchBuf[0]))) ? 0 : 1;
    scores[*num_scores].score = scores[*num_scores].dist * 10 + len_diff * 8 + first_diff * 15;
    (*num_scores)++;
}

int SpellCheckPopup(int closestIdx) {
    int maxItems = (currentMode == MODE_INTERNAL) ? dictionarySize : sd_dictionarySize; 
    DISPBOX box; 
    Score scores[48];
    int num_scores = 0, s_len, i, iter, k, p_idx;
    char baseWords[5][80], rootWords[5][80], perm[32], ch, tmp;
    unsigned int key; int j, count = 0, suggestionIndices[5];
    
    if (closestIdx < 0) closestIdx = 0; 
    if (closestIdx >= maxItems) closestIdx = maxItems - 1;
    s_len = strlen(searchBuffer);
    
    /* 1. Sample +/- 20 words around closest position in alphabet */
    for (i = -20; i <= 20; i++) {
        AddSpellCandidate(closestIdx + i, searchBuffer, s_len, maxItems, scores, &num_scores);
    }
    
    /* 2. Check adjacent letter transpositions across the entire word (e.g. DAED -> DEAD) */
    for (i = 0; i < s_len - 1; i++) {
        strcpy(perm, searchBuffer);
        tmp = perm[i];
        perm[i] = perm[i+1];
        perm[i+1] = tmp;
        p_idx = BinarySearchSDCard(perm, 0, NULL);
        if (p_idx < 0) p_idx = BinarySearchSDCard(perm, 1, NULL);
        if (p_idx >= 0) {
            for (k = -2; k <= 3; k++) {
                AddSpellCandidate(p_idx + k, searchBuffer, s_len, maxItems, scores, &num_scores);
            }
        }
    }
    
    /* 3. Check single-letter deletions across the entire word */
    for (i = 0; i < s_len; i++) {
        strcpy(perm, searchBuffer);
        memmove(perm + i, perm + i + 1, strlen(perm + i + 1) + 1);
        if (perm[0] == '\0') continue;
        p_idx = BinarySearchSDCard(perm, 0, NULL);
        if (p_idx < 0) p_idx = BinarySearchSDCard(perm, 1, NULL);
        if (p_idx >= 0) {
            for (k = -2; k <= 3; k++) {
                AddSpellCandidate(p_idx + k, searchBuffer, s_len, maxItems, scores, &num_scores);
            }
        }
    }
    
    /* 4. Multi-Prefix Binary Search for Idioms (A, AN, THE, TO, BE, IN, ON, ALL) */
    if (isIdiomMode) {
        static const char* const s_idmPrefixes[] = {
            "A ", "AN ", "THE ", "TO ", "BE ", "IN ", "ON ", "ALL "
        };
        int pfx_i;
        for (pfx_i = 0; pfx_i < 8; pfx_i++) {
            char pfxQuery[64];
            int pClosest = -1;
            strcpy(pfxQuery, s_idmPrefixes[pfx_i]);
            strncat(pfxQuery, searchBuffer, 63 - strlen(pfxQuery));
            p_idx = BinarySearchSDCard(pfxQuery, 0, &pClosest);
            if (p_idx < 0) p_idx = BinarySearchSDCard(pfxQuery, 1, &pClosest);
            if (p_idx >= 0) {
                for (k = -2; k <= 3; k++) {
                    AddSpellCandidate(p_idx + k, searchBuffer, s_len, maxItems, scores, &num_scores);
                }
            } else if (pClosest >= 0) {
                for (k = -1; k <= 2; k++) {
                    AddSpellCandidate(pClosest + k, searchBuffer, s_len, maxItems, scores, &num_scores);
                }
            }
        }
        /* If query already starts with an article/stopword, also search with it stripped */
        if (CaseInsensitivePrefixCompare(searchBuffer, "A ", 2) == 0 ||
            CaseInsensitivePrefixCompare(searchBuffer, "TO ", 3) == 0 ||
            CaseInsensitivePrefixCompare(searchBuffer, "BE ", 3) == 0 ||
            CaseInsensitivePrefixCompare(searchBuffer, "IN ", 3) == 0 ||
            CaseInsensitivePrefixCompare(searchBuffer, "ON ", 3) == 0) {
            const char* stripped = strchr(searchBuffer, ' ');
            if (stripped && *(stripped + 1)) {
                int pClosest = -1;
                stripped++;
                p_idx = BinarySearchSDCard(stripped, 0, &pClosest);
                if (p_idx < 0) p_idx = BinarySearchSDCard(stripped, 1, &pClosest);
                if (p_idx >= 0) {
                    for (k = -2; k <= 3; k++) {
                        AddSpellCandidate(p_idx + k, searchBuffer, s_len, maxItems, scores, &num_scores);
                    }
                }
            }
        } else if (CaseInsensitivePrefixCompare(searchBuffer, "THE ", 4) == 0 ||
                   CaseInsensitivePrefixCompare(searchBuffer, "ALL ", 4) == 0) {
            const char* stripped = searchBuffer + 4;
            if (*stripped) {
                int pClosest = -1;
                p_idx = BinarySearchSDCard(stripped, 0, &pClosest);
                if (p_idx < 0) p_idx = BinarySearchSDCard(stripped, 1, &pClosest);
                if (p_idx >= 0) {
                    for (k = -2; k <= 3; k++) {
                        AddSpellCandidate(p_idx + k, searchBuffer, s_len, maxItems, scores, &num_scores);
                    }
                }
            }
        }
    } else {
        /* 5. Check substitutions and insertions for up to first 3 characters (only in dictionary mode) */
        for (i = 0; i <= s_len && i < 3; i++) {
            for (iter = 0; iter < 52; iter++) {
                strcpy(perm, searchBuffer);
                if (iter < 26) {
                    if (i < s_len) {
                        ch = 'A' + iter;
                        if (perm[i] == ch || perm[i] == 'a' + iter) continue;
                        perm[i] = ch;
                    } else continue;
                } else {
                    if (s_len < 31) {
                        memmove(perm + i + 1, perm + i, strlen(perm + i) + 1);
                        perm[i] = 'A' + (iter - 26);
                    } else continue;
                }
                if (perm[0] == '\0') continue;
                p_idx = BinarySearchSDCard(perm, 0, NULL);
                if (p_idx < 0) p_idx = BinarySearchSDCard(perm, 1, NULL);
                if (p_idx >= 0) {
                    for (k = -1; k <= 2; k++) {
                        AddSpellCandidate(p_idx + k, searchBuffer, s_len, maxItems, scores, &num_scores);
                    }
                }
            }
        }
    }
    
    /* 5. Sort candidates by score (best match first) */
    for (i = 0; i < num_scores - 1; i++) {
        for (j = i + 1; j < num_scores; j++) {
            if (scores[j].score < scores[i].score || 
               (scores[j].score == scores[i].score && scores[j].dist < scores[i].dist)) {
                Score t = scores[i]; scores[i] = scores[j]; scores[j] = t;
            }
        }
    }
    
    /* 6. Extract top 5 unique root words */
    for (i = 0; i < num_scores && count < 5; i++) {
        int uniq = 1;
        for (j = 0; j < count; j++) {
            if (CaseInsensitiveCompare(rootWords[j], scores[i].root) == 0) {
                uniq = 0; break;
            }
        }
        if (uniq) {
            suggestionIndices[count] = scores[i].cIdx;
            strcpy(baseWords[count], scores[i].base);
            strcpy(rootWords[count], scores[i].root);
            count++;
        }
    }
    
    if (count == 0) return -1;
    g_hl_line = 0; g_timer_tick = 0; strcpy(g_hl_word, baseWords[0]); KillTimer(1);
    while(1) { 
        box.left = 2; box.top = 8; box.right = 125; box.bottom = 60; 
        Bdisp_AreaClr_VRAM(&box); 
        Bdisp_DrawLineVRAM(2, 8, 125, 8); 
        Bdisp_DrawLineVRAM(2, 60, 125, 60); 
        Bdisp_DrawLineVRAM(2, 8, 2, 60); 
        Bdisp_DrawLineVRAM(125, 8, 125, 60); 
        PrintMini(6, 10, (unsigned char*)"Did you mean...?", 0);
        for (i = 0; i < count; i++) { 
            char line[80], dp[64]; int limit = 16, off = 0; 
            if (isIdiomMode) {
                char *p_idm = strstr(baseWords[i], "idm.");
                if (!p_idm) p_idm = strstr(baseWords[i], "idm");
                if (p_idm && (p_idm == baseWords[i] || *(p_idm - 1) == ' ')) {
                    while (p_idm > baseWords[i] && *(p_idm - 1) == ' ') p_idm--;
                    *p_idm = '\0';
                }
            }
            if (i == g_hl_line) { 
                int wl = CasioStrLen(baseWords[i]); 
                if (wl > limit) { 
                    int mo = wl - limit; 
                    off = g_timer_tick % (mo + 4); 
                    if (off > mo) off = 0; 
                } 
            } 
            line[0] = '1' + i; line[1] = ':'; line[2] = ' '; line[3] = '\0'; 
            CasioSubStr(baseWords[i], off, limit, dp); strcat(line, dp);
            locate(2, 3 + i); 
            if (i == g_hl_line) PrintRev((unsigned char*)line); 
            else Print((unsigned char*)line); 
        }
        Bdisp_PutDisp_DD(); 
        SetTimer(2, GetScrollInterval(), PopupScrollTimer); 
        GetKey(&key); 
        KillTimer(2); 
        if (key == KEY_CTRL_UP) { 
            g_hl_line = (g_hl_line > 0) ? g_hl_line - 1 : count - 1; 
            g_timer_tick = 0; 
            strcpy(g_hl_word, baseWords[g_hl_line]); 
        } else if (key == KEY_CTRL_DOWN) { 
            g_hl_line = (g_hl_line < count - 1) ? g_hl_line + 1 : 0; 
            g_timer_tick = 0; 
            strcpy(g_hl_word, baseWords[g_hl_line]); 
        } else if (key == KEY_CTRL_EXE) return suggestionIndices[g_hl_line]; 
        else if (key == KEY_CTRL_QUIT) return SIGNAL_QUIT_TO_MENU; 
        if (key == 30100 || key == 0x7f50) { 
            if (ShowHelpScreen() == SIGNAL_QUIT_TO_MENU) return SIGNAL_QUIT_TO_MENU; 
            continue; 
        } 
        if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC) return -1; 
        else if (key >= KEY_CHAR_1 && key < KEY_CHAR_1 + count) return suggestionIndices[key - KEY_CHAR_1];
    }
}

int HandleSearch(int currentIndex) {
    int closest = 0;
    if (searchPos == 0) {
        if (isIdiomMode) {
            ReadSDWord(currentIndex, searchBuffer, NULL, NULL, NULL);
        } else if (currentMode == MODE_INTERNAL) {
            strncpy(searchBuffer, dictionary[currentIndex].word, SEARCH_LEN);
        } else {
            ReadSDWord(currentIndex, searchBuffer, NULL, NULL, NULL);
        }
        searchBuffer[SEARCH_LEN] = '\0';
        searchPos = strlen(searchBuffer);
        cursorIndex = searchPos;
    }
    if (currentMode == MODE_INTERNAL) {
        int i;
        for (i = 0; i < dictionarySize; i++) {
            if (CaseInsensitiveCompare(dictionary[i].word, searchBuffer) == 0) return ShowDefinitionViewer(i);
        }
    } else {
        int foundIdx = -1;
        if (isIdiomMode) {
            foundIdx = BinarySearchSDCard(searchBuffer, 0, &closest);
            if (foundIdx < 0) foundIdx = BinarySearchSDCard(searchBuffer, 1, &closest);
            if (foundIdx < 0) foundIdx = SpellCheckPopup(closest);
        } else {
            foundIdx = BinarySearchSDCard(searchBuffer, 0, &closest);
            if (foundIdx < 0) foundIdx = BinarySearchSDCard(searchBuffer, 1, &closest);
            if (foundIdx < 0) foundIdx = SpellCheckPopup(closest);
        }
        if (foundIdx == SIGNAL_QUIT_TO_MENU) return SIGNAL_QUIT_TO_MENU;
        if (foundIdx >= 0) {
            if (isIdiomMode) {
                char dispWord[80];
                unsigned int parentIdx = 0;
                ReadSDWord(foundIdx, dispWord, NULL, NULL, &parentIdx);
                strncpy(g_jump_idm_text, dispWord, 79);
                g_jump_idm_text[79] = '\0';
                SwitchListMode(0);
                if (ShowDefinitionViewer(parentIdx) == SIGNAL_QUIT_TO_MENU) return SIGNAL_QUIT_TO_MENU;
                SwitchListMode(1);
                return foundIdx;
            } else {
                return ShowDefinitionViewer(foundIdx);
            }
        }
    }
    return currentIndex;
}

void DrawSprite(int x, int y, int w, int h, const unsigned char* data) {
    int i, j;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            if (x + i >= 0 && x + i < 128 && y + j >= 0 && y + j < 64) {
                int bit = (data[(j * w + i) / 8] >> (7 - ((j * w + i) % 8))) & 1;
                Bdisp_SetPoint_VRAM(x + i, y + j, bit);
            }
        }
    }
}

int MainMenu() {
    unsigned int key;
    int sel = 0, prev_sel;
    int redraw_all = 1;
    int left, right;
    int p_left, p_right, n_left, n_right;

    while (1) {
        if (redraw_all) {
            Bdisp_AllClr_VRAM();
            DrawSprite(0, 0, 128, 64, ui_bg_library);
            DrawSprite(11, 14, 29, 37, ui_icon_1000);
            DrawSprite(50, 14, 29, 37, ui_icon_oald);
            DrawSprite(89, 14, 29, 37, ui_icon_ldoce);
            
            if (sel == 0)      { left = 11; right = 11 + 28; }
            else if (sel == 1) { left = 50; right = 50 + 28; }
            else               { left = 89; right = 89 + 28; }
            
            Bdisp_AreaReverseVRAM(left, 14, right, 14 + 36);
            PrintMini(2, 58, (unsigned char*)"\x7F\x50:Help", 0);
            Bdisp_PutDisp_DD();
            redraw_all = 0;
        }

        GetKey(&key);

        if (key == KEY_CTRL_QUIT) { redraw_all = 1; continue; }
        if (key == KEY_CTRL_SETUP) {
            SetupMenu();
            redraw_all = 1;
            continue;
        }
        if (key == 30100 || key == 0x7f50) {
            ShowHelpScreen();
            redraw_all = 1;
            continue;
        }

        if (key == KEY_CHAR_1 || key == '1' || key == 0x31) return 0;
        if (key == KEY_CHAR_2 || key == '2' || key == 0x32) return 1;
        if (key == KEY_CHAR_3 || key == '3' || key == 0x33) return 2;
        if (key == KEY_CTRL_EXE) return sel;
        if (key == KEY_CTRL_EXIT) return -1;

        prev_sel = sel;
        if (key == KEY_CTRL_LEFT) {
            if (sel > 0) sel--; else sel = 2;
        } else if (key == KEY_CTRL_RIGHT) {
            if (sel < 2) sel++; else sel = 0;
        }

        if (sel != prev_sel) {
            if (prev_sel == 0)      { p_left = 11; p_right = 11 + 28; }
            else if (prev_sel == 1) { p_left = 50; p_right = 50 + 28; }
            else                    { p_left = 89; p_right = 89 + 28; }

            if (sel == 0)           { n_left = 11; n_right = 11 + 28; }
            else if (sel == 1)      { n_left = 50; n_right = 50 + 28; }
            else                    { n_left = 89; n_right = 89 + 28; }

            Bdisp_AreaReverseVRAM(p_left, 14, p_right, 14 + 36);
            Bdisp_AreaReverseVRAM(n_left, 14, n_right, 14 + 36);
            Bdisp_PutDisp_DD();
        }
    }
}
int GoToLibraryMenu(int* p_maxItems, int* p_topIndex, int* p_currentIndex) {
    unsigned int temp_key;
    int choice = MainMenu();
    if (choice == -1) return -1;
    isIdiomMode = 0;
    g_jump_idm_text[0] = '\0';
    if (choice == 0) currentMode = MODE_INTERNAL;
    else if (choice == 1) currentMode = MODE_OALD_SD;
    else currentMode = MODE_LDOCE_SD;
    CloseSDCardFiles();
    if (currentMode != MODE_INTERNAL && OpenSDCardFiles() < 0) {
        PopUpWin(1); locate(2, 1); Print((unsigned char*)"SD Error!"); Bdisp_PutDisp_DD(); GetKey(&temp_key);
        return -1;
    }
    *p_maxItems = (currentMode == MODE_INTERNAL) ? dictionarySize : sd_dictionarySize;
    searchPos = 0;
    cursorIndex = 0;
    searchBuffer[0] = 0;
    *p_topIndex = 0;
    *p_currentIndex = 0;
    return 0;
}

int AddIn_main(int isAppli, unsigned short OptionNum) {
    unsigned int key; int topIndex = 0, currentIndex = 0, maxItems; 
    SetQuitHandler((void*)CloseSDCardFiles); 
    CloseSDCardFiles(); 
    if (GoToLibraryMenu(&maxItems, &topIndex, &currentIndex) == -1) return 1;
    sw_alpha_lock = 1;
    while (1) {
        DisplayList(topIndex, currentIndex); UpdateSearchBar(1); Bdisp_PutDisp_DD(); SetTimer(1, 50, ListViewTimerHandler); GetKey(&key); KillTimer(1);
        if (key == 30007 || key == 30022 || key == KEY_CTRL_ALPHA) { 
            sw_alpha_lock = 1 - sw_alpha_lock; 
            continue; 
        } 
        key = TranslateAlphaKey(key); 
        if (key == 30008) { isScrollingPaused = 1 - isScrollingPaused; continue; } 
        if (key == 30033) { isInsertMode = 1 - isInsertMode; continue; } 
        if (key == KEY_CTRL_LEFT) { if (cursorIndex > 0) cursorIndex--; continue; } 
        if (key == KEY_CTRL_RIGHT) { if (cursorIndex < searchPos) cursorIndex++; continue; } 
        g_timer_tick = 0; isScrollingPaused = 0;
        if (key == KEY_CTRL_UP) { if (currentIndex > 0) { currentIndex = GetPrevDistinctWord(currentIndex); EnsureVisible(&topIndex, currentIndex); } } 
        else if (key == KEY_CTRL_DOWN) { int idx = GetNextDistinctWord(currentIndex); if (idx != -1 && idx < maxItems) { currentIndex = idx; EnsureVisible(&topIndex, currentIndex); } }
        else if (key == KEY_CTRL_PAGEUP || key == 30052) {
            int cnt = 6;
            while (cnt-- > 0 && currentIndex > 0) currentIndex = GetPrevDistinctWord(currentIndex);
            EnsureVisible(&topIndex, currentIndex);
        }
        else if (key == KEY_CTRL_PAGEDOWN || key == 30053) {
            int cnt = 6;
            while (cnt-- > 0) {
                int idx = GetNextDistinctWord(currentIndex);
                if (idx != -1 && idx < maxItems) currentIndex = idx;
                else break;
            }
            EnsureVisible(&topIndex, currentIndex);
        }
        else if (key == 30004) { 
            if (maxItems > 0) { 
                if (isIdiomMode) {
                    char dispWord[80];
                    unsigned int parentIdx = 0;
                    ReadSDWord(currentIndex, dispWord, NULL, NULL, &parentIdx);
                    strncpy(g_jump_idm_text, dispWord, 79); g_jump_idm_text[79] = '\0';
                    SwitchListMode(0);
                    if (ShowDefinitionViewer(parentIdx) == SIGNAL_QUIT_TO_MENU) {
                        if (GoToLibraryMenu(&maxItems, &topIndex, &currentIndex) == -1) break;
                        continue;
                    }
                    SwitchListMode(1);
                    maxItems = sd_dictionarySize;
                    EnsureVisible(&topIndex, currentIndex);
                } else {
                    currentIndex = ShowDefinitionViewer(currentIndex); 
                    if (currentIndex == SIGNAL_QUIT_TO_MENU) {
                        if (GoToLibraryMenu(&maxItems, &topIndex, &currentIndex) == -1) break;
                        continue;
                    }
                    EnsureVisible(&topIndex, currentIndex); 
                }
            } 
        }
        else if (key == 30025) { 
            if (isInsertMode) {
                if (cursorIndex > 0) {
                    int j;
                    for (j = cursorIndex - 1; j < searchPos; j++) searchBuffer[j] = searchBuffer[j + 1];
                    searchPos--;
                    cursorIndex--;
                    searchBuffer[searchPos] = '\0';
                    currentIndex = GetFirstSplitWord(JumpListToPrefix());
                    topIndex = currentIndex;
                }
            } else {
                if (cursorIndex < searchPos) {
                    int j;
                    for (j = cursorIndex; j < searchPos; j++) searchBuffer[j] = searchBuffer[j + 1];
                    searchPos--;
                    searchBuffer[searchPos] = '\0';
                    currentIndex = GetFirstSplitWord(JumpListToPrefix());
                    topIndex = currentIndex;
                } else if (cursorIndex > 0) {
                    int j;
                    for (j = cursorIndex - 1; j < searchPos; j++) searchBuffer[j] = searchBuffer[j + 1];
                    searchPos--;
                    cursorIndex--;
                    searchBuffer[searchPos] = '\0';
                    currentIndex = GetFirstSplitWord(JumpListToPrefix());
                    topIndex = currentIndex;
                }
            }
        }
        else if (key == 30015) { searchPos = 0; cursorIndex = 0; searchBuffer[0] = 0; }
        if (key == KEY_CTRL_QUIT || key == 30002) { 
            if (GoToLibraryMenu(&maxItems, &topIndex, &currentIndex) == -1) break; 
            continue; 
        }
        else if (key == 30009) { 
            currentIndex = HandleSearch(currentIndex); 
            if (currentIndex == SIGNAL_QUIT_TO_MENU) {
                if (GoToLibraryMenu(&maxItems, &topIndex, &currentIndex) == -1) break;
                continue;
            }
            topIndex = currentIndex; 
            EnsureVisible(&topIndex, currentIndex); 
        }
        else if (key == KEY_CTRL_F2 || key == 30010) {
            if (currentMode != MODE_INTERNAL) {
                isIdiomMode = 1 - isIdiomMode;
                SwitchListMode(isIdiomMode);
                maxItems = sd_dictionarySize;
                topIndex = 0;
                currentIndex = 0;
                continue;
            }
        }
        else if (key == 30037) { 
            if (SetupMenu() == SIGNAL_QUIT_TO_MENU) {
                if (GoToLibraryMenu(&maxItems, &topIndex, &currentIndex) == -1) break;
                continue;
            }
            continue; 
        } 
        if (key == 30100 || key == 0x7f50) { 
            if (ShowHelpScreen() == SIGNAL_QUIT_TO_MENU) {
                if (GoToLibraryMenu(&maxItems, &topIndex, &currentIndex) == -1) break;
                continue;
            }
            continue; 
        }
        else if ((key >= 0x20 && key <= 0x7E) || key == 0x87 || key == 0x99) { if (searchPos < SEARCH_LEN) { char c = (char)key; if (key == 0x87 || key == 0x99) c = '-'; else if (c >= 'a' && c <= 'z') c -= 32; if (isInsertMode) { int j; for(j = searchPos; j >= cursorIndex; j--) searchBuffer[j+1] = searchBuffer[j]; searchBuffer[cursorIndex] = c; searchPos++; } else { searchBuffer[cursorIndex] = c; if (cursorIndex == searchPos) { searchPos++; searchBuffer[searchPos] = 0; } } cursorIndex++; currentIndex = GetFirstSplitWord(JumpListToPrefix()); topIndex = currentIndex; } }
    }
    CloseSDCardFiles(); return 1;
}

#pragma section _BR_Size
unsigned long BR_Size;
#pragma section
#pragma section _TOP
int InitializeSystem(int isAppli, unsigned short OptionNum) { return INIT_ADDIN_APPLICATION(isAppli, OptionNum); }
#pragma section

// Force recompile for updated ui_data.h icon - Oxford with '2' badge
