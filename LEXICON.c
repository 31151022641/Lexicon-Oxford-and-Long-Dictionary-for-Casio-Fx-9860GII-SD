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
#define CACHE_SIZE 10
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
int g_hl_line = 0;
int g_timer_tick = 0;
int isScrollingPaused = 0;
int g_searchOffset = 0;
int blinkState = 1;
int sw_alpha_lock = 1;

/* Cache for smooth scrolling */
typedef struct { int index; char disp[24]; char spos[8]; } ListCache;
static ListCache list_cache[CACHE_SIZE];
static int cache_base = -1;

static unsigned char ICON_HELP[] = {0x7F, 0x50, ':', 'H', 'e', 'l', 'p', 0};
static unsigned char ICON_HELP_HDR[] = {0x7F, 0x50, ':', ' ', 'H', 'e', 'l', 'p', 0};

FONTCHARACTER oald_idx[] = {'\\', '\\', 'c', 'r', 'd', '0', '\\', 'O', 'A', 'L', 'D', '.', 'I', 'D', 'X', 0};
FONTCHARACTER oald_dat[] = {'\\', '\\', 'c', 'r', 'd', '0', '\\', 'O', 'A', 'L', 'D', '.', 'D', 'A', 'T', 0};
FONTCHARACTER ldoce_idx[] = {'\\', '\\', 'c', 'r', 'd', '0', '\\', 'L', 'D', 'O', 'C', 'E', '.', 'I', 'D', 'X', 0};
FONTCHARACTER ldoce_dat[] = {'\\', '\\', 'c', 'r', 'd', '0', '\\', 'L', 'D', 'O', 'C', 'E', '.', 'D', 'A', 'T', 0};

static char def_buffer_storage[5120]; // 5KB
static char ctx_buffer_storage[2048];
static char *defBuffer = def_buffer_storage;
static char *ctxBuffer = ctx_buffer_storage;

/* Prototypes */
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
void ListViewTimerHandler(void);
void PrintIPA(const char* s, int* currentLine);
int GetNextWrappedLine(const char* text, int startPos, int limit_chars, char* buffer, int* lineLen);
int HandleSearch(int currentIndex);
int MainMenu();
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

/* Implementation */

unsigned int TranslateAlphaKey(unsigned int key) {
    if (!sw_alpha_lock) return key;
    switch (key) {
        case 30001: return 'a'; case 0x95: return 'b'; case 0x85: return 'c'; case 0x81: return 'd'; case 0x82: return 'e'; case 0x83: return 'f'; case 0xbb: return 'g'; case 30046: return 'h'; case 0x28: return 'i'; case 0x29: return 'j'; case 0x2c: return 'k'; case 0x0e: return 'l'; 
        case 0x37: return 'm'; case 0x38: return 'n'; case 0x39: return 'o'; case 0x34: return 'p'; case 0x35: return 'q'; case 0x36: return 'r'; case 0xa9: return 's'; case 0xb9: return 't'; case 0x31: return 'u'; case 0x32: return 'v'; case 0x33: return 'w'; case 0x89: return 'x'; case 0x99: return 'y'; case 0x30: return 'z'; case 0x2e: return ' ';
    }
    return key;
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
        else if (*s == ' ' && *(s+1) == 'I' && *(s+2) == ' ') { s += 3; }
        else if (*s == ' ') { s++; }
        else if (*s == 0xE5 || *s == 0xE6 || *s == 0xE7 || *s == 0x7F) { *d++ = *s++; *d++ = *s++; } 
        else { *d++ = *s++; } 
    } 
    *d = '\0';
}

