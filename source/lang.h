/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#ifndef langH
#define langH

extern char lang[64];
extern WCHAR dtBuf[2048];

char *lng(int i, char *s);
int strleni(char *s);
void initLang();
int setLang(int cmd);
HMENU loadMenu(char *name, int *subId);
void loadMenu(HWND hwnd, char *name, int *subId);
void changeDialog(HWND &wnd, int x, int y, LPCTSTR dlgTempl, DLGPROC dlgProc);
void setDlgTexts(HWND hDlg);
void setDlgTexts(HWND hDlg, int id);
void CodePageToWideChar(char* s);
void Utf8ToWideChar(char* s);
void SetWindowTextT(HWND hWnd, char* s);
void GetWindowTextT(HWND hWnd, char* s, int bufLen);
void SetWindowTextUtf8(HWND hWnd, char* s);
void SetEditTextUtf8(HWND hWnd, char* s, DWORD flags=0);
void getExeDir(char *fn, char *e);
char *cutPath(char *s);
int convertCodePage(char*& dest, char* src, int len, UINT destCodePage, UINT srcCodePage);
bool loadFile(char* fn, char*& content, DWORD& len, char*& start, void (*clear)(), bool ignoreError=false);

extern void langChanging();
extern void langChanged();
extern void msg(char *text, ...);
extern HINSTANCE inst;

#endif
