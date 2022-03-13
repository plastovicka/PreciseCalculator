/*
	(C) Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#ifndef CONSOLE
#include "lang.h"
/*
Usage:
read lang from registry, call initLang() and langChanged()
paste setLang(cmd) to WM_COMMAND
paste setDlgTexts(hDlg,titleId) to WM_INITDIALOG
implement langChanged() - reload menu, file filters, invalidate, reload not modal dialogs
*/

#ifndef MAXLNGSTR
#define MAXLNGSTR 2500
#endif
//---------------------------------------------------------------------------
const int MAXLANG=60;
char lang[64];         //current language name (from file name)
char *langFile;        //file content (\n is replaced by \0)
char *lngstr[MAXLNGSTR];    //pointers to lines in langFile
char *lngNames[MAXLANG+1];  //all found languages names
bool isWin9X;
UINT codePage;
WCHAR dtBuf[2048];
//-------------------------------------------------------------------------
// 1) File name.
// 2) Unicode name to display on Windows NT+.
const struct Tlng { char *a; WCHAR *u; } lngInter[]={
#pragma setlocale("CZECH")
{"Czech", L"\x10c" L"esky" },
#pragma setlocale("SPANISH")
{"Spanish", L"Espa\xf1ol"},
#pragma setlocale("FRENCH")
{"French", L"Fran\xe7" L"ais"},
#pragma setlocale("RUSSIAN")
{"Russian", L"\x420\x443\x441\x441\x43a\x438\x439"},
{"Ukrainian", L"\x423\x43a\x440\x430\x457\x43d\x441\x44c\x43a\x430"},
#pragma setlocale("C")
};
//-------------------------------------------------------------------------
#define sizeA(A) (sizeof(A)/sizeof(*A))

char *lng(int i, char *s)
{
	return (i>=0 && i<sizeA(lngstr) && lngstr[i]) ? lngstr[i] : s;
}

//return pointer to name after a path
char *cutPath(char *s)
{
	char *t;
	t=strchr(s, 0);
	while(t>=s && *t!='\\') t--;
	t++;
	return t;
}

//concatenate current directory and e, write result to fn
void getExeDir(char *fn, char *e)
{
	GetModuleFileName(0, fn, 192);
	strcpy(cutPath(fn), e);
}
//-------------------------------------------------------------------------
static BOOL CALLBACK enumControls(HWND hwnd, LPARAM)
{
	int i=GetDlgCtrlID(hwnd);
	if((i>=300 && i<sizeA(lngstr) || i<11 && i>0) && lngstr[i]){
		SetWindowTextT(hwnd, lngstr[i]);
	}
	return TRUE;
}

void setDlgTexts(HWND hDlg)
{
	EnumChildWindows(hDlg, (WNDENUMPROC)enumControls, 0);
}

void setDlgTexts(HWND hDlg, int id)
{
	char *s=lng(id, 0);
	if(s) SetWindowTextT(hDlg, s);
	setDlgTexts(hDlg);
}

void CodePageToWideChar(char* s)
{
	MultiByteToWideChar(codePage, 0, s, -1, dtBuf, sizeA(dtBuf)-1);
}

void SetWindowTextT(HWND hWnd, char *s)
{
	if(s) {
		if (codePage) {
			CodePageToWideChar(s);
			SetWindowTextW(hWnd, dtBuf);
		} else
			SetWindowTextA(hWnd, s);
	}
}