void StripSuperscript(char* str) {
    int i = strlen(str) - 1; while (i >= 0 && isdigit(str[i])) { str[i] = '\0'; i--; }
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

void DefViewTimerHandler(void) {
    int limit = (g_hl_pos[0] == '\0') ? 21 : 15;
    if (CasioStrLen(g_hl_word) > limit && !isScrollingPaused) {
        char dispWord[128]; g_timer_tick++; CasioSubStr(g_hl_word, GetCurrentScrollOffset(g_hl_word, limit, g_timer_tick), limit, dispWord);
        locate(1, g_hl_line); PrintRev((unsigned char*)dispWord); if (g_hl_pos[0]) PrintMini(CasioStrLen(dispWord) * 6 + 4, (g_hl_line - 1) * 8 + 2, (unsigned char*)g_hl_pos, 1);
        Bdisp_PutDisp_DD();
    }
}

void ListViewTimerHandler(void) {
    static int tick50 = 0; int need_disp = 0; tick50++;
    if (tick50 % (GetScrollInterval() / 50) == 0) {
        int limit = (g_hl_pos[0] == '\0') ? 21 : 15;
        if (CasioStrLen(g_hl_word) > limit && !isScrollingPaused) {
            char dispWord[128]; g_timer_tick++; CasioSubStr(g_hl_word, GetCurrentScrollOffset(g_hl_word, limit, g_timer_tick), limit, dispWord);
            locate(1, g_hl_line); PrintRev((unsigned char*)dispWord); if (g_hl_pos[0]) PrintMini(CasioStrLen(dispWord) * 6 + 4, (g_hl_line - 1) * 8 + 2, (unsigned char*)g_hl_pos, 1);
            need_disp = 1;
        }
    }
    if (tick50 % 8 == 0) { blinkState = 1 - blinkState; UpdateSearchBar(blinkState); need_disp = 1; }
    if (need_disp) Bdisp_PutDisp_DD();
}

void UpdateSearchBar(int blinkState) {
    char temp[64]; int cx, maxDisp, hasLeft, hasRight;
    if (cursorIndex < g_searchOffset) g_searchOffset = cursorIndex;
    while(1) { hasLeft = (g_searchOffset > 0); maxDisp = 19 - (hasLeft ? 1 : 0); hasRight = (searchPos > g_searchOffset + maxDisp); if (hasRight) maxDisp--; if (cursorIndex >= g_searchOffset + maxDisp) g_searchOffset++; else break; }
    strcpy(temp, "S:"); if (hasLeft) strcat(temp, "\xE6\x9A");
    { char sub[64]; int i; for(i = 0; i < maxDisp && (g_searchOffset + i) < searchPos; i++) sub[i] = searchBuffer[g_searchOffset + i]; sub[i] = '\0'; strcat(temp, sub); }
    if (hasRight) strcat(temp, "\xE6\x91"); while (CasioStrLen(temp) < 21) strcat(temp, " ");
    locate(1, 1); Print((unsigned char*)temp);
    if (sw_alpha_lock) Bdisp_SetPoint_VRAM(127, 0, 1); else Bdisp_SetPoint_VRAM(127, 0, 0);
    if (blinkState) { cx = (2 + (hasLeft ? 1 : 0) + (cursorIndex - g_searchOffset)) * 6; if (isInsertMode) Bdisp_DrawLineVRAM(cx, 0, cx, 7); else Bdisp_DrawLineVRAM(cx, 7, cx + 5, 7); }
}

int GetScrollInterval() { if (scrollSpeedLevel == 1) return 500; if (scrollSpeedLevel == 2) return 400; if (scrollSpeedLevel == 4) return 200; if (scrollSpeedLevel == 5) return 150; return 300; }

void CloseSDCardFiles() { if (idx_handle >= 0) Bfile_CloseFile(idx_handle); if (dat_handle >= 0) Bfile_CloseFile(dat_handle); idx_handle = -1; dat_handle = -1; sd_dictionarySize = 0; cache_base = -1; }

int OpenSDCardFiles() {
    int retry; FONTCHARACTER *idx_file, *dat_file;
    if (idx_handle >= 0 && dat_handle >= 0) return 0;
    if (currentMode == MODE_OALD_SD) { idx_file = oald_idx; dat_file = oald_dat; }
    else if (currentMode == MODE_LDOCE_SD) { idx_file = ldoce_idx; dat_file = ldoce_dat; }
    else return -1;
    for (retry = 0; retry < 5; retry++) {
        idx_handle = Bfile_OpenFile(idx_file, _OPENMODE_READ);
        if (idx_handle >= 0) { sd_dictionarySize = Bfile_GetFileSize(idx_handle) / RECORD_SIZE; dat_handle = Bfile_OpenFile(dat_file, _OPENMODE_READ); if (dat_handle >= 0) return 0; Bfile_CloseFile(idx_handle); idx_handle = -1; }
        Sleep(100);
    }
    return -1;
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
    int pos = index * RECORD_SIZE; if (idx_handle < 0) { if (OpenSDCardFiles() < 0) return; }
    if (outDispWord) { SafeSDRead(0, outDispWord, 24, pos); outDispWord[23] = '\0'; }
    if (outSearchWord) { SafeSDRead(0, outSearchWord, 24, pos + 24); outSearchWord[23] = '\0'; }
    if (outShortPos) { SafeSDRead(0, outShortPos, 8, pos + 48); outShortPos[7] = '\0'; }
    if (outOffset) { unsigned int rv = 0; unsigned char* b = (unsigned char*)&rv; SafeSDRead(0, &rv, 4, pos + 56); *outOffset = (unsigned int)b[0] | ((unsigned int)b[1] << 8) | ((unsigned int)b[2] << 16) | ((unsigned int)b[3] << 24); }
}

int ShowHelpScreen() {
    unsigned int key; int scroll = 0, i;
    const char* help_lines[] = { "=== GLOBAL KEYS ===", "SHIFT+EXIT : Quit to Menu", (char*)ICON_HELP_HDR, " ", "=== LIST VIEW ===", "A-Z/0-9 : Search words", "DEL : Backspace/Delete", "AC  : Clear search box", "EXE : Open Definition", "F1  : Fuzzy Search", "UP/DWN : Scroll list", "L/R : Jump list pages", " ", "=== DEFINITION VIEW ===", "UP/DWN : Scroll text", "L/R : Prev/Next word", "EXE : Jump to Highlight", "( / ) : Toggle Pronun.", "EXIT : Back to List View", " ", "=== SETUP MENU ===", "UP/DWN : Select option", "EXE/F1 : Toggle/Change", "F6 : Default settings", "EXIT : Save & Close" };
    while (1) {
        Bdisp_AllClr_DDVRAM(); for (i = 0; i < 8; i++) { if (scroll + i < 25) { locate(1, i + 1); if (help_lines[scroll + i][0] == '=') PrintRev((unsigned char*)help_lines[scroll + i]); else Print((unsigned char*)help_lines[scroll + i]); } }
        Bdisp_PutDisp_DD(); GetKey(&key); if (key == KEY_CTRL_QUIT) return SIGNAL_QUIT_TO_MENU; if (key == KEY_CTRL_EXIT || key == 30100 || key == 0x7f50) return 0;
        if (key == KEY_CTRL_UP && scroll > 0) scroll--; if (key == KEY_CTRL_DOWN && scroll < 17) scroll++;
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

int GetNextWrappedLine(const char* text, int startPos, int limit_chars, char* buffer, int* lineLen) {
    int pos = startPos, chars = 0, lastSpace = -1, tlen = strlen(text);
    if (pos >= tlen || text[pos] == '\n') { if (text[pos] == '\n') pos++; buffer[0] = '\0'; *lineLen = 0; return pos; }
    while (pos < tlen && text[pos] != '\n' && chars < limit_chars) {
        if (text[pos] == ' ') lastSpace = pos;
        if ((unsigned char)text[pos] >= 0xE5 || (unsigned char)text[pos] == 0x7F) {
            if (chars + 2 > limit_chars) break;
            buffer[(*lineLen)++] = text[pos++]; buffer[(*lineLen)++] = text[pos++]; chars += 2;
        } else { buffer[(*lineLen)++] = text[pos++]; chars++; }
    }
    if (chars >= limit_chars && lastSpace > startPos && pos < tlen && text[pos] != ' ' && text[pos] != '\n') {
        int diff = pos - lastSpace; pos = lastSpace + 1; *lineLen -= diff;
    }
    buffer[*lineLen] = '\0'; if (pos < tlen && text[pos] == '\n') pos++; return pos;
}

int ShowDefinitionViewer(int index) {
    unsigned int key; 
    int scrollOffset = 0, currentIndex = index, viewScreen = 0;
    int maxItems = (currentMode == MODE_INTERNAL) ? dictionarySize : sd_dictionarySize;
    char word[128], pos_str[32], ipa_br[128], ipa_nam[128], dispWord[128], *definition, *s; 
    unsigned int offset; 
    int i, j, reset_view = 1, highlight_idx = 0, has_ctx_data = 0;
    int item_positions[64], item_count = 0; 
    char item_jump_words[64][32];
    int def_len;
    int in_ex = 0, is_shortcut = 0, shift_pressed = 0;
    
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
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 2047) ctxBuffer[i++] = c; ctxBuffer[i] = '\0'; offset += i + 1;
            } else if (currentMode == MODE_LDOCE_SD) {
                ReadSDWord(currentIndex, NULL, NULL, NULL, &offset);
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 127) word[i++] = c; word[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 31) pos_str[i++] = c; pos_str[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 127) ipa_br[i++] = c; ipa_br[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 127) ipa_nam[i++] = c; ipa_nam[i] = '\0'; offset += i + 1;
                i = 0; while (SafeSDRead(1, &c, 1, offset + i) == 1 && c != '\0' && i < 5119) defBuffer[i++] = c; defBuffer[i] = '\0'; offset += i + 1;
                j = 0; if (i < 5118) defBuffer[i++] = '\n';
                while (SafeSDRead(1, &c, 1, offset + j) == 1 && c != '\0' && i < 5119) { defBuffer[i++] = c; j++; } defBuffer[i] = '\0'; offset += j + 1;
                ctxBuffer[0] = '\0'; // LDOCE Payload doesn't contain ctxBuffer field
            }
            if (ipa_br[0]) { int len = strlen(ipa_br); if (len < 126) { memmove(ipa_br + 1, ipa_br, len + 1); ipa_br[0] = '/'; ipa_br[len + 1] = '/'; ipa_br[len + 2] = '\0'; } }
            if (ipa_nam[0]) { int len = strlen(ipa_nam); if (len < 126) { memmove(ipa_nam + 1, ipa_nam, len + 1); ipa_nam[0] = '/'; ipa_nam[len + 1] = '/'; ipa_nam[len + 2] = '\0'; } }
            
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
            isScrollingPaused = 0; g_timer_tick = 0; scrollOffset = 0; highlight_idx = 0; reset_view = 0;
        }
        {
            int pos = 0, currentLine = 1, visible_lines = 0, has_more = 0; Bdisp_AllClr_DDVRAM();
            strcpy(g_hl_word, word); if (pos_str[0]) strcpy(g_hl_pos, pos_str); else g_hl_pos[0] = '\0'; g_hl_line = 1;
            CasioSubStr(word, GetCurrentScrollOffset(word, (pos_str[0] == '\0' || pos_str[0] < 0x20 ? 40 : 35), g_timer_tick), (pos_str[0] == '\0' || pos_str[0] < 0x20 ? 40 : 35), dispWord);
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
                    s = pos_str;
                    if (*s == '\x02') { PrintMini(px, 2, (unsigned char*)"\xE5\x80", 1); px += 12; s++; }
                    else if (*s == '\x03') { PrintMini(px, 2, (unsigned char*)"\xE6\x80", 1); px += 12; s++; }
                    if (*s) PrintMini(px, 2, (unsigned char*)s, 1);
                }
            }
            currentLine = 2;

            if (currentMode == MODE_INTERNAL) {            if (viewScreen == 0) {
                if (showPron == 3) { 
                    if (ipa_br[0]) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"BrE", 0); currentLine++; PrintIPA(ipa_br, &currentLine); currentLine++; }
                    if (ipa_nam[0] && currentLine <= MAX_LINES) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"NAmE", 0); currentLine++; PrintIPA(ipa_nam, &currentLine); currentLine++; }
                } else {
                    int dBrE = (showPron == 1) || (forceShowMode == 1), dNAmE = (showPron == 2) || (forceShowMode == 2);
                    if (dBrE && ipa_br[0]) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"BrE", 0); currentLine++; PrintIPA(ipa_br, &currentLine); currentLine++; }
                    if (dNAmE && ipa_nam[0] && currentLine <= MAX_LINES) { PrintMini(0, (currentLine-1)*8+2, (unsigned char*)"NAmE", 0); currentLine++; PrintIPA(ipa_nam, &currentLine); currentLine++; }
                }
                in_ex = 0; is_shortcut = 0;
                definition = defBuffer; def_len = strlen(definition);
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0, limit = 21, start_p = pos, is_f_ex = 0;
                    if (pos == 0 || definition[pos-1] == '\n') {
                        if (definition[pos] == '\x08' || definition[pos] == '\x05') { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                        if (definition[pos] == '\x06') { is_shortcut = 1; in_ex = 0; pos++; }
                        else if (definition[pos] == '\x01') { in_ex = 1; is_shortcut = 0; pos++; }
                        else if (definition[pos] == '\x04') { in_ex = 0; is_shortcut = 0; pos++; }
                        else if (definition[pos] >= 0x20 && definition[pos] < 0x80 && isdigit(definition[pos])) { in_ex = 0; is_shortcut = 0; }
                    }
                    if (in_ex) { 
                        if (!showExamples) { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; in_ex = 0; continue; }
                        is_f_ex = 1; limit = 30; 
                    } else if (is_shortcut) { limit = 19; }
                    
                    pos = GetNextWrappedLine(definition, pos, limit, buffer, &lineLen);
                    if (is_shortcut == 2 && pos > 0 && definition[pos-1] != '\n') {
                        while (pos < def_len && definition[pos] != '\n') pos++;
                        if (pos < def_len && definition[pos] == '\n') pos++;
                    }
                    
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { has_more = 1; break; }
                        if (is_shortcut) { locate(1, currentLine); Print((unsigned char*)"\xE6\xA8"); Print((unsigned char*)buffer); }
                        else if (is_f_ex) { PrintMini(4, (currentLine-1)*8+2, (unsigned char*)buffer, 0); }
                        else { 
                            locate(1, currentLine); s = buffer; 
                            while (*s) { 
                                if (*s == '\x02') Print((unsigned char*)"\xE5\x80"); 
                                else if (*s == '\x03') Print((unsigned char*)"\xE6\x80"); 
                                else if (*s == '\x06') Print((unsigned char*)"\xE6\xA8"); 
                                else if (*s == '\x07') Print((unsigned char*)"\xE6\xA6"); 
                                else if (*s == '\x01' || *s == '\x04' || *s == '\x05' || *s == '\x0B') { }
                                else if (*s >= 0x20) { 
                                    unsigned char b[3]; b[0] = *s++; 
                                    if ((unsigned char)b[0] >= 0xE5 || (unsigned char)b[0] == 0x7F) b[1] = *s++; else b[1] = 0; 
                                    b[2] = 0; Print(b); continue; 
                                } s++; 
                            } 
                        }
                        currentLine++;
                    }
                    visible_lines++;
                }
                if (has_more) { locate(21, 8); Print((unsigned char*)"\xE6\xAD"); }
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
                definition = defBuffer; def_len = strlen(definition); pos = 0;
                
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0, start_p = pos;
                    if (pos == 0 || definition[pos-1] == '\n') {
                        if (definition[pos] == '\x05') { in_f_gram = 1; in_link = 0; pos++; }
                        else if (definition[pos] == '\x08') { in_link = 1; in_f_gram = 0; pos++; }
                        else { in_f_gram = 0; in_link = 0; while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                    }
                    
                    pos = GetNextWrappedLine(definition, pos, 30, buffer, &lineLen);
                    
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { has_more = 1; break; }
                        
                        if (in_f_gram) { 
                            char* p = buffer; int lb;
                            if (*p == '[') p++;
                            lb = strlen(p);
                            if (lb > 0 && p[lb-1] == ']') p[lb-1] = '\0';
                            PrintMini(0, (currentLine-1)*8+2, (unsigned char*)p, 0); 
                        }
                        else if (in_link) {
                            if (highlight_idx < item_count && item_positions[highlight_idx] == start_p) Bdisp_DrawLineVRAM(0, (currentLine-1)*8+1, 127, (currentLine-1)*8+1);
                            locate(1, currentLine); s = buffer;
                            while (*s) {
                                if (*s == '\x08') Print((unsigned char*)"\xE6\x9E");
                                else if (*s == '\x0B') Print((unsigned char*)" ");
                                else if (*s >= 0x20) { unsigned char b[3]; b[0] = *s++; if ((unsigned char)b[0] >= 0xE5 || (unsigned char)b[0] == 0x7F) b[1] = *s++; else b[1] = 0; b[2] = 0; Print(b); continue; }
                                s++;
                            }
                            if (highlight_idx < item_count && item_positions[highlight_idx] == start_p) Bdisp_DrawLineVRAM(0, currentLine*8, 127, currentLine*8);
                        }
                        currentLine++;
                    }
                    visible_lines++;
                }
                locate(1, 8); PrintRev((unsigned char*)"");
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
                in_ex = 0; is_shortcut = 0;
                definition = defBuffer; def_len = strlen(definition);
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0, limit = 21, start_p = pos, is_f_ex = 0;
                    if (pos == 0 || definition[pos-1] == '\n') {
                        if (definition[pos] == '\x08' || definition[pos] == '\x05') { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                        if (definition[pos] == '\x06') { is_shortcut = 1; in_ex = 0; pos++; }
                        else if (definition[pos] == '\x01') { in_ex = 1; is_shortcut = 0; pos++; }
                        else if (definition[pos] == '\x04') { in_ex = 0; is_shortcut = 0; pos++; }
                        else if (definition[pos] >= 0x20 && definition[pos] < 0x80 && isdigit(definition[pos])) { in_ex = 0; is_shortcut = 0; }
                    }
                    if (in_ex) { 
                        if (!showExamples) { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; in_ex = 0; continue; }
                        is_f_ex = 1; limit = 30; 
                    } else if (is_shortcut) { limit = 19; }
                    
                    pos = GetNextWrappedLine(definition, pos, limit, buffer, &lineLen);
                    if (is_shortcut == 2 && pos > 0 && definition[pos-1] != '\n') {
                        while (pos < def_len && definition[pos] != '\n') pos++;
                        if (pos < def_len && definition[pos] == '\n') pos++;
                    }
                    
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { has_more = 1; break; }
                        if (is_shortcut) { locate(1, currentLine); Print((unsigned char*)"\xE6\xA8"); Print((unsigned char*)buffer); }
                        else if (is_f_ex) { PrintMini(4, (currentLine-1)*8+2, (unsigned char*)buffer, 0); }
                        else { 
                            locate(1, currentLine); s = buffer; 
                            while (*s) { 
                                if (*s == '\x02') Print((unsigned char*)"\xE5\x80"); 
                                else if (*s == '\x03') Print((unsigned char*)"\xE6\x80"); 
                                else if (*s == '\x06') Print((unsigned char*)"\xE6\xA8"); 
                                else if (*s == '\x07') Print((unsigned char*)"\xE6\xA6"); 
                                else if (*s == '\x01' || *s == '\x04' || *s == '\x05' || *s == '\x0B') { }
                                else if (*s >= 0x20) { 
                                    unsigned char b[3]; b[0] = *s++; 
                                    if ((unsigned char)b[0] >= 0xE5 || (unsigned char)b[0] == 0x7F) b[1] = *s++; else b[1] = 0; 
                                    b[2] = 0; Print(b); continue; 
                                } s++; 
                            } 
                        }
                        currentLine++;
                    }
                    visible_lines++;
                }
                if (has_more) { locate(21, 8); Print((unsigned char*)"\xE6\xAD"); }
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
                definition = defBuffer; def_len = strlen(definition); pos = 0;
                
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0, start_p = pos;
                    if (pos == 0 || definition[pos-1] == '\n') {
                        if (definition[pos] == '\x05') { in_f_gram = 1; in_link = 0; pos++; }
                        else if (definition[pos] == '\x08') { in_link = 1; in_f_gram = 0; pos++; }
                        else { in_f_gram = 0; in_link = 0; while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                    }
                    
                    pos = GetNextWrappedLine(definition, pos, 30, buffer, &lineLen);
                    
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { has_more = 1; break; }
                        
                        if (in_f_gram) { 
                            char* p = buffer; int lb;
                            if (*p == '[') p++;
                            lb = strlen(p);
                            if (lb > 0 && p[lb-1] == ']') p[lb-1] = '\0';
                            PrintMini(0, (currentLine-1)*8+2, (unsigned char*)p, 0); 
                        }
                        else if (in_link) {
                            if (highlight_idx < item_count && item_positions[highlight_idx] == start_p) Bdisp_DrawLineVRAM(0, (currentLine-1)*8+1, 127, (currentLine-1)*8+1);
                            locate(1, currentLine); s = buffer;
                            while (*s) {
                                if (*s == '\x08') Print((unsigned char*)"\xE6\x9E");
                                else if (*s == '\x0B') Print((unsigned char*)" ");
                                else if (*s >= 0x20) { unsigned char b[3]; b[0] = *s++; if ((unsigned char)b[0] >= 0xE5 || (unsigned char)b[0] == 0x7F) b[1] = *s++; else b[1] = 0; b[2] = 0; Print(b); continue; }
                                s++;
                            }
                            if (highlight_idx < item_count && item_positions[highlight_idx] == start_p) Bdisp_DrawLineVRAM(0, currentLine*8, 127, currentLine*8);
                        }
                        currentLine++;
                    }
                    visible_lines++;
                }
                locate(1, 8); PrintRev((unsigned char*)"");
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
                in_ex = 0; is_shortcut = 0;
                definition = defBuffer; def_len = strlen(definition);
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0, limit = 21, start_p = pos, is_f_ex = 0;
                    if (pos == 0 || definition[pos-1] == '\n') {
                        if (definition[pos] == '\x08' || definition[pos] == '\x05') { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                        if (definition[pos] == '\x06') { is_shortcut = 1; in_ex = 0; pos++; }
                        else if (definition[pos] == '\x01') { in_ex = 1; is_shortcut = 0; pos++; }
                        else if (definition[pos] == '\x04') { in_ex = 0; is_shortcut = 0; pos++; }
                        else if (definition[pos] >= 0x20 && definition[pos] < 0x80 && isdigit(definition[pos])) { in_ex = 0; is_shortcut = 0; }
                    }
                    if (in_ex) { 
                        if (!showExamples) { while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; in_ex = 0; continue; }
                        is_f_ex = 1; limit = 30; 
                    } else if (is_shortcut) { limit = 19; }
                    
                    pos = GetNextWrappedLine(definition, pos, limit, buffer, &lineLen);
                    if (is_shortcut == 2 && pos > 0 && definition[pos-1] != '\n') {
                        while (pos < def_len && definition[pos] != '\n') pos++;
                        if (pos < def_len && definition[pos] == '\n') pos++;
                    }
                    
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { has_more = 1; break; }
                        if (is_shortcut) { locate(1, currentLine); Print((unsigned char*)"\xE6\xA8"); Print((unsigned char*)buffer); }
                        else if (is_f_ex) { PrintMini(4, (currentLine-1)*8+2, (unsigned char*)buffer, 0); }
                        else { 
                            locate(1, currentLine); s = buffer; 
                            while (*s) { 
                                if (*s == '\x02') Print((unsigned char*)"\xE5\x80"); 
                                else if (*s == '\x03') Print((unsigned char*)"\xE6\x80"); 
                                else if (*s == '\x06') Print((unsigned char*)"\xE6\xA8"); 
                                else if (*s == '\x07') Print((unsigned char*)"\xE6\xA6"); 
                                else if (*s == '\x01' || *s == '\x04' || *s == '\x05' || *s == '\x0B') { }
                                else if (*s >= 0x20) { 
                                    unsigned char b[3]; b[0] = *s++; 
                                    if ((unsigned char)b[0] >= 0xE5 || (unsigned char)b[0] == 0x7F) b[1] = *s++; else b[1] = 0; 
                                    b[2] = 0; Print(b); continue; 
                                } s++; 
                            } 
                        }
                        currentLine++;
                    }
                    visible_lines++;
                }
                if (has_more) { locate(21, 8); Print((unsigned char*)"\xE6\xAD"); }
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
                definition = defBuffer; def_len = strlen(definition); pos = 0;
                
                while (definition && pos < def_len) {
                    char buffer[256]; int lineLen = 0, start_p = pos;
                    if (pos == 0 || definition[pos-1] == '\n') {
                        if (definition[pos] == '\x05') { in_f_gram = 1; in_link = 0; pos++; }
                        else if (definition[pos] == '\x08') { in_link = 1; in_f_gram = 0; pos++; }
                        else { in_f_gram = 0; in_link = 0; while (pos < def_len && definition[pos] != '\n') pos++; if (definition[pos] == '\n') pos++; continue; }
                    }
                    
                    pos = GetNextWrappedLine(definition, pos, 30, buffer, &lineLen);
                    
                    if (visible_lines >= scrollOffset) {
                        if (currentLine > 8) { has_more = 1; break; }
                        
                        if (in_f_gram) { 
                            char* p = buffer; int lb;
                            if (*p == '[') p++;
                            lb = strlen(p);
                            if (lb > 0 && p[lb-1] == ']') p[lb-1] = '\0';
                            PrintMini(0, (currentLine-1)*8+2, (unsigned char*)p, 0); 
                        }
                        else if (in_link) {
                            if (highlight_idx < item_count && item_positions[highlight_idx] == start_p) Bdisp_DrawLineVRAM(0, (currentLine-1)*8+1, 127, (currentLine-1)*8+1);
                            locate(1, currentLine); s = buffer;
                            while (*s) {
                                if (*s == '\x08') Print((unsigned char*)"\xE6\x9E");
                                else if (*s == '\x0B') Print((unsigned char*)" ");
                                else if (*s >= 0x20) { unsigned char b[3]; b[0] = *s++; if ((unsigned char)b[0] >= 0xE5 || (unsigned char)b[0] == 0x7F) b[1] = *s++; else b[1] = 0; b[2] = 0; Print(b); continue; }
                                s++;
                            }
                            if (highlight_idx < item_count && item_positions[highlight_idx] == start_p) Bdisp_DrawLineVRAM(0, currentLine*8, 127, currentLine*8);
                        }
                        currentLine++;
                    }
                    visible_lines++;
                }
                locate(1, 8); PrintRev((unsigned char*)"");
                if (has_more) { locate(21, 8); Print((unsigned char*)"\xE6\xAD"); }
            }
            }
            Bdisp_PutDisp_DD();
            SetTimer(1, GetScrollInterval(), DefViewTimerHandler); GetKey(&key); KillTimer(1);

            if (key == KEY_CTRL_QUIT) return SIGNAL_QUIT_TO_MENU;
            if (key == 30100 || key == 0x7f50) { if (ShowHelpScreen() == SIGNAL_QUIT_TO_MENU) return SIGNAL_QUIT_TO_MENU; continue; }
            if (key == KEY_CTRL_EXIT) return currentIndex;
            if (key == KEY_CTRL_SETUP) { if (SetupMenu() == SIGNAL_QUIT_TO_MENU) return SIGNAL_QUIT_TO_MENU; continue; }
            if (key == KEY_CTRL_DOWN && has_more) scrollOffset++;
            if (key == KEY_CTRL_UP && scrollOffset > 0) scrollOffset--;
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
            if (key == KEY_CHAR_XTT) { if (has_ctx_data) { viewScreen = 1 - viewScreen; scrollOffset = 0; } continue; }
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
int BinarySearchSDCard(const char* targetWord, int searchPrefix, int* closest) { int left = 0, right = sd_dictionarySize - 1, bestMatch = -1; char sWord[32]; if (idx_handle < 0) { if (OpenSDCardFiles() < 0) return -1; } while (left <= right) { int mid = left + (right - left) / 2, cmp; ReadSDWord(mid, NULL, sWord, NULL, NULL); if (searchPrefix) cmp = CaseInsensitivePrefixCompare(sWord, targetWord, searchPos); else cmp = CaseInsensitiveCompare(sWord, targetWord); if (cmp == 0) { bestMatch = mid; right = mid - 1; if (!searchPrefix) break; } else if (cmp < 0) left = mid + 1; else right = mid - 1; } if (closest) *closest = left; return bestMatch; }
int JumpListToPrefix() { int left = 0, right = (currentMode == MODE_INTERNAL ? dictionarySize : sd_dictionarySize) - 1, bestMatch = -1; char sWord[32] = ""; if (right < 0 || searchPos == 0) return 0; while (left <= right) { int mid = left + (right - left) / 2, cmp; if (currentMode == MODE_INTERNAL) strncpy(sWord, dictionary[mid].word, 23); else ReadSDWord(mid, NULL, sWord, NULL, NULL); sWord[23] = '\0'; cmp = CaseInsensitivePrefixCompare(sWord, searchBuffer, searchPos); if (cmp == 0) { bestMatch = mid; right = mid - 1; } else if (cmp < 0) left = mid + 1; else right = mid - 1; } if (bestMatch >= 0) return bestMatch; return (left > right) ? right : left; }