//reload not modal dialog or create new dialog at position x,y
void changeDialog(HWND &wnd, int x, int y, LPCTSTR dlgTempl, DLGPROC dlgProc)
{
	HWND a, w;

	a=GetActiveWindow();
	w=CreateDialog(inst, dlgTempl, 0, dlgProc);
	if(wnd){
		RECT rc;
		GetWindowRect(wnd, &rc);
		MoveWindow(w, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, FALSE);
		if(IsWindowVisible(wnd)) ShowWindow(w, SW_SHOW);
		DestroyWindow(wnd);
	}
	else{
		SetWindowPos(w, 0, x, y, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
	}
	wnd=w;
	if(a) SetActiveWindow(a);
}
//-------------------------------------------------------------------------
static int *subPtr;

//recurse through menu and change all item names
static void fillPopup(HMENU h)
{
	int i, id, j;
	char *s, *a;
	BOOL u;
	UINT f;
	HMENU sub;
	MENUITEMINFO mii;

	for(i=GetMenuItemCount(h)-1; i>=0; i--){
		id=GetMenuItemID(h, i);
		if(id==29999){
			for(j=0; (a=lngNames[j])!=0; j++){
				if (!_strnicmp(a + 1, "esky", 4) || !_strnicmp(a, "Espa", 4)) continue; //ignore Cesky and Expanol from old version
				f=MF_BYPOSITION|(_stricmp(a, lang) ? 0 : MF_CHECKED);
				u = FALSE;
				if(!isWin9X)
					for(int k=0; k<sizeA(lngInter); k++)
						if(!_stricmp(a, lngInter[k].a)) {
							_snwprintf(dtBuf, sizeA(dtBuf)-1, L"%S (%s)", a, lngInter[k].u);
							InsertMenuW(h, 0xFFFFFFFF, f, 30000+j, dtBuf);
							u = TRUE;
							break;
						}
				if(!u) InsertMenuA(h, 0xFFFFFFFF, f, 30000+j, a);
			}
			DeleteMenu(h, 0, MF_BYPOSITION);
		}
		else{
			if(id<0 || id>=0xffffffff){
				sub=GetSubMenu(h, i);
				if(sub){
					id=*subPtr++;
					fillPopup(sub);
				}
			}
			s=lng(id, 0);
			if(s){
				mii.cbSize=sizeof(MENUITEMINFO);
				mii.fMask=MIIM_TYPE|MIIM_STATE;
				mii.fType=MFT_STRING;
				mii.fState=MFS_ENABLED;
				if (codePage) {
					mii.dwTypeData = reinterpret_cast<LPSTR>(dtBuf);
					CodePageToWideChar(s);
					SetMenuItemInfoW(h, i, TRUE, reinterpret_cast<MENUITEMINFOW*>(&mii));
				}
				else {
					mii.dwTypeData = s;
					mii.cch = (UINT)strlen(s);
					SetMenuItemInfo(h, i, TRUE, &mii);
				}
			}
		}
	}
}

//load menu from resources
//subId are string numbers for submenus
HMENU loadMenu(char *name, int *subId)
{
	HMENU hMenu= LoadMenu(inst, name);
	subPtr=subId;
	fillPopup(hMenu);
	return hMenu;
}

void loadMenu(HWND hwnd, char *name, int *subId)
{
	if(!hwnd) return;
	HMENU m= GetMenu(hwnd);
	SetMenu(hwnd, loadMenu(name, subId));
	DestroyMenu(m);
}
//-------------------------------------------------------------------------
static void parseLng()
{
	char *s, *d, *e;
	int id, err=0, line=1;

	for(s=langFile; *s; s++){
		if(*s==';' || *s=='#' || *s=='\n' || *s=='\r'){
			//comment
		}
		else{
			id=(int)strtol(s, &e, 10);
			if(s==e){
				if(!err) msg(lng(755, "Error in %s\nLine %d"), lang, line);
				err++;
			}
			else if(id<0 || id>=sizeA(lngstr)){
				if(!err) msg(lng(756, "Error in %s\nMessage number %d is too big"), lang, id);
				err++;
			}
			else if(lngstr[id]){
				if(!err) msg(lng(757, "Error in %s\nDuplicated number %d"), lang, id);
				err++;
			}
			else{
				s=e;
				while(*s==' ' || *s=='\t') s++;
				if(*s=='=') s++;
				lngstr[id]=s;
			}
		}
		for(d=s; *s!='\n' && *s!='\r'; s++){
			if(*s=='\\'){
				s++;
				if(*s=='\r'){
					line++;
					if(s[1]=='\n') s++;
					continue;
				}
				else if(*s=='\n'){
					line++;
					continue;
				}
				else if(*s=='0'){
					*s='\0';
				}
				else if(*s=='n'){
					*s='\n';
				}
				else if(*s=='r'){
					*s='\r';
				}
				else if(*s=='t'){
					*s='\t';
				}
			}
			*d++=*s;
		}
		if(*s!='\r' || s[1]!='\n') line++;
		*d='\0';
	}
}
//-------------------------------------------------------------------------
void scanLangDir()
{
	int n;
	HANDLE h;
	WIN32_FIND_DATA fd;
	char buf[256];

	lngNames[0]="English";
	getExeDir(buf, "language\\*.lng");
	h = FindFirstFile(buf, &fd);
	if(h!=INVALID_HANDLE_VALUE){
		n=1;
		do{
			if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
				int len= strleni(fd.cFileName)-4;
				if(len>0){
					char *s= new char[len+1];
					memcpy(s, fd.cFileName, len);
					s[len]='\0';
					lngNames[n++]=s;
				}
			}
		} while(FindNextFile(h, &fd) && n<MAXLANG);
		FindClose(h);
	}
}
//-------------------------------------------------------------------------
static void loadLang()
{
	memset(lngstr, 0, sizeof(lngstr));
	codePage = 0;
	char buf[256];
	GetModuleFileName(0, buf, sizeof(buf)-strleni(lang)-14);
	strcpy(cutPath(buf), "language\\");
	char *fn=strchr(buf, 0);
	strcpy(fn, lang);
	strcat(buf, ".lng");
	HANDLE f=CreateFile(buf, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if(f!=INVALID_HANDLE_VALUE){
		DWORD len=GetFileSize(f, 0);
		if(len>10000000){
			msg(lng(753, "File %s is too long"), fn);
		}
		else{
			delete[] langFile;
			langFile= new char[len+3];
			DWORD r;
			ReadFile(f, langFile, len, &r, 0);
			if(r<len){
				msg(lng(754, "Error reading file %s"), fn);
			}
			else{
				if(langFile[0]=='#' && langFile[1]=='C' && langFile[2]=='P' && !isWin9X){
					codePage=atoi(langFile+3);
					if(codePage == GetACP()) codePage=0;
				}
				langFile[len]='\n';
				langFile[len+1]='\n';
				langFile[len+2]='\0';
				parseLng();
			}
		}
		CloseHandle(f);
	}
}
//---------------------------------------------------------------------------
int setLang(int cmd)
{
	if(cmd>=30000 && cmd<30000+MAXLANG && lngNames[cmd-30000]){
		langChanging();
		strncpy(lang, lngNames[cmd-30000], sizeof(lang)-1);
		loadLang();
		langChanged();
		return 1;
	}
	return 0;
}
//---------------------------------------------------------------------------
void initLang()
{
	OSVERSIONINFO v;
	v.dwOSVersionInfoSize= sizeof(OSVERSIONINFO);
	GetVersionEx(&v);
	isWin9X = v.dwPlatformId==VER_PLATFORM_WIN32_WINDOWS;

	scanLangDir();
	if(!lang[0]){
		//language detection
		const char* s;
		switch(PRIMARYLANGID(GetUserDefaultLangID()))
		{
			case LANG_CATALAN: s="Catalan"; break;
			case LANG_CZECH: s="Czech"; break;
			case LANG_SPANISH: s="Spanish"; break;
			case LANG_FRENCH: s="French"; break;
			case LANG_CHINESE: s="ChineseSimplified";
				if(SUBLANGID(GetUserDefaultLangID())==SUBLANG_CHINESE_TRADITIONAL)
					s= (GetACP() == 950) ? "ChineseTraditionalBig5" : "ChineseTraditionalGBK";
				break;
			case LANG_ITALIAN: s="Italiano"; break;
			case LANG_RUSSIAN: s="Russian"; break;
			default: s="English"; break;
		}
		strncpy(lang, s, sizeof(lang)-1);
	}
	loadLang();
}
//---------------------------------------------------------------------------
#endif