int GetDistinctIdentifier_FromIdx(int index, char* outRoot, char* outPos) {
    char word[128];
    if (outPos) outPos[0] = '\0';
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
    GetDistinctIdentifier_FromIdx(idx, targetRoot, targetPos);
    while (idx > 0) {
        idx--;
        GetDistinctIdentifier_FromIdx(idx, curRoot, curPos);
        if (CaseInsensitiveCompare(targetRoot, curRoot) != 0 || strcmp(targetPos, curPos) != 0) return GetFirstSplitWord(idx);
    }
    return 0;
}

void EnsureVisible(int* topIdx, int curIdx) {
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
    Bdisp_AllClr_DDVRAM(); if (maxItems <= 0) { locate(1, 4); Print((unsigned char*)"No Data Found!"); return; }
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
        char dWord[128], short_pos[12], dispWord[64]; int limit;
        int current_disp_mode = currentMode;
        
        if (current_disp_mode == MODE_INTERNAL) { 
            strncpy(dWord, dictionary[listIdx].word, 127); dWord[127]='\0'; short_pos[0] = '\0'; 
        } else if (current_disp_mode == MODE_OALD_SD) { 
            int ci = listIdx - cache_base; if (ci >= 0 && ci < CACHE_SIZE) { strcpy(dWord, list_cache[ci].disp); strcpy(short_pos, list_cache[ci].spos); } else ReadSDWord(listIdx, dWord, NULL, short_pos, NULL); 
        } else if (current_disp_mode == MODE_LDOCE_SD) {
            int ci = listIdx - cache_base; if (ci >= 0 && ci < CACHE_SIZE) { strcpy(dWord, list_cache[ci].disp); strcpy(short_pos, list_cache[ci].spos); } else ReadSDWord(listIdx, dWord, NULL, short_pos, NULL); 
        }
        
        StripSyllableDots(dWord);
        limit = (short_pos[0] == '\0' ? 40 : 35); locate(1, i + 2);
        
        if (listIdx == first_cur) {
            if (current_disp_mode == MODE_INTERNAL) { 
                strncpy(g_hl_word, dWord, 127); g_hl_word[127] = '\0'; 
            } else if (current_disp_mode == MODE_OALD_SD) { 
                unsigned int off; char c; int j = 0; ReadSDWord(listIdx, NULL, NULL, NULL, &off); while (SafeSDRead(1, &c, 1, off + j) == 1 && c != '\0' && j < 127) g_hl_word[j++] = c; g_hl_word[j] = '\0'; StripSyllableDots(g_hl_word); 
            } else if (current_disp_mode == MODE_LDOCE_SD) { 
                unsigned int off; char c; int j = 0; ReadSDWord(listIdx, NULL, NULL, NULL, &off); while (SafeSDRead(1, &c, 1, off + j) == 1 && c != '\0' && j < 127) g_hl_word[j++] = c; g_hl_word[j] = '\0'; StripSyllableDots(g_hl_word); 
            }
            
            CasioSubStr(g_hl_word, GetCurrentScrollOffset(g_hl_word, limit, g_timer_tick), limit, dispWord);
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

int LevDist(const char *s, int ls, const char *t, int lt) {
    int d[33][33]; int i, j, cost, min; unsigned char c1, c2;
    if (ls > 32) ls = 32; if (lt > 32) lt = 32;
    if (ls == 0) return lt; if (lt == 0) return ls;
    for (i = 0; i <= ls; i++) d[i][0] = i; for (j = 0; j <= lt; j++) d[0][j] = j;
    for (i = 1; i <= ls; i++) { c1 = (unsigned char)s[i-1]; if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        for (j = 1; j <= lt; j++) { c2 = (unsigned char)t[j-1]; if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
            cost = (c1 == c2) ? 0 : 1; min = d[i-1][j] + 1;
            if (d[i][j-1] + 1 < min) min = d[i][j-1] + 1; if (d[i-1][j-1] + cost < min) min = d[i-1][j-1] + cost; d[i][j] = min; } }
    return d[ls][lt];
}

void PopupScrollTimer(void) { 
    g_timer_tick++; 
    if (CasioStrLen(g_hl_word) > 40) {
        char dp[64], line[80]; int mo = CasioStrLen(g_hl_word) - 40, off = g_timer_tick % (mo + 4); if (off > mo) off = 0;
        line[0] = '1' + g_hl_line; line[1] = ':'; line[2] = ' '; line[3] = '\0';
        CasioSubStr(g_hl_word, off, 40, dp); strcat(line, dp);
        locate(2, 3 + g_hl_line); PrintRev((unsigned char*)line); Bdisp_PutDisp_DD();
    }
}

typedef struct { int cIdx; char base[64]; int dist; } Score;

int SpellCheckPopup(int closestIdx) {
    int maxItems = (currentMode == MODE_INTERNAL) ? dictionarySize : sd_dictionarySize; DISPBOX box; Score scores[250]; int num_scores = 0, s_len, iter; Score t;
    char baseWords[5][64];
    unsigned int key; int i, j, count = 0, suggestionIndices[5];
    if (closestIdx < 0) closestIdx = 0; if (closestIdx >= maxItems) closestIdx = maxItems - 1;
    s_len = strlen(searchBuffer);
    for (i = -50; i <= 50; i++) {
        int cIdx = closestIdx + i; char base[64], posTag[16] = ""; if (cIdx < 0 || cIdx >= maxItems) continue;
        GetDistinctIdentifier_FromIdx(cIdx, base, posTag);
        if (posTag[0]) { char cleanPos[16]; strcpy(cleanPos, posTag); if (cleanPos[0] == '\x02' || cleanPos[0] == '\x03') memmove(cleanPos, cleanPos + 1, strlen(cleanPos)); strcat(base, " "); strcat(base, cleanPos); }
        scores[num_scores].cIdx = cIdx; strcpy(scores[num_scores].base, base); scores[num_scores].dist = LevDist(searchBuffer, s_len, base, CasioStrLen(base)); num_scores++;
    }
    for (i = 0; i <= s_len && i < 3; i++) {
        for (iter = 0; iter < 54; iter++) {
            char perm[32]; int p_idx = -1; strcpy(perm, searchBuffer);
            if (iter == 0) { 
                if (i < s_len - 1) { char tmp = perm[i]; perm[i] = perm[i+1]; perm[i+1] = tmp; } else continue;
            }
            else if (iter == 1) { 
                if (i < s_len) { memmove(perm + i, perm + i + 1, strlen(perm + i + 1) + 1); } else continue;
            }
            else if (iter >= 2 && iter < 28) { 
                if (i < s_len) { if (perm[i] == 'a' + (iter - 2)) continue; perm[i] = 'a' + (iter - 2); } else continue;
            }
            else if (iter >= 28 && iter < 54) {
                if (s_len < 31) { memmove(perm + i + 1, perm + i, strlen(perm + i) + 1); perm[i] = 'a' + (iter - 28); } else continue;
            }
            else continue;
            if (perm[0] == '\0') continue;
            
            p_idx = BinarySearchSDCard(perm, 0, NULL); if (p_idx < 0) p_idx = BinarySearchSDCard(perm, 1, NULL);
            if (p_idx >= 0 && num_scores < 240) {
                int k; for (k = -2; k <= 3; k++) {
                    int cIdx = p_idx + k; char base[64], posTag[16] = ""; int is_dup = 0, d_i; if (cIdx < 0 || cIdx >= maxItems || num_scores >= 249) continue;
                    for (d_i = 0; d_i < num_scores; d_i++) { if (scores[d_i].cIdx == cIdx) { is_dup = 1; break; } }
                    if (is_dup) continue;

                    GetDistinctIdentifier_FromIdx(cIdx, base, posTag);
                    if (posTag[0]) { char cleanPos[16]; strcpy(cleanPos, posTag); if (cleanPos[0] == '\x02' || cleanPos[0] == '\x03') memmove(cleanPos, cleanPos + 1, strlen(cleanPos)); strcat(base, " "); strcat(base, cleanPos); }
                    scores[num_scores].cIdx = cIdx; strcpy(scores[num_scores].base, base); scores[num_scores].dist = LevDist(searchBuffer, s_len, base, CasioStrLen(base)); num_scores++;
                }
            }
        }
    }
    for (i = 0; i < num_scores - 1; i++) {
        for (j = i + 1; j < num_scores; j++) {
            if (scores[j].dist < scores[i].dist) { t = scores[i]; scores[i] = scores[j]; scores[j] = t; }
        }
    }
    for (i = 0; i < num_scores && count < 5; i++) {
        int uniq = 1; for (j = 0; j < count; j++) { if (strcmp(baseWords[j], scores[i].base) == 0) { uniq = 0; break; } }
        if (uniq) { suggestionIndices[count] = scores[i].cIdx; strcpy(baseWords[count], scores[i].base); count++; }
    }
    if (count == 0) return -1; g_hl_line = 0; g_timer_tick = 0; strcpy(g_hl_word, baseWords[0]); KillTimer(1);
    while(1) { box.left = 2; box.top = 8; box.right = 125; box.bottom = 60; Bdisp_AreaClr_VRAM(&box); Bdisp_DrawLineVRAM(2, 8, 125, 8); Bdisp_DrawLineVRAM(2, 60, 125, 60); Bdisp_DrawLineVRAM(2, 8, 2, 60); Bdisp_DrawLineVRAM(125, 8, 125, 60); PrintMini(6, 10, (unsigned char*)"Did you mean...?", 0);
        for (i = 0; i < count; i++) { char line[80], dp[64]; int limit = 35, off = 0; if (i == g_hl_line) { int wl = CasioStrLen(baseWords[i]); if (wl > limit) { int mo = wl - limit; off = g_timer_tick % (mo + 4); if (off > mo) off = 0; } } line[0] = '1' + i; line[1] = ':'; line[2] = ' '; line[3] = '\0'; CasioSubStr(baseWords[i], off, limit, dp); strcat(line, dp);
            locate(2, 3 + i); if (i == g_hl_line) PrintRev((unsigned char*)line); else Print((unsigned char*)line); }
        Bdisp_PutDisp_DD(); SetTimer(2, GetScrollInterval(), PopupScrollTimer); GetKey(&key); KillTimer(2); if (key == KEY_CTRL_UP) { g_hl_line = (g_hl_line > 0) ? g_hl_line - 1 : count - 1; g_timer_tick = 0; strcpy(g_hl_word, baseWords[g_hl_line]); } else if (key == KEY_CTRL_DOWN) { g_hl_line = (g_hl_line < count - 1) ? g_hl_line + 1 : 0; g_timer_tick = 0; strcpy(g_hl_word, baseWords[g_hl_line]); } else if (key == KEY_CTRL_EXE) return suggestionIndices[g_hl_line]; else if (key == KEY_CTRL_QUIT) return SIGNAL_QUIT_TO_MENU; if (key == 30100 || key == 0x7f50) { if (ShowHelpScreen() == SIGNAL_QUIT_TO_MENU) return SIGNAL_QUIT_TO_MENU; continue; } if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC) return -1; else if (key >= KEY_CHAR_1 && key < KEY_CHAR_1 + count) return suggestionIndices[key - KEY_CHAR_1];
    }
}

int HandleSearch(int currentIndex) { int closest = 0; if (searchPos == 0) return currentIndex; if (currentMode == MODE_INTERNAL) { int i; for (i = 0; i < dictionarySize; i++) { if (CaseInsensitiveCompare(dictionary[i].word, searchBuffer) == 0) return ShowDefinitionViewer(i); } } else { int foundIdx = BinarySearchSDCard(searchBuffer, 0, &closest); if (foundIdx < 0) foundIdx = BinarySearchSDCard(searchBuffer, 1, &closest); if (foundIdx >= 0) return ShowDefinitionViewer(foundIdx); foundIdx = SpellCheckPopup(closest); if (foundIdx >= 0) return ShowDefinitionViewer(foundIdx); } return currentIndex; }

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
    unsigned int key; int sel = 0; 
    while (1) { 
        int left, right;
        Bdisp_AllClr_DDVRAM(); 
        
        DrawSprite(0, 0, 128, 64, ui_bg_library);
        
        DrawSprite(11, 14, 29, 37, ui_icon_1000);
        DrawSprite(50, 14, 29, 37, ui_icon_oald);
        DrawSprite(89, 14, 29, 37, ui_icon_ldoce);
        
        if (sel == 0)      { left = 11; right = 11 + 28; }
        else if (sel == 1) { left = 50; right = 50 + 28; }
        else { left = 89; right = 89 + 28; }
        
        Bdisp_AreaReverseVRAM(left, 14, right, 14 + 36);
        PrintMini(2, 58, (unsigned char*)"\x7F\x50:Help", 0);
        Bdisp_PutDisp_DD(); 
        GetKey(&key); 
        
        if (key == KEY_CTRL_QUIT) return SIGNAL_QUIT_TO_MENU; 
        if (key == KEY_CTRL_SETUP) { if (SetupMenu() == SIGNAL_QUIT_TO_MENU) return SIGNAL_QUIT_TO_MENU; continue; } 
        if (key == 30100 || key == 0x7f50) { if (ShowHelpScreen() == SIGNAL_QUIT_TO_MENU) return SIGNAL_QUIT_TO_MENU; continue; } 
        
        if (key == KEY_CTRL_LEFT) { if (sel > 0) sel--; else sel = 2; } 
        else if (key == KEY_CTRL_RIGHT) { if (sel < 2) sel++; else sel = 0; }
        
        if (key == KEY_CTRL_EXE) return sel; 
        if (key == KEY_CHAR_1) return 0; 
        if (key == KEY_CHAR_2) return 1; 
        if (key == KEY_CHAR_3) return 2; 
        if (key == KEY_CTRL_EXIT) return -1; 
    }
}
int AddIn_main(int isAppli, unsigned short OptionNum) {
    unsigned int key; int topIndex = 0, currentIndex = 0, choice, maxItems; SetQuitHandler((void*)CloseSDCardFiles); CloseSDCardFiles(); choice = MainMenu(); if (choice == -1) return 1; 
    if (choice == 0) currentMode = MODE_INTERNAL; else if (choice == 1) currentMode = MODE_OALD_SD; else currentMode = MODE_LDOCE_SD;
    if (currentMode != MODE_INTERNAL && OpenSDCardFiles() < 0) { PopUpWin(1); locate(2, 1); Print((unsigned char*)"SD Error!"); Bdisp_PutDisp_DD(); GetKey(&key); return 1; }
    maxItems = (currentMode == MODE_INTERNAL) ? dictionarySize : sd_dictionarySize; searchPos = 0; cursorIndex = 0; searchBuffer[0] = 0; sw_alpha_lock = 1;
    while (1) {
        DisplayList(topIndex, currentIndex); UpdateSearchBar(1); Bdisp_PutDisp_DD(); SetTimer(1, 50, ListViewTimerHandler); GetKey(&key); KillTimer(1);
        if (key == 30007) { sw_alpha_lock = 1 - sw_alpha_lock; continue; } key = TranslateAlphaKey(key); if (key == 30008) { isScrollingPaused = 1 - isScrollingPaused; continue; } if (key == 30033) { isInsertMode = 1 - isInsertMode; continue; } if (key == KEY_CTRL_LEFT) { if (cursorIndex > 0) cursorIndex--; continue; } if (key == KEY_CTRL_RIGHT) { if (cursorIndex < searchPos) cursorIndex++; continue; } g_timer_tick = 0; isScrollingPaused = 0;
        if (key == KEY_CTRL_UP) { if (currentIndex > 0) { currentIndex = GetPrevDistinctWord(currentIndex); EnsureVisible(&topIndex, currentIndex); } } else if (key == KEY_CTRL_DOWN) { int idx = GetNextDistinctWord(currentIndex); if (idx != -1 && idx < maxItems) { currentIndex = idx; EnsureVisible(&topIndex, currentIndex); } }
        else if (key == 30004) { if (maxItems > 0) { currentIndex = ShowDefinitionViewer(currentIndex); if (currentIndex == SIGNAL_QUIT_TO_MENU) break; EnsureVisible(&topIndex, currentIndex); } }
        else if (key == 30025) { if (cursorIndex < searchPos) { int j; for(j = cursorIndex; j < searchPos; j++) searchBuffer[j] = searchBuffer[j+1]; searchPos--; currentIndex = GetFirstSplitWord(JumpListToPrefix()); topIndex = currentIndex; } else if (cursorIndex > 0) { int j; for(j = cursorIndex - 1; j < searchPos; j++) searchBuffer[j] = searchBuffer[j+1]; searchPos--; cursorIndex--; currentIndex = GetFirstSplitWord(JumpListToPrefix()); topIndex = currentIndex; } }
        else if (key == 30015) { searchPos = 0; cursorIndex = 0; searchBuffer[0] = 0; }
        if (key == KEY_CTRL_QUIT || key == 30002) { choice = MainMenu(); if (choice == -1) break; 
            if (choice == 0) currentMode = MODE_INTERNAL; else if (choice == 1) currentMode = MODE_OALD_SD; else currentMode = MODE_LDOCE_SD;
            CloseSDCardFiles(); if (currentMode != MODE_INTERNAL && OpenSDCardFiles() < 0) break; 
            maxItems = (currentMode == MODE_INTERNAL) ? dictionarySize : sd_dictionarySize; searchPos = 0; cursorIndex = 0; searchBuffer[0] = 0; topIndex = 0; currentIndex = 0; continue; }
        else if (key == 30009) { currentIndex = HandleSearch(currentIndex); if (currentIndex == SIGNAL_QUIT_TO_MENU) break; topIndex = currentIndex; }
        else if (key == 30037) { if (SetupMenu() == SIGNAL_QUIT_TO_MENU) break; continue; } if (key == 30100 || key == 0x7f50) { if (ShowHelpScreen() == SIGNAL_QUIT_TO_MENU) break; continue; }
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
