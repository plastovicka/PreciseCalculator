/*
	(C) Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#ifndef CONSOLE
#include "preccalc.h"
#ifndef NDEBUG
#include <stdarg.h>
#endif

#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"version.lib")
#pragma comment(lib,"htmlhelp.lib")

const char *trigB[]={"sin", "cos", "tan", "tg", "cot", "cotg",
"asin", "acos", "atan", "atg", "acot", "acotg",
"arcsin", "arccos", "arctan", "arctg", "arccot", "arccotg", 0};

// custom buttons
const struct TcustTT { const char *name; int trid; const char *tooltip; } custT[]={
	{"EXE", 2078, "Start calculation"},
	{"^2",  2018, "Square power of X"},
	{"^3",  2020, "Cubic power of X"},
	{"10^", 2023, "Power of 10"},
	{".",   2072, "Integer and fractional parts separator"},
	{";",   2073, "Calculations separator"},
	{"E",   2074, "Exponent, power of 10"},
	{"i",   2077, "Imaginary part symbol"},
	{"print", 2090, "Print result to output window"},
	{"Del", 2047, "Delete 1 symbol"},
	{"C",   2048, "Clear all"},
	{":",   2208, "Label mark"},
};

int width=522,
 height=499,
 left=50,
 top=60,
 right,
 bottom,
 xgap=5,
 ygap=5,
 split=350,
 maxHistory=200,
 keyboard=0,
 historyLen,
 idEnter,
 sizeLock,
 autoSave=1,
 logging=0,
 logSize=0,
 tooltipsShow=1;

DWORD macroFileLen, constantsFileLen, unitsFileLen;

char *sepExpr="\r--------------------\r";
char *sepResult="\r =\r";
char *macroFile, *constantsFile, *unitsFile;
static int oldW, oldH;
const int MBUTTONTEXT=32;
UINT inv, hyp;

const bool dual=true; //NOT YET IMPLEMENTED !

bool clearInput,
resizing,
 delreg,
 modif,
 invByShift;

TfileName fnExpr, fnMacro, fnBtn, fnLog;
HWND hWin, hIn, hOut, hTt;
HINSTANCE inst;
HACCEL haccel;
WNDPROC btnProc, editProc;
LOGFONT font;
HFONT hFont, hFontBut;
char *title="Precise Calculator";
char lastMacro[MAX_MACRO_LEN];
char *btnFile;
char ttBuf[256];

struct Tmacro {
	char *name;
	char *content;
};

Darray<Tmacro> macros, constants, units;
List2 history;
TstrItem *curHistory;

struct Tbtn {
	char *f;
	char *invf;
	int ftid, invftid; // funcTab index with description (.descr) and translation (.descrTid)
	int x, y, w, h;
	HWND wnd;
};
Darray<Tbtn> buttons;

COLORREF colors[]={0x000000, 0x600000, 0x0000b0, 0x004090, 0x800080};


const char
*subkey="Software\\Petr Lastovicka\\calc";
struct Treg { char *s; int *i; } regVal[]={
	{"height", &height},
	{"width", &width},
	{"X", &left},
	{"Y", &top},
	{"split", &split},
	{"base", &base},
	{"angleMode", &angleMode},
	{"numFormat", &numFormat},
	{"digits", &digits},
	{"fixDigits", &fixDigits},
	//{"binDigits",&binDigits},
	{"useSep1", &useSeparator1},
	{"useSep2", &useSeparator2},
	{"sepFreq1", &sepFreq1},
	{"sepFreq2", &sepFreq2},
	{"separator1", &separator1},
	{"separator2", &separator2},
	{"fractions", &enableFractions},
	{"history", &maxHistory},
	{"keyboard", &keyboard},
	{"autoSave", &autoSave},
	{"tooltipsShow", &tooltipsShow},
	{"log", &logging},
	{"logSize", &logSize},
	{"disableRounding", &disableRounding},
};
struct Tregs { char *s; char *i; DWORD n; BYTE isPath; } regValS[]={
	{"language", lang, sizeof(lang), 0},
	{"file", fnMacro, sizeof(fnMacro), 1},
	{"buttons", fnBtn, sizeof(fnBtn), 1},
	{"logFile", fnLog, sizeof(fnLog), 1},
};

struct Tregb { TCHAR *s; void *i; DWORD n; } regValB[]={
	{TEXT("font"), &font, sizeof(LOGFONT)},
	{"colors", colors, sizeof(colors)},
};

OPENFILENAME exprOfn={
	sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 1,
	fnExpr, sizeof(fnExpr),
	0, 0, 0, 0, 0, 0, 0, "TXT", 0, 0, 0
};
OPENFILENAME macroOfn={
	sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 1,
	fnMacro, sizeof(fnMacro),
	0, 0, 0, 0, 0, 0, 0, "CAL", 0, 0, 0
};
OPENFILENAME btnOfn={
	sizeof(OPENFILENAME), 0, 0, 0, 0, 0, 1,
	fnBtn, sizeof(fnBtn),
	0, 0, 0, 0, 0, 0, 0, "CBT", 0, 0, 0
};
//-----------------------------------------------------------------
int vmsg(HWND w, char *caption, char *text, int btn, va_list v)
{
	if(!text) return IDCANCEL;
	WCHAR buf[1024];
	CodePageToWideChar(text);
	for (WCHAR* s = dtBuf; *s; s++) {
		if(*s == '%') {
			if(s[1] == 's') s[1] = 'S';
			if(s[1] == 'S') s[1] = 's';
		}
	}		
	_vsnwprintf(buf, sizeof(buf), dtBuf, v);
	buf[sizeA(buf) - 1] = 0;
	CodePageToWideChar(caption);
	return MessageBoxW(w, buf, dtBuf, btn);
}

void msg(char *text, ...)
{
	va_list ap;
	va_start(ap, text);
	vmsg(hWin, title, text, MB_OK|MB_ICONERROR, ap);
	va_end(ap);
}

void msg(HWND w, char *text, ...)
{
	va_list ap;
	va_start(ap, text);
	vmsg(w, title, text, MB_OK|MB_ICONERROR, ap);
	va_end(ap);
}

DWORD getTickCount()
{
	static LARGE_INTEGER freq;
	if(!freq.QuadPart){
		QueryPerformanceFrequency(&freq);
		if(!freq.QuadPart) return GetTickCount();
	}
	LARGE_INTEGER c;
	QueryPerformanceCounter(&c);
	return (DWORD)(c.QuadPart*1000/freq.QuadPart);
}

HANDLE createFile(char *fn, DWORD creation)
{
	HANDLE f=CreateFile(fn, (creation == OPEN_ALWAYS) ? GENERIC_WRITE|GENERIC_READ : GENERIC_WRITE,
		0, 0, creation, 0, 0);
	if(f==INVALID_HANDLE_VALUE){
		if(fn[0]) msg(lng(733, "Cannot create file %s"), fn);
	}
	return f;
}

HANDLE createFile(char *fn)
{
	return createFile(fn, CREATE_ALWAYS);
}

bool openDlg(OPENFILENAME *o)
{
	for(;;){
		o->hwndOwner= hWin;
		o->Flags= OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
		if(GetOpenFileName(o)) return true;  //ok
		if(CommDlgExtendedError()!=FNERR_INVALIDFILENAME
			|| !o->lpstrFile[0]) return false; //cancel
		o->lpstrFile[0]=0;
	}
}

bool saveDlg(OPENFILENAME *o)
{
	for(;;){
		o->hwndOwner= hWin;
		o->Flags= OFN_PATHMUSTEXIST|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT;
		if(GetSaveFileName(o)) return true;  //ok
		if(CommDlgExtendedError()!=FNERR_INVALIDFILENAME
			|| !o->lpstrFile[0]) return false; //cancel
		o->lpstrFile[0]=0;
	}
}

#ifndef NDEBUG
void showx(Pint x)
{
	char *buf= AWRITEX(x, digits);
	msg("%s", buf);
	delete[] buf;
}


const char *logfile="preccalc.log";
static int logLock=0;

void logs(char *fmt, ...)
{
	if(logLock) return;
	va_list ap;
	va_start(ap, fmt);
	if(!fmt){
		remove(logfile);
	}
	else
	{
		FILE *f = fopen(logfile, "a");
		if(f){
			vfprintf(f, fmt, ap);
			fclose(f);
		}
	}
	va_end(ap);
}


void logx(char *msg, Pint x)
{
	if(logLock) return;
	logLock++;
	FILE *f = fopen(logfile, "a");
	if(!x)
		fputs(msg, f);
	else{
		const int d=200;
		digits+=d;
		char *buf= AWRITEX(x, digits);
		digits-=d;
		fprintf(f, "%s=%s;\n", msg, buf);
		delete[] buf;
	}
	fclose(f);
	logLock--;
}
#endif

char *getInput(int *startPos)
{
	TEXTRANGEW tr;
	if(dual){
		tr.chrg.cpMin=0;
		tr.chrg.cpMax=-1;
	}
	else{
		FINDTEXT ft;
		SendMessage(hIn, EM_EXGETSEL, 0, (LPARAM)&ft.chrg);
		ft.chrg.cpMax=0;
		ft.lpstrText=sepExpr;
		int i= (int)SendMessage(hIn, EM_FINDTEXT, FR_MATCHCASE, (LPARAM)&ft);
		if(i<0){
			i=0;
		}
		else{
			i+=strleni(sepExpr);
		}
		tr.chrg.cpMin=i;
		ft.chrg.cpMin=i;
		ft.chrg.cpMax=-1;
		ft.lpstrText=sepExpr;
		int next= (int)SendMessage(hIn, EM_FINDTEXT, FR_DOWN|FR_MATCHCASE, (LPARAM)&ft);
		ft.lpstrText=sepResult;
		int result= (int)SendMessage(hIn, EM_FINDTEXT, FR_DOWN|FR_MATCHCASE, (LPARAM)&ft);
		tr.chrg.cpMax= (result>=0 && (result<next || next<0)) ? result : next;
	}
	if(tr.chrg.cpMax<0) tr.chrg.cpMax=GetWindowTextLengthW(hIn);
	int len = tr.chrg.cpMax - tr.chrg.cpMin + 2; //reserve 1 byte for checkBrackets()
	tr.lpstrText=new WCHAR[len];
	tr.lpstrText[0]=0;
	if(startPos) *startPos=tr.chrg.cpMin;
	SendMessageW(hIn, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
	char* result = new char[len*3];
	WideCharToMultiByte(CP_UTF8, 0, tr.lpstrText, -1, result, len*3, 0, 0);
	delete[] tr.lpstrText;
	return result;
}

void clearhIn()
{
	clearInput=false;
	if(dual){
		SetWindowText(hIn, "");
		if(error) SetWindowText(hOut, "");
	}
	else{
		setSel(-1, 0);
	}
}

void writeOutput(char* s)
{
	SetEditTextUtf8(hOut, s);
}

void paste(char *text)
{
	if(clearInput){
		clearhIn();
		char c=*text;
		if((c=='+' || c=='/' || c=='*' || c=='^') && text[1]==0){
			SendMessage(hIn, EM_REPLACESEL, TRUE, (LPARAM)"Ans");
		}
	}
	SetEditTextUtf8(hIn, text, ST_SELECTION | ST_KEEPUNDO);
}

void focusEXE()
{
	SetFocus(GetDlgItem(hWin, idEnter));
	clearInput=true;
}

void pressEXE()
{
	calc();
	focusEXE();
}

bool isBetweenInOut(LPARAM lP, int &y)
{
	if(!dual) return false;
	int x=(short)LOWORD(lP);
	y=(short)HIWORD(lP);
	RECT rc;
	GetWindowRect(hIn, &rc);
	MapWindowPoints(0, hWin, (POINT*)&rc, 2);
	return y>rc.bottom && y<rc.bottom+9 && x<rc.right;
}

void checkFractButton()
{
	HWND e, c;
	e=GetDlgItem(hWin, IDC_FIXDIGITS);
	c=GetDlgItem(hWin, IDC_FRACT);
	if(numFormat==MODE_FIX){
		HWND w=e; e=c; c=w;
	}
	ShowWindow(c, SW_SHOW);
	ShowWindow(e, SW_HIDE);
	CheckDlgButton(hWin, IDC_FRACT, enableFractions ? BST_CHECKED : BST_UNCHECKED);
}

TstrItem::TstrItem(char *t)
{
	s=new char[strlen(t)+1];
	strcpy(s, t);
	historyLen++;
}

TstrItem::~TstrItem()
{
	delete[] s;
	historyLen--;
}

//-----------------------------------------------------------------
void deleteini()
{
	HKEY key;
	DWORD i;

	delreg=true;
	if(RegDeleteKey(HKEY_CURRENT_USER, subkey)==ERROR_SUCCESS){
		if(RegOpenKey(HKEY_CURRENT_USER,
			"Software\\Petr Lastovicka", &key)==ERROR_SUCCESS){
			i=1;
			RegQueryInfoKey(key, 0, 0, 0, &i, 0, 0, 0, 0, 0, 0, 0);
			RegCloseKey(key);
			if(!i)
				RegDeleteKey(HKEY_CURRENT_USER, "Software\\Petr Lastovicka");
		}
	}
}

void writeini(int force)
{
	HKEY key;
	if(RegCreateKey(HKEY_CURRENT_USER, subkey, &key)!=ERROR_SUCCESS)
		msg(lng(735, "Cannot write to Windows registry"));
	else{
		RegSetValueEx(key, "autoSave", 0, REG_DWORD, (BYTE *)&autoSave, sizeof(int));

		if(force || autoSave){
			for(Treg *u=regVal; u<endA(regVal); u++){
				RegSetValueEx(key, u->s, 0, REG_DWORD,
					(BYTE *)u->i, sizeof(int));
			}
			for(Tregb *w=regValB; w<endA(regValB); w++){
				RegSetValueEx(key, w->s, 0, REG_BINARY, (BYTE *)w->i, w->n);
			}

			TfileName buf;
			getExeDir(buf, "");
			int len = strleni(buf);
			for(Tregs *v=regValS; v<endA(regValS); v++){
				char *s = v->i;
				if(v->isPath && !_strnicmp(buf, v->i, len)) s += len;
				RegSetValueEx(key, v->s, 0, REG_SZ, (BYTE *)s, strleni(s)+1);
			}
		}
		RegCloseKey(key);
	}
}

void readini()
{
	HKEY key;
	DWORD d;

	if(RegOpenKey(HKEY_CURRENT_USER, subkey, &key)==ERROR_SUCCESS){
		for(Treg *u=regVal; u<endA(regVal); u++){
			d=sizeof(int);
			RegQueryValueEx(key, u->s, 0, 0, (BYTE *)u->i, &d);
		}
		for(Tregb *w=regValB; w<endA(regValB); w++){
			d=w->n;
			RegQueryValueEx(key, w->s, 0, 0, (BYTE *)w->i, &d);
		}

		char buf[192+sizeof(TfileName)];
		getExeDir(buf, "");
		int len = strleni(buf);
		for(Tregs *v=regValS; v<endA(regValS); v++){
			d=v->n;
			RegQueryValueEx(key, v->s, 0, 0, (BYTE *)v->i, &d);
			if(v->isPath && v->i[0] && v->i[1]!=':' && v->i[0]!='\\'){
				strcat(buf, v->i);
				lstrcpyn(v->i, buf, v->n);
				buf[len]=0;
			}
		}
		RegCloseKey(key);
	}
}
//-----------------------------------------------------------------
void openExpr()
{
	OPENFILENAME *o= &exprOfn;
	if(!openDlg(o)) return;
	DWORD len;
	char* buf = 0, *n, *start;
	if(loadFile(o->lpstrFile, buf, len, start, 0, true)) {
		for(n=start;; n++){
			n=strchr(n, '[');
			if(!n){ n=start+len; break; }
			if(n[1]=='=' && n[2]==']' && (n==start || n[-1]=='\n' || n[-1]=='\r')) break;
		}
		while(n>start && (n[-1]=='\r' || n[-1]=='\n')) n--;
		*n=0;
		SetWindowText(hOut, "");
		SetWindowTextUtf8(hIn, start);
		delete[] buf;
	}
}

void saveResult(HANDLE f, char *fmt, char *input, char *output, bool truncate)
{
	char *buf, *s;
	DWORD r, w, len, d, n, p;
	const int bufSize=1048576;

	if(f==INVALID_HANDLE_VALUE) return;

	if(truncate && logSize>0){
		aminmax(logSize, 5, 2000000);
		n=logSize*1024;
		d=n/5;
		amax(d, 10000000);
		len=GetFileSize(f, &w);
		if(len>n+d || w>0){
			d=len-n+d;
			SetFilePointer(f, d, 0, FILE_BEGIN);
			buf= new char[bufSize];

			//find expression separator
			ReadFile(f, buf, bufSize-1, &r, 0);
			buf[r]=0;
			s=strstr(buf, "\r\n--------------------\r\n");
			if(s) d += int(s-buf)+2;

			//move from position d to p
			for(p=0; d<len; d+=r, p+=r){
				SetFilePointer(f, d, 0, FILE_BEGIN);
				ReadFile(f, buf, bufSize, &r, 0);
				SetFilePointer(f, p, 0, FILE_BEGIN);
				WriteFile(f, buf, r, &w, 0);
			}
			SetEndOfFile(f);
			delete[] buf;
		}
	}

	if(SetFilePointer(f, 0, NULL, FILE_CURRENT) == 0) 
		WriteFile(f, "\xEF\xBB\xBF", 3, &w, 0); //BOM

	for(s=fmt; *s; s++){
		if(*s=='%'){
			s++;
			if(*s=='1' || *s=='2'){
				buf = (*s=='1') ? input : output;
				WriteFile(f, buf, strleni(buf), &w, 0);
				continue;
			}
			if(!*s) s--;
		}
		WriteFile(f, s, 1, &w, 0);
	}
	CloseHandle(f);
}

char* getEditText(HWND hwnd)
{
	int len = GetWindowTextLengthW(hwnd) + 1;
	WCHAR* buf = new WCHAR[len];
	GetWindowTextW(hwnd, buf, len);
	char* result = new char[len * 3];
	WideCharToMultiByte(CP_UTF8, 0, buf, len, result, len * 3, 0, 0);
	delete[] buf;
	return result;
}

void saveResult()
{
	OPENFILENAME *o= &exprOfn;
	if(!saveDlg(o)) return;

	char* bufIn = getEditText(hIn);
	char* bufOut= getEditText(hOut);
	saveResult(createFile(o->lpstrFile), "%1\r\n[=]\r\n%2", bufIn, bufOut);
	delete[] bufOut;
	delete[] bufIn;
}

//-----------------------------------------------------------------
void delStr(char *s)
{
	if(!macroFile || s<macroFile || s>macroFile+macroFileLen){
		delete[] s;
	}
}

void destroyMacros()
{
	for(Tlen i=macros.len-1; i>=0; i--){
		Tmacro *m= &macros[i];
		delStr(m->name);
		delStr(m->content);
	}
	macros.reset();
}

int findMacro(char *s)
{
	Tlen i;
	for(i=macros.len-1; i>=0; i--){
		if(!strcmp(s, macros[i].name)) break;
	}
	return (int)i;
}

HMENU insertSubmenu(HMENU menu, char *name)
{
	HMENU p;
	CodePageToWideChar(name);

	for(int i=GetMenuItemCount(menu)-1; i>=0; i--){
		p=GetSubMenu(menu, i);
		if(p){
			WCHAR wbuf[64];
			GetMenuStringW(menu, i, wbuf, sizeA(wbuf), MF_BYPOSITION);
			if(!wcscmp(dtBuf, wbuf)) return p;
		}
	}
	p=CreatePopupMenu();
	AppendMenuW(menu, MF_POPUP, (UINT_PTR)p, dtBuf);
	return p;
}

void addMacro(HMENU menu, char *name, int id)
{
	for(char *s=name; *s; s++){
		if(*s=='\\'){
			if(s==name){
				name++;
			}
			else{
				*s=0;
				addMacro(insertSubmenu(menu, name), s+1, id);
				*s='\\';
				return;
			}
		}
	}
	CodePageToWideChar(name);
	AppendMenuW(menu, MF_STRING, id, dtBuf);
}

void initMenu()
{
	int i;
	HMENU h, h1;

	static int subId[]={452, 465, 455, 454, 453, 450, 451};
	loadMenu(hWin, MAKEINTRESOURCE(IDD_MENU), subId);
	h=GetMenu(hWin);
	h1=GetSubMenu(h, GetMenuItemCount(h)-2);
	for(i=0; i<macros.len; i++){
		addMacro(h1, macros[i].name, ID_MACROS+i);
	}
	h1=GetSubMenu(h, GetMenuItemCount(h)-4);
	for(i=0; i<constants.len; i++){
		addMacro(h1, constants[i].name, ID_CONSTANTS+i);
	}
	DeleteMenu(h1, 0, MF_BYPOSITION);
	h1=GetSubMenu(h, GetMenuItemCount(h)-3);
	for(i=0; i<units.len; i++){
		addMacro(h1, units[i].name, ID_UNITS+i);
	}
	DeleteMenu(h1, 0, MF_BYPOSITION);
}

void rdMacro(Darray<Tmacro> &_macros, char *start)
{
	char *s, *t, *n;
	Tmacro *m;

	for(s=start; ;){
		//find left bracket
		for(;;){
			s=strchr(s, '[');
			if(!s) return;
			if(s==start || s[-1]=='\n' || s[-1]=='\r') break;
			s++;
		}
		//trim trailing EOL
		t=s;
		while(t>start && (t[-1]=='\r' || t[-1]=='\n')) t--;
		*t=0;
		//find right bracket
		s++;
		n=s;
		for(;;){
			s=strchr(s, ']');
			if(!s) return;
			if(s[1]=='\r' || s[1]=='\n') break;
			s++;
		}
		*s++=0;
		while(*s=='\r' || *s=='\n') s++;
		//add macro
		m= _macros++;
		m->name=n;
		m->content=s;
	}
}

void rdMacros()
{
	char *s;
	int i, len;
	char* start;

	if(!loadFile(fnMacro, macroFile, macroFileLen, start, destroyMacros)) {
		*fnMacro = 0;
	}else{
		rdMacro(macros, start);
		initMenu();
		modif=false;

		//autostart macro
		for(i=0; i<macros.len; i++){
			Tmacro *m=&macros[i];
			if(!strcmp(m->name, "autostart")){
				len=strleni(m->content)+2;
				s=new char[len];
				strcpy(s, m->content);
				if(len<3 || s[len-3]!=';'){ s[len-2]=';'; s[len-1]=0; }
				calc(s);
				break;
			}
		}
	}
}

void destroyConstants() 
{
	constants.reset();
}

void rdConstants()
{
	char* start;
	TfileName fn;
	getExeDir(fn, lng(10, "constants.cns"));
	if(loadFile(fn, constantsFile, constantsFileLen, start, destroyConstants))
		rdMacro(constants, start);
}

void destroyUnits()
{
	units.reset();
}

void rdUnits()
{
	char* start;
	TfileName fn;
	getExeDir(fn, lng(12, "units.unt"));
	if(loadFile(fn, unitsFile, unitsFileLen, start, destroyUnits))
		rdMacro(units, start);
}

void wrMacro()
{
	int i;
	DWORD w;

	HANDLE f=createFile(fnMacro);
	if(f!=INVALID_HANDLE_VALUE){
		WriteFile(f, "\xEF\xBB\xBF", 3, &w, 0); //BOM
		for(i=0; i<macros.len; i++){
			Tmacro *m=&macros[i];
			WriteFile(f, "\r\n[", 3, &w, 0);
			WriteFile(f, m->name, strleni(m->name), &w, 0);
			WriteFile(f, "]\r\n", 3, &w, 0);
			WriteFile(f, m->content, strleni(m->content), &w, 0);
		}
		CloseHandle(f);
		modif=false;
	}
}

void saveAtExit()
{
	if(modif){
		if(!*fnMacro) saveDlg(&macroOfn);
		wrMacro();
	}
	if(!delreg) writeini(0);
}

int __cdecl cmpMacro(const void *elem1, const void *elem2)
{
	return lstrcmp(((Tmacro*)elem1)->name, ((Tmacro*)elem2)->name);
}

void sortMacros()
{
	qsort(macros.array, macros.len, sizeof(Tmacro), cmpMacro);
	initMenu();
	modif=true;
}

static Tvar **vA;
static int vAlen;

int __cdecl cmpVar(const void *a, const void *b)
{
	return lstrcmp((*(Tvar**)a)->name, (*(Tvar**)b)->name);
}

void initVarList(HWND listBox)
{
	delete[] vA;
	vA= new Tvar*[vAlen=vars.len];
	for(int i=0; i<vAlen; i++) vA[i]= &vars[i];
	qsort(vA, vAlen, sizeof(Tvar*), cmpVar);
	SendMessage(listBox, LB_SETCOUNT, vAlen, 0);
}

static TstrItem **vH;
static int vHlen;

void initHistoryList(HWND listBox)
{
	delete[] vH;
	vH= new TstrItem*[vHlen=history.count()];
	int i=0;
	forl2(TstrItem, history){
		vH[i++] = item;
	}
	SendMessage(listBox, LB_SETCOUNT, vHlen, 0);
}
//-----------------------------------------------------------------

BOOL CALLBACK OptionsProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM)
{
	switch(mesg){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 18);
			CheckDlgButton(hWnd, 2046, tooltipsShow ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, 537, autoSave ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, 530, useSeparator1 ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, 531, useSeparator2 ? BST_CHECKED : BST_UNCHECKED);
			SetDlgItemText(hWnd, 101, (char*)&separator1);
			SetDlgItemText(hWnd, 102, (char*)&separator2);
			SetDlgItemInt(hWnd, 103, sepFreq1, FALSE);
			SetDlgItemInt(hWnd, 104, sepFreq2, FALSE);
			SetDlgItemInt(hWnd, 105, maxHistory, FALSE);
			CheckDlgButton(hWnd, 538, logging ? BST_CHECKED : BST_UNCHECKED);
			SetDlgItemText(hWnd, 106, fnLog);
			SetDlgItemInt(hWnd, 107, logSize, FALSE);
			SendMessage(GetDlgItem(hWnd, 101), EM_SETLIMITTEXT, 1, 0);
			SendMessage(GetDlgItem(hWnd, 102), EM_SETLIMITTEXT, 1, 0);
			SendMessage(GetDlgItem(hWnd, 107), EM_SETLIMITTEXT, 7, 0);
			return 1;

		case WM_COMMAND:
			wP=LOWORD(wP);
			switch(wP){
				case IDOK:
					autoSave = IsDlgButtonChecked(hWnd, 537);
					tooltipsShow = IsDlgButtonChecked(hWnd, 2046);
					if(hTt) SendMessage(hTt, TTM_ACTIVATE, tooltipsShow, 0);
					useSeparator1= IsDlgButtonChecked(hWnd, 530);
					useSeparator2= IsDlgButtonChecked(hWnd, 531);
					GetDlgItemText(hWnd, 101, (char*)&separator1, 2);
					GetDlgItemText(hWnd, 102, (char*)&separator2, 2);
					sepFreq1= GetDlgItemInt(hWnd, 103, 0, FALSE);
					sepFreq2= GetDlgItemInt(hWnd, 104, 0, FALSE);
					maxHistory= GetDlgItemInt(hWnd, 105, 0, FALSE);
					logging= IsDlgButtonChecked(hWnd, 538);
					GetDlgItemText(hWnd, 106, fnLog, sizeA(fnLog));
					logSize= GetDlgItemInt(hWnd, 107, 0, FALSE);
				case IDCANCEL:
					EndDialog(hWnd, wP);
					break;
			}
	}
	return 0;
}

void drawFocus(DRAWITEMSTRUCT *lpdis)
{
	SelectObject(lpdis->hDC, GetStockObject(NULL_BRUSH));
	SelectObject(lpdis->hDC, GetStockObject(WHITE_PEN));
	Rectangle(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top,
		lpdis->rcItem.right, lpdis->rcItem.bottom);
	if(lpdis->itemState & ODS_SELECTED){
		DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
	}
}

//change button position in dialog
void moveX(HDWP p, HWND hDlg, int id, int dx)
{
	RECT rc;
	POINT pt;
	HWND hWnd;

	hWnd=GetDlgItem(hDlg, id);
	GetWindowRect(hWnd, &rc);
	pt.x=rc.left;
	pt.y=rc.top;
	ScreenToClient(hDlg, &pt);
	DeferWindowPos(p, hWnd, 0, pt.x+dx, pt.y,
		0, 0, SWP_NOSIZE|SWP_NOZORDER);
}

void moveW(HDWP p, HWND hDlg, int id, int dx, int dy)
{
	RECT rc;
	HWND hWnd;

	hWnd=GetDlgItem(hDlg, id);
	GetWindowRect(hWnd, &rc);
	DeferWindowPos(p, hWnd, 0, 0, 0,
		rc.right-rc.left+dx, rc.bottom-rc.top+dy,
		SWP_NOMOVE|SWP_NOZORDER);
}

void historySize(HWND hWnd, LPARAM lP, int &oldWidth, int &oldHeight)
{
	if(oldWidth){
		//adjust controls positions
		int dw=LOWORD(lP)-oldWidth;
		int dh=HIWORD(lP)-oldHeight;
		HDWP p = BeginDeferWindowPos(5);
		moveX(p, hWnd, 520, dw);
		moveX(p, hWnd, 521, dw);
		moveX(p, hWnd, 522, dw);
		moveX(p, hWnd, 9, dw);
		moveW(p, hWnd, 101, dw, dh); //listBox
		EndDeferWindowPos(p);
	}
	oldWidth=LOWORD(lP);
	oldHeight=HIWORD(lP);
}

BOOL CALLBACK VarListProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	HWND listBox = GetDlgItem(hWnd, 101);
	Tvar *v;
	char *s;
	int i, item, notif;
	Complex c;
	DRAWITEMSTRUCT *lpdis;
	MEASUREITEMSTRUCT *lpmis;
	TEXTMETRIC tm;
	RECT rc;
	static int oldWidth, oldHeight;

	switch(mesg){
		case WM_INITDIALOG:
			oldWidth=oldHeight=0;
			setDlgTexts(hWnd, 17);
			initVarList(listBox);
			return 1;

		case WM_MEASUREITEM:
			lpmis = (LPMEASUREITEMSTRUCT)lP;
			lpmis->itemHeight = HIWORD(GetDialogBaseUnits())+1;
			return TRUE;

		case WM_DRAWITEM:
			lpdis = (LPDRAWITEMSTRUCT)lP;
			if(lpdis->itemID == -1) break;
			switch(lpdis->itemAction){
				case ODA_DRAWENTIRE:
					v= vA[lpdis->itemID];
					GetTextMetrics(lpdis->hDC, &tm);
					rc.top= lpdis->rcItem.top+1;
					rc.bottom= lpdis->rcItem.bottom;
					rc.left=4;
					rc.right=60;
					Utf8ToWideChar(v->name);
					DrawTextW(lpdis->hDC, dtBuf, -1, &rc, DT_END_ELLIPSIS|DT_NOPREFIX);
					c=ALLOCC(5);
					COPYM(c, v->modif ? v->newx : v->oldx);
					s= AWRITEM(c, isMatrix(c) ? 3 : (!isZero(c.r) && !isZero(c.i) ? 10 : 20), 0);
					FREEM(c);
					rc.left=rc.right+5;
					rc.right=lpdis->rcItem.right;
					Utf8ToWideChar(s);
					DrawTextW(lpdis->hDC, dtBuf, -1, &rc, DT_END_ELLIPSIS|DT_NOPREFIX);
					delete[] s;
				case ODA_SELECT:
					drawFocus(lpdis);
			}
			return TRUE;

		case WM_SIZE:
			if(lP){ historySize(hWnd, lP, oldWidth, oldHeight); InvalidateRect(GetDlgItem(hWnd, 101), 0, TRUE); }
			break;

		case WM_GETMINMAXINFO:
			((MINMAXINFO*)lP)->ptMinTrackSize.x = 300;
			((MINMAXINFO*)lP)->ptMinTrackSize.y = 150;
			break;

		case WM_COMMAND:
			item= (int)SendMessage(listBox, LB_GETCURSEL, 0, 0);
			notif = HIWORD(wP);
			wP=LOWORD(wP);
			switch(wP){
				case 101: //listBox
					if(notif!=LBN_DBLCLK) return FALSE;
					//!
				case 520: //paste
					if(item<0 || item>=vAlen) break;
					paste(vA[item]->name);
					//!
				case IDCANCEL:
				case 9:  //close
					EndDialog(hWnd, wP);
					break;
				case 521: //remove
					if(item<0 || item>=vAlen) break;
					v=vA[item];
					if(stop()) break;
					v->destroy();
					memmove(v, v+1, (&vars[vars.len]-v-1)*sizeof(Tvar));
					vars--;
					initVarList(listBox);
					break;
				case 522: //remove all
					if(stop()) break;
					for(i=0; i<vars.len; i++) vars[i].destroy();
					vars.reset();
					initVarList(listBox);
					break;
			}
			break;
	}
	return FALSE;
}

BOOL CALLBACK HistoryProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	HWND listBox = GetDlgItem(hWnd, 101);
	TstrItem *h;
	int item, notif;
	DRAWITEMSTRUCT *lpdis;
	MEASUREITEMSTRUCT *lpmis;
	TEXTMETRIC tm;
	RECT rc;
	static int oldWidth, oldHeight;

	switch(mesg){
		case WM_INITDIALOG:
			oldWidth=oldHeight=0;
			setDlgTexts(hWnd);
			SetWindowTextT(hWnd, lng(20, "History"));
			initHistoryList(listBox);
			SendMessage(listBox, LB_SETCURSEL, vHlen-1, 0);
			return 1;

		case WM_MEASUREITEM:
			lpmis = (LPMEASUREITEMSTRUCT)lP;
			lpmis->itemHeight = HIWORD(GetDialogBaseUnits())+1;
			return TRUE;

		case WM_DRAWITEM:
			lpdis = (LPDRAWITEMSTRUCT)lP;
			if(lpdis->itemID == -1) break;
			switch(lpdis->itemAction){
				case ODA_DRAWENTIRE:
					h= vH[lpdis->itemID];
					GetTextMetrics(lpdis->hDC, &tm);
					rc.top= lpdis->rcItem.top+1;
					rc.bottom= lpdis->rcItem.bottom;
					rc.left=4;
					rc.right=lpdis->rcItem.right;
					Utf8ToWideChar(h->s);
					DrawTextW(lpdis->hDC, dtBuf, -1, &rc, DT_END_ELLIPSIS|DT_NOPREFIX);
				case ODA_SELECT:
					drawFocus(lpdis);
			}
			return TRUE;

		case WM_SIZE:
			if(lP){ historySize(hWnd, lP, oldWidth, oldHeight); InvalidateRect(GetDlgItem(hWnd, 101), 0, TRUE); }
			break;

		case WM_GETMINMAXINFO:
			((MINMAXINFO*)lP)->ptMinTrackSize.x = 300;
			((MINMAXINFO*)lP)->ptMinTrackSize.y = 150;
			break;

		case WM_COMMAND:
			item= (int)SendMessage(listBox, LB_GETCURSEL, 0, 0);
			notif = HIWORD(wP);
			wP=LOWORD(wP);
			switch(wP){
				case 101: //listBox
					if(notif!=LBN_DBLCLK) return FALSE;
					//!
				case 520: //paste
					if(item<0 || item>=vHlen) break;
					curHistory= vH[item];
					SetEditTextUtf8(hIn, curHistory->s);
					pressEXE();

					//!
				case IDCANCEL:
				case 9:  //close
					EndDialog(hWnd, wP);
					break;
				case 521: //remove
					if(item<0 || item>=vHlen) break;
					h=vH[item];
					if(stop()) break;
					delete h;
					if(h==curHistory) curHistory=0;
					initHistoryList(listBox);
					break;
				case 522: //remove all
					if(stop()) break;
					history.deleteAll();
					curHistory=0;
					initHistoryList(listBox);
					EndDialog(hWnd, wP);
					break;
			}
			break;
	}
	return FALSE;
}

//write space before left brackets that are at the beginning of line
void checkBrackets(char *s)
{
	if(*s=='['){
		memmove(s+1, s, strlen(s)+1);
		*s=' ';
	}
	for(;;){
		s=strchr(s, '[');
		if(!s) break;
		if(s[-1]=='\r' || s[-1]=='\n'){
			s[-1]=' ';
		}
		s++;
	}
}

BOOL CALLBACK NewMacroProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM)
{
	Tmacro *m;
	int i;

	switch(mesg){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 14);
			SetWindowTextT(GetDlgItem(hWnd, 101), lastMacro);
			return 1;
		case WM_COMMAND:
			wP=LOWORD(wP);
			switch(wP){
				case IDOK:
				{
					char* buf = lastMacro;
					GetWindowTextT(GetDlgItem(hWnd, 101), buf, MAX_MACRO_LEN);
					if(*buf){
						i=findMacro(buf);
						if(i>=0){
							m=&macros[i];
							delStr(m->name);
							delStr(m->content);
						}
						else{
							m= macros++;
						}
						m->name= new char[strlen(buf)+1];
						strcpy(m->name, buf);
						m->content= getInput();
						checkBrackets(m->content);
						sortMacros();
					}
				}
				case IDCANCEL:
					EndDialog(hWnd, wP);
			}
			break;
	}
	return FALSE;
}

void initMacroCombo(HWND combo)
{
	for(int m=0; m<macros.len; m++){
		Utf8ToWideChar(macros[m].name);
		SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)dtBuf);
	}
	SendMessage(combo, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)lastMacro);
}

BOOL CALLBACK DeleteMacroProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM)
{
	int m;
	HWND combo=GetDlgItem(hWnd, 101);

	switch(mesg){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 15);
			initMacroCombo(combo);
			return 1;
		case WM_COMMAND:
			wP=LOWORD(wP);
			switch(wP){
				case IDOK:
					char buf[MAX_MACRO_LEN];
					GetWindowTextT(combo, buf, MAX_MACRO_LEN);
					m=findMacro(buf);
					if(m>=0){
						delStr(macros[m].name);
						delStr(macros[m].content);
						memmove(macros+m, macros+m+1, sizeof(Tmacro)*(macros.len-m-1));
						macros--;
						sortMacros();
					}
				case IDCANCEL:
					EndDialog(hWnd, wP);
			}
			break;
	}
	return FALSE;
}

BOOL CALLBACK RenameMacroProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM)
{
	Tmacro *m;
	int i, j, cmd;
	HWND ed2=GetDlgItem(hWnd, 102);
	HWND combo=GetDlgItem(hWnd, 101);

	switch(mesg){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 16);
			initMacroCombo(combo);
			return 1;
		case WM_COMMAND:
			cmd=LOWORD(wP);
			switch(cmd){
				case 101:
					if(HIWORD(wP)==CBN_CLOSEUP){
						GetWindowTextW(combo, dtBuf, MAX_MACRO_LEN);
						SetWindowTextW(ed2, dtBuf);
						SendMessage(ed2, EM_SETSEL, 0, (LPARAM)-1);
						SetFocus(ed2);
					}
					break;
				case IDOK:
				{
					char* buf = lastMacro;
					GetWindowTextT(combo, buf, MAX_MACRO_LEN);
					i=findMacro(buf);
					if(i>=0){
						GetWindowTextT(ed2, buf, MAX_MACRO_LEN);
						j=findMacro(buf);
						if(j>=0){
							WCHAR bufw[MAX_MACRO_LEN];
							GetWindowTextW(ed2, bufw, MAX_MACRO_LEN);
							msg(hWnd, lng(801, "Macro \"%S\" already exists"), bufw);
						}
						else if(*buf){
							m=&macros[i];
							delStr(m->name);
							m->name= new char[strlen(buf)+1];
							strcpy(m->name, buf);
							sortMacros();
						}
					}
				}
				case IDCANCEL:
					EndDialog(hWnd, cmd);
			}
			break;
	}
	return FALSE;
}

VS_FIXEDFILEINFO *getVer()
{
	HRSRC r;
	HGLOBAL h;
	void *s;
	VS_FIXEDFILEINFO *v;
	UINT i;

	r=FindResource(0, (char*)VS_VERSION_INFO, RT_VERSION);
	h=LoadResource(0, r);
	s=LockResource(h);
	if(!s || !VerQueryValue(s, "\\", (void**)&v, &i)) return 0;
	return v;
}

BOOL CALLBACK AboutProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM)
{
	char buf[64];
	VS_FIXEDFILEINFO *v;

	switch(mesg){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 11);
			v=getVer();
			sprintf(buf, HIWORD(v->dwFileVersionLS) ? "%d.%d.%d" : "%d.%d",
				HIWORD(v->dwFileVersionMS), LOWORD(v->dwFileVersionMS), HIWORD(v->dwFileVersionLS));
			SetDlgItemText(hWnd, 101, buf);
			return TRUE;

		case WM_COMMAND:
			int cmd=LOWORD(wP);
			switch(cmd){
				case 123:
				case 124:
					GetDlgItemTextA(hWnd, cmd, buf, sizeA(buf)-13);
					if(cmd==123 && !strcmp(lang, "Czech")) strcat(buf, "/indexCS.html");
					ShellExecuteA(0, 0, buf, 0, 0, SW_SHOWNORMAL);
					break;
				case IDOK: case IDCANCEL:
					EndDialog(hWnd, wP);
			}
			break;
	}
	return 0;
}
//-----------------------------------------------------------------
void colorChanged()
{
	for(int i=0; i<buttons.len; i++){
		InvalidateRect(buttons[i].wnd, 0, TRUE);
	}
}

static COLORREF custom[16];

BOOL CALLBACK ColorProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM lP)
{
	static bool chng;
	static CHOOSECOLOR chc;
	static COLORREF clold[Ncl];
	DRAWITEMSTRUCT *di;
	HBRUSH br;
	int cmd;

	switch(msg){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 19);
			memcpy(clold, colors, sizeof(clold));
			chng=false;
			return TRUE;

		case WM_DRAWITEM:
			di = (DRAWITEMSTRUCT*)lP;
			br= CreateSolidBrush(colors[di->CtlID-100]);
			FillRect(di->hDC, &di->rcItem, br);
			DeleteObject(br);
			break;

		case WM_COMMAND:
			cmd=LOWORD(wP);
			switch(cmd){
				default: //color square
					chc.lStructSize= sizeof(CHOOSECOLOR);
					chc.hwndOwner= hWnd;
					chc.hInstance= 0;
					chc.rgbResult= colors[cmd-100];
					chc.lpCustColors= custom;
					chc.Flags= CC_RGBINIT|CC_FULLOPEN;
					if(ChooseColor(&chc)){
						colors[cmd-100]=chc.rgbResult;
						InvalidateRect(GetDlgItem(hWnd, cmd), 0, TRUE);
						colorChanged();
						chng=true;
					}
					break;
				case IDCANCEL:
					if(chng){
						memcpy(colors, clold, sizeof(clold));
						colorChanged();
					}
					//!
				case IDOK:
					EndDialog(hWnd, cmd);
			}
			break;
	}
	return FALSE;
}

//-----------------------------------------------------------------
static bool IsDefaultMacro;

void langChanging()
{
	TfileName buf;
	getExeDir(buf, lng(21, "examples.cal"));
	IsDefaultMacro = !strcmp(buf, fnMacro);
}

void setOfnFilter(OPENFILENAME& ofn, int id, char* s)
{
	s = lng(id, s);
	char* e;
	for(e = s; *e || e[1]; e++);
	convertCodePage(*(char**)&ofn.lpstrFilter,s, int(e-s)+1, CP_ACP, CP_UTF8);
}

void langChanged()
{
	rdConstants();
	rdUnits();
	initMenu();
	SetWindowTextT(hWin, title= lng(504, "Precise Calculator"));
#if defined(ARIT64) && !defined(NDEBUG)
	SetWindowTextT(hWin, title= "Precise Calculator 64-bit");
#endif
	SetWindowTextT(GetDlgItem(hWin, IDC_SPRECISION), lng(IDC_SPRECISION, "Precision:"));

	setOfnFilter(exprOfn, 508, "Text files (*.txt)\0*.txt\0All files\0*.*\0");
	setOfnFilter(macroOfn, 509, "Macros (*.cal)\0*.cal\0All files\0*.*\0");
	setOfnFilter(btnOfn, 510, "Buttons (*.cbt)\0*.cbt\0All files\0*.*\0");

	if (IsDefaultMacro) {
		getExeDir(fnMacro, lng(21, "examples.cal"));
		if (!modif) rdMacros();
	}
}

void resizeInOut(int y)
{
	RECT rcI, rcO;
	int i, hi, ho;

	GetWindowRect(hIn, &rcI);
	MapWindowPoints(0, hWin, (POINT*)&rcI, 2);
	GetWindowRect(hOut, &rcO);
	MapWindowPoints(0, hWin, (POINT*)&rcO, 2);
	i=rcI.right-rcI.left;
	aminmax(y, rcI.top+20, rcO.bottom-20);
	HDWP p=BeginDeferWindowPos(2);
	hi= y-rcI.top-4;
	DeferWindowPos(p, hIn, 0, 0, 0, i, hi, SWP_NOMOVE|SWP_NOZORDER);
	ho= rcO.bottom-y-4;
	DeferWindowPos(p, hOut, 0, rcO.left, y+4, i, ho, SWP_NOZORDER);
	EndDeferWindowPos(p);
	split= (y-rcI.top)*1000/(rcO.bottom-rcI.top);
}

void getBtnTxt(int i, char *buf, bool shift)
{
	char *s;

	Tbtn *b= &buttons[i];
	s=b->f;
	if((inv || shift) && b->invf && *b->invf) s=b->invf;
	strcpy(buf, s);
	if(hyp){
		for(char **u=(char**)&trigB; *u; u++){
			int len= strleni(*u);
			if(!_strnicmp(s, *u, len) &&
				(s[len]==' ' || s[len]==0 || s[len]=='(')){
				buf[len]= (s[0]>'Z') ? 'h' : 'H';
				strcpy(buf+len+1, s+len);
				if(!_strnicmp(buf, "arc", 3)) buf[2]= (buf[0]>'Z') ? 'g' : 'G';
				break;
			}
		}
	}
}

void getBtnTooltip(int i, BOOL shift)
{
	char *s;
	BOOL h;	// hyperbolic
	int hl;
	Top *t;

	Tbtn *b= &buttons[i];
	if((inv || shift) && b->invf && *b->invf) {
		s=b->invf;
		hl = b->invftid;	// inverse tooltip index
	} else {
		s=b->f;
		hl = b->ftid;	// normal tooltip index
	}
	if(hl>=0) { // function names from funcTab[]
		h = FALSE;
		if(hyp){
			for(char **u=(char**)&trigB; *u; u++){
				int len= strleni(*u);
				if(!_strnicmp(s, *u, len) &&
					(s[len]==' ' || s[len]==0 || s[len]=='(')){
					h = TRUE;
					break;
				}
			}
		}
		t = (Top*)&funcTab[hl];
		if(h) { // hyperbolic toolti]p
			s = lng(2000, "hyperbolic"); // hyperbolic LNG id
			hl = strleni(s);
			strncpy( ttBuf, s, sizeof(ttBuf)-1 );
			if( hl < sizeof(ttBuf)-2 ) { // enough space for at least 1 letter of tooltip
				ttBuf[hl++] = ' ';
				strncpy( ttBuf + hl, lng(t->descrTid, (char*)t->descr), sizeof(ttBuf)-1 - hl );
			}
		} else
			strncpy( ttBuf, lng(t->descrTid, (char*)t->descr), sizeof(ttBuf)-1 );
	} else { // funcTab index<0, custom buttons descriptions
		if( hl > -(int)sizeA(custT)-1 ) {
			hl = -(++hl);
			strncpy( ttBuf, lng(custT[hl].trid, (char*) custT[hl].tooltip), sizeof(ttBuf)-1 );
		} else // unknown (not present in custT[]), don't show tooltip
			*ttBuf = 0;
	}
}

void initButtons()
{
	char buf[MBUTTONTEXT], * s;
	WCHAR buf1[MBUTTONTEXT], buf2[MBUTTONTEXT];

	if(!inv) invByShift=false;

	for(int i=0; i<buttons.len; i++){
		getBtnTxt(i, buf, false);
		s = strchr(buf + 1, '(');
		if(s) *s = 0;
		MultiByteToWideChar(CP_UTF8, 0, buf, -1, buf1, MBUTTONTEXT);
		HWND w= buttons[i].wnd;
		GetWindowTextW(w, buf2, MBUTTONTEXT);
		if(wcscmp(buf1, buf2)) SetWindowTextW(w, buf1);
	}
}

void skipEol(char *&s)
{
	while(*s!='\r' && *s!='\n'){
		if(!*s) return;
		s++;
	}
	if(*s=='\r' && s[1]=='\n') s++;
	s++;
}

int getDescrIndex(char *s)
{
	int i, l;
	char *n;
	if (strlen(s)) { // skipped inv name?
		n = s;
		while( *s==' ' || *s == 9 ) s++; // skip spaces and tabs
		if(!*s) s = n;	// only spaces - "blank" button?
						// leave as is to allow descriptions for '   '-alike buttons
		for(i=0; i<funcTab_size; i++){
			n = (char*) funcTab[i].name;
			l = strleni(n);
			if( !_strnicmp(s, n, l) ) {
				n=(char*)s+l;
				if( *n==' ' || *n==0  || *n=='(' ) {
					while(i>0 && !funcTab[i].descr) i--; // NULL description = previous description
					return i;
				}
			}
		}
		// custom buttons names
		for(i=0; i<sizeA(custT); i++){
			n = (char*) custT[i].name;
			l = strleni(n);
			if( !_strnicmp(s, n, l) ) {
				n=(char*)s+l;
				if( *n==' ' || *n==0  || *n=='(' ) return -(++i);
			}
		}
	}
	return - (int)sizeA(custT) - 1; // non-existent custom or wrong function name
}

void parseButtons(char* s)
{
	char *t;
	int i, j, ver, x0, y0, x, y, w, h, rows, cols;
	Tbtn *b;

	if(sscanf(s, "PRECCALC%d", &ver)!=1){
		msg(lng(731, "Bad format of %s"), fnBtn);
		return;
	}
	skipEol(s);
	right=bottom=0;
	w=46; h=28;
	for(;;){
		cols=rows=1;
		if(sscanf(s, "%d%d ,%d%d ,%d%d", &x0, &y0, &cols, &rows, &w, &h)<2) break;
		skipEol(s);
		aminmax(rows, 1, 100);
		aminmax(cols, 1, 100);
		for(i=0, y=y0; i<rows; i++, y+=h){
			for(j=0, x=x0; j<cols; j++, x+=w){
				b=buttons++;
				b->w=w; b->h=h;
				b->x=x; b->y=y;
				amin(right, x+w);
				amin(bottom, y+h);
				b->f=s;
				// zero terminate button text
				skipEol(s);
				t=s-1;
				if(*t=='\n' && t[-1]=='\r') t--;
				if(int(t-b->f)>=MBUTTONTEXT) t=b->f+MBUTTONTEXT-1;
				*t=0;
				b->ftid = getDescrIndex(b->f);
				b->invf=s;
				// zero terminate button inverse text
				skipEol(s);
				t=s-1;
				if(*t=='\n' && t[-1]=='\r') t--;
				if(int(t-b->invf)>=MBUTTONTEXT) t=b->invf+MBUTTONTEXT-1;
				*t=0;
				b->invftid = getDescrIndex(b->invf);
			}
		}
	}
}

void setBase(int b)
{
	base=b;
	SetDlgItemInt(hWin, IDC_BASE, b, FALSE);
	wrAns();
	if(clearInput) focusEXE();
	else SetFocus(hIn);
}

void setSel(int b, int e)
{
	CHARRANGE p;
	p.cpMin=b;
	p.cpMax=e;
	SendMessage(hIn, EM_EXSETSEL, 0, (LPARAM)&p);
}

int charCount(const char* buf, const char* ptr)
{
	return MultiByteToWideChar(CP_UTF8, 0, buf, int(ptr - buf), 0, 0);
}

void selVar(int n)
{
	char *buf;
	const char *s, *b, *e;
	int startPos;

	buf=getInput(&startPos);
	s=buf;
	while(n--){
		for(;;){
			//find =
			while(*s != '='){
				skipComment(s);
				skipString(s);
				if(!*s) goto e;
				s++;
			}
			//skip <=, >=, !=, ==
			if(s>buf && (s[-1]==' ' || isVarLetter(s[-1])) && s[1]!='=') break;
			s++;
		}
		s++;
	}
	b=s;
	skipArg(s, &e);
	while(e[-1]==' ') e--; //trim trailing spaces
	if(*b=='(' && e[-1]==')') b++, e--; //remove parenthesis
	setSel(startPos + charCount(buf, b), startPos + charCount(buf, e));
	SetFocus(hIn);
e:
	delete[] buf;
}


void bcksp()
{
	int i, m;
	TEXTRANGEW r;
	CHARRANGE p;
	WCHAR c, buf[2];

	SendMessageW(hIn, EM_EXGETSEL, 0, (LPARAM)&p);
	m=p.cpMax;
	if(!m) return;
	for(i=m; i>0; i--){
		r.chrg.cpMin=i-1;
		r.chrg.cpMax=i;
		r.lpstrText=buf;
		SendMessageW(hIn, EM_GETTEXTRANGE, 0, (LPARAM)&r);
		c=buf[0];
		if(!(isLetter((char)c) && c<0x80 || c==' ' && i==m)){
			break;
		}
	}
	if(i==m) i--;
	setSel(i, m);
	SendMessageW(hIn, EM_REPLACESEL, TRUE, (LPARAM)L"");
}

LRESULT CALLBACK enterProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	if(mesg==WM_KEYDOWN && clearInput){
		if(wP==VK_LEFT || wP==VK_UP){
			PostMessage(hWin, WM_INPUTCUR, 0, GetWindowTextLength(hIn));
			clearInput=false;
			return 0;
		}
		if(wP==VK_RIGHT || wP==VK_DOWN){
			PostMessage(hWin, WM_INPUTCUR, 0, 0);
			clearInput=false;
			return 0;
		}
		BYTE state[256];
		GetKeyboardState(state);
		WORD w;
		if(ToAscii((UINT)wP, (lP>>16)&255, state, &w, 0)==1 && (unsigned char)(w)>=' '){
			char buf[6], c;
			c=(char)w;
			buf[0]=c; buf[1]=0;
			paste(buf);
			SetFocus(hIn);
			return 0;
		}
	}
	if(mesg==WM_GETDLGCODE){
		return DLGC_BUTTON|DLGC_WANTARROWS|DLGC_WANTCHARS;
	}
	return CallWindowProcW((WNDPROC)btnProc, hWnd, mesg, wP, lP);
}

LRESULT CALLBACK buttonProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	if(mesg==WM_MOUSEMOVE){
		if(((wP&MK_SHIFT)!=0) == !inv && (invByShift || !inv))
		{
			invByShift=true;
			inv=!inv;
			CheckDlgButton(hWin, IDC_INV, inv ? BST_CHECKED : BST_UNCHECKED);
			initButtons();
		}
	}
	return CallWindowProcW((WNDPROC)btnProc, hWnd, mesg, wP, lP);
}

LRESULT CALLBACK inProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	if(mesg==WM_SETFOCUS) clearInput=false;
	if(mesg==WM_CHAR){
		static WPARAM wP0;
		if(wP==',' && (lP>>16 &255)==83)  wP='.';
		if(!(wP==' ' || isVarLetter((char)wP) && isVarLetter((char)wP0) ||
			(wP>='0' && wP<='9') && (wP0>='0' && wP0<='9'))){
			SendMessage(hWnd, EM_STOPGROUPTYPING, 0, 0);
		}
		wP0=wP;
	}
	return CallWindowProcW((WNDPROC)editProc, hWnd, mesg, wP, lP);
}

HWND createRichEdit(int x, int y, int r, int b, int id)
{
	RECT rc;
	SetRect(&rc, x, y, r, b);
	MapDialogRect(hWin, &rc);
	HWND w=CreateWindowExW(0, RICHEDIT_CLASSW, L"",
		ES_MULTILINE|ES_AUTOVSCROLL|WS_BORDER|WS_VSCROLL|WS_TABSTOP|WS_VISIBLE|WS_CHILD,
		rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top,
		hWin, (HMENU)(UINT_PTR)id, inst, 0);
	SendMessage(w, EM_SETLANGOPTIONS, 0, 0);
	SendMessage(w, EM_SETTEXTMODE, TM_PLAINTEXT|TM_MULTILEVELUNDO, 0);
	SendMessage(w, EM_SETLIMITTEXT, 10000000, 0);
	return w;
}

void destroyButtons()
{
	LockWindowUpdate(hWin);
	//unregister tooltips
	if(hTt) {
		TOOLINFO ti;
		ti.cbSize = sizeof(ti);
		ti.hwnd = hWin;

		for(int i = 0; i < buttons.len; i++) {
			ti.uId = (UINT_PTR)buttons[i].wnd;
			SendMessage(hTt, TTM_DELTOOL, 0, (LPARAM)&ti);
		}
	}
	//delete buttons
	for(int i = 0; i < buttons.len; i++) {
		DestroyWindow(buttons[i].wnd);
	}
	buttons.reset();
}

void loadButtons()
{
	char *fn, buf[256];
	int i, dpix, dpiy;
	HDC dc;
	RECT rc;
	SIZE sz;

	fn=fnBtn;
	if(!*fn){
		fn=buf;
		getExeDir(buf, "buttons\\default.cbt");
	}
	DWORD len;
	char* start;
	if(loadFile(fn, btnFile, len, start, destroyButtons))
	{
		start[len] = '\n';
		start[len + 1] = '\n';
		start[len + 2] = '\0';
		parseButtons(start);

		//resize window
		dc = GetDC(hWin);
		dpix = GetDeviceCaps(dc, LOGPIXELSX);
		dpiy = GetDeviceCaps(dc, LOGPIXELSY);
		GetTextExtentPoint32(dc, "InvHyp", 6, &sz);
		ReleaseDC(hWin, dc);
		GetWindowRect(GetDlgItem(hWin, IDC_FIXDIGITS), &rc);
		MapWindowPoints(0, hWin, (POINT*)&rc, 2);
		GetWindowRect(GetDlgItem(hWin, IDC_INV), &rc);
		MapWindowPoints(0, hWin, (POINT*)&rc, 2);
		rc.top += sz.cy + 5 + ygap;
		amin(height, rc.top + bottom * dpiy / 96 + 7 +
			GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYMENU));
		int dw = rc.left + right * dpix / 96 + 7;
		if(dw > width) oldW = dw;
		sizeLock++;
		SetWindowPos(hWin, 0, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
		sizeLock--;

		TOOLINFO ti;
		ti.cbSize = sizeof(ti);
		ti.uFlags = TTF_IDISHWND; // TTF_IDISHWND | TTF_SUBCLASS;
		ti.hwnd = hWin;
		ti.hinst = inst;
		ti.lpszText = LPSTR_TEXTCALLBACK;

		//create buttons
		for(i = 0; i < buttons.len; i++) {
			Tbtn* b = &buttons[i];
			b->wnd = CreateWindowW(L"BUTTON", L"",
				WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
				rc.left + b->x * dpix / 96, rc.top + b->y * dpiy / 96,
				b->w * dpix / 96 - xgap, b->h * dpiy / 96 - ygap, hWin,
				(HMENU)(UINT_PTR)(300 + i), inst, 0);
			if(hTt) { // register tooltip
				ti.uId = (UINT_PTR)b->wnd;
				SendMessage(hTt, TTM_ADDTOOL, 0, (LPARAM)&ti);
			}
			if(!strcmp(b->f, "EXE")) {
				idEnter = 300 + i;
				btnProc = (WNDPROC)SetWindowLongPtrW(b->wnd, GWLP_WNDPROC, (LONG_PTR)enterProc);
				SendMessage(hWin, DM_SETDEFID, 300 + i, 0);
			}
			else {
				btnProc = (WNDPROC)SetWindowLongPtrW(b->wnd, GWLP_WNDPROC, (LONG_PTR)buttonProc);
			}
			SendMessage(b->wnd, WM_SETFONT, SendMessage(hWin, WM_GETFONT, 0, 0), 0);
		}
		initButtons();
		LockWindowUpdate(0);
	}
}

void setFont()
{
	DeleteObject(hFont);
	hFont=CreateFontIndirect(&font);
	SendMessage(hIn, WM_SETFONT, (WPARAM)hFont, (LPARAM)MAKELPARAM(TRUE, 0));
	SendMessage(hOut, WM_SETFONT, (WPARAM)hFont, (LPARAM)MAKELPARAM(TRUE, 0));
}

UINT_PTR APIENTRY CFHookProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM)
{
	if(message==WM_COMMAND && LOWORD(wParam)==1026){
		SendMessage(hDlg, WM_CHOOSEFONT_GETLOGFONT, 0, (LPARAM)&font);
		setFont();
		return 1;
	}
	return 0;
}

static bool helpVisible;

void showHelp(char *topic)
{
	char buf[MAX_PATH], buf2[MAX_PATH+24];

	getExeDir(buf, lng(13, "preccalc.chm"));
	//if ZIP file has been extracted by Explorer, CHM has internet zone identifier which must be deleted before displaying help
	sprintf(buf2, "%s:Zone.Identifier:$DATA", buf);
	DeleteFile(buf2); //delete only alternate data stream
	if(HtmlHelp(hWin, buf, 0, (DWORD_PTR)topic)) helpVisible=true;
}

void dialogBox(int id, DLGPROC proc)
{
	DialogBoxW(inst, MAKEINTRESOURCEW(id), hWin, (DLGPROC)proc);
}
//-----------------------------------------------------------------
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	int i, y, cmd, c;
	Tbtn *b;
	RECT rc;
	DRAWITEMSTRUCT *dis;
	HWND w;
	HGDIOBJ oldF;
	char *s;
	static const int Mbuf=256;

	switch(mesg){

		case WM_INPUTCUR:
			SetFocus(hIn);
			setSel((int)lP, (int)lP);
			break;
		case WM_GETMINMAXINFO:
			((MINMAXINFO FAR*) lP)->ptMinTrackSize.x = 510;
			((MINMAXINFO FAR*) lP)->ptMinTrackSize.y = 300;
			break;
		case WM_MOVE:
		case WM_EXITSIZEMOVE:
			if(!IsZoomed(hWnd) && !IsIconic(hWnd)){
				GetWindowRect(hWnd, &rc);
				top= rc.top;
				left= rc.left;
				width= rc.right-rc.left;
				height= rc.bottom-rc.top;
			}
			break;
		case WM_MOUSEMOVE:
		{
			bool between=isBetweenInOut(lP, y);
			if(resizing){
				resizeInOut(y);
				between=true;
			}
			SetCursor(LoadCursor(0, between ? IDC_SIZENS : IDC_ARROW));
			break;
		}
		case WM_LBUTTONDOWN:
			if(isBetweenInOut(lP, y)){
				resizing=true;
				SetCapture(hWin);
			}
			break;
		case WM_LBUTTONUP:
			if(resizing){
				resizing=false;
				ReleaseCapture();
			}
			break;
		case WM_QUERYENDSESSION:
			saveAtExit();
			return FALSE;
		case WM_CLOSE:
			saveAtExit();
			DestroyWindow(hWin);
			if(hTt) {
				SendMessage(hTt, TTM_ACTIVATE, FALSE, 0);
				DestroyWindow(hTt);
				hTt = NULL;
			}
			break;
		case WM_DESTROY:
			error=1100;
			PostQuitMessage(0);
			break;

		case WM_NOTIFY:
		{
			LPNMHDR nmhdr = (LPNMHDR)lP;
			if( nmhdr->code == TTN_NEEDTEXTA || nmhdr->code == TTN_NEEDTEXTW ){
				TOOLTIPTEXT *ttt = (LPTOOLTIPTEXT)lP;
				ttt->hinst = NULL;
				i = GetWindowLong( (HWND)ttt->hdr.idFrom, GWL_ID );
				if(i>=300 && i<300+buttons.len){
					getBtnTooltip(i-300, GetKeyState(VK_SHIFT)<0);
					ttt->lpszText = (char *) &ttBuf;
				} else switch (i) {
				case IDC_INV:
					ttt->lpszText = lng(i, "Inverse trigonometric functions");
					break;
				case IDC_HYP:
					ttt->lpszText = lng(i, "Hyperbolic trigonometric functions");
					break;
				case IDC_FRACT:
					ttt->lpszText = lng(i, "Calculate fractions if possible");
					break;
				case IDC_FIXDIGITS:
					ttt->lpszText = lng(i, "Number of fixed digits");
					break;
				}
				if (nmhdr->code == TTN_NEEDTEXTW) {
					CodePageToWideChar(ttt->lpszText);
					ttt->lpszText = reinterpret_cast<LPSTR>(dtBuf);
				}
			}
		}
		break;

		case WM_INITDIALOG:
			hWin= hWnd;
			hIn=createRichEdit(6, 27, 249, 136, IDC_IN);
			hOut=createRichEdit(6, 140, 249, 220, IDC_OUT);
			CheckRadioButton(hWnd, IDC_DEG, IDC_GRAD, IDC_DEG+angleMode);
			CheckRadioButton(hWnd, IDC_SCI, IDC_FIX, IDC_SCI+numFormat);
			SetDlgItemInt(hWnd, IDC_BASE, base, FALSE);
			SetDlgItemInt(hWnd, IDC_PRECISION, digits, FALSE);
			SetDlgItemInt(hWnd, IDC_FIXDIGITS, fixDigits, FALSE);
			checkFractButton();
			editProc = (WNDPROC)SetWindowLongPtrW(GetDlgItem(hWnd, IDC_IN),
				GWLP_WNDPROC, (LONG_PTR)inProc);
			setFont();
			return 1;

		case WM_SIZE:
			if(oldW){
				int dw, dh, last;
				dw=LOWORD(lP);
				dh=HIWORD(lP);
				if(dh==0 || dw<100 && dh<100) break;
				dw-=oldW; dh-=oldH;
				GetWindowRect(GetDlgItem(hWnd, IDC_OUT), &rc);
				i = rc.bottom + dh;
				GetWindowRect(GetDlgItem(hWnd, IDC_IN), &rc);
				i -= rc.top;
				y = i*split/1000;
				aminmax(y, 20, i-20);
				MapWindowPoints(0, hWnd, (POINT*)&rc, 1);
				y += rc.top;
				const int first=291;
				last=300;
				if(!sizeLock) last+=buttons.len;
				HDWP p=BeginDeferWindowPos(last-first);
				for(i=first; i<last; i++){
					w= (i<300) ? GetDlgItem(hWnd, i) : buttons[i-300].wnd;
					GetWindowRect(w, &rc);
					MapWindowPoints(0, hWnd, (POINT*)&rc, 2);
					if(i==IDC_IN){
						DeferWindowPos(p, w, 0, 0, 0, rc.right-rc.left+dw,
							y-rc.top-4, SWP_NOZORDER|SWP_NOMOVE);
					}
					else if(i==IDC_OUT){
						DeferWindowPos(p, w, 0, rc.left, y+4, rc.right-rc.left+dw,
							rc.bottom-y-4+dh, SWP_NOZORDER);
					}
					else{
						DeferWindowPos(p, w, 0, rc.left+dw, rc.top,
							0, 0, SWP_NOZORDER|SWP_NOSIZE);
					}
				}
				EndDeferWindowPos(p);
			}
			oldW=LOWORD(lP);
			oldH=HIWORD(lP);
			break;
		case WM_DRAWITEM:
			dis= (DRAWITEMSTRUCT*)lP;
			{
				WCHAR* buf = (WCHAR*)_alloca(Mbuf * 2);
				GetWindowTextW(dis->hwndItem, buf, Mbuf);
				c = clFunction;
				i = dis->CtlID;
				if(*buf >= '0' && *buf <= '9' && buf[1] == 0
					|| *buf == '.' || *buf == ' ' && buf[1] == 'E') c = clNumber;
				else if(!wcscmp(buf, L"C") || !wcscmp(buf, L"Del")) c = clDelC;
				else if(*buf == '+' || *buf == '-' || *buf == '*' || *buf == '/') c = clOperator;
				else if(!wcscmp(buf, L"EXE")) c = clExe;
				SetTextColor(dis->hDC, colors[c]);
				SetBkMode(dis->hDC, TRANSPARENT);
				oldF = 0;
				if(c == clNumber || c == clOperator) oldF = SelectObject(dis->hDC, hFontBut);
				DrawFrameControl(dis->hDC, &dis->rcItem, DFC_BUTTON,
					DFCS_BUTTONPUSH | (dis->itemState & ODS_SELECTED ? DFCS_PUSHED : 0));
				DrawTextW(dis->hDC, buf, -1,
					&dis->rcItem, DT_CENTER | DT_NOCLIP | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
				if(oldF) SelectObject(dis->hDC, oldF);
			}
			break;
		case WM_SYSCOMMAND:
			if(wP!=SC_RESTORE) return FALSE;
			ShowWindow(hWnd, SW_RESTORE);
			focusEXE();
			break;

		case WM_COMMAND:
			cmd=LOWORD(wP);
			if(setLang(cmd)) break;
			switch(cmd){
				default:
					if(cmd==idEnter){
						nextAns();
						calc();
						clearInput=true;
						break;
					}
					if(cmd>=300 && cmd<300+buttons.len){
						b=&buttons[cmd-300];
						s=b->f;
						if(!strcmp(s, "Del")){
							bcksp();
						}
						else if(!strcmp(s, "C")){
							PostMessage(hWnd, WM_COMMAND, ID_CLEAR, 0);
						}
						else{
							char* buf = (char*)_alloca(MBUTTONTEXT+4);
							getBtnTxt(cmd-300, buf, GetKeyState(VK_SHIFT)<0);
							paste(buf);
						}
						CheckDlgButton(hWnd, IDC_HYP, BST_UNCHECKED);
						hyp=0;
						if(!invByShift){
							CheckDlgButton(hWnd, IDC_INV, BST_UNCHECKED);
							inv=0;
						}
						initButtons();
						SetFocus(hIn);
					}
					if(cmd>=ID_CONSTANTS && cmd<ID_CONSTANTS+constants.len){
						paste(constants[cmd-ID_CONSTANTS].content);
					}
					if(cmd>=ID_UNITS && cmd<ID_UNITS+units.len){
						paste(units[cmd-ID_UNITS].content);
					}
					if(cmd>=ID_MACROS && cmd<ID_MACROS+macros.len){
						bool old=clearInput;
						paste(macros[cmd-ID_MACROS].content);
						strcpy(lastMacro, macros[cmd-ID_MACROS].name);
						clearInput=old;
					}
					break;
				case IDC_DEG:
				case IDC_RAD:
				case IDC_GRAD:
					angleMode= cmd-IDC_DEG;
					CheckRadioButton(hWnd, IDC_DEG, IDC_GRAD, IDC_DEG+angleMode);
					focusEXE();
					break;
				case IDC_NORM:
				case IDC_FIX:
				case IDC_SCI:
				case IDC_ENG:
					numFormat= cmd-IDC_SCI;
					CheckRadioButton(hWnd, IDC_SCI, IDC_FIX, IDC_SCI+numFormat);
					checkFractButton();
					pressEXE();
					break;
				case IDC_DEC:
					setBase(10);
					break;
				case IDC_HEX:
					setBase(16);
					break;
				case IDC_BIN:
					setBase(2);
					break;
				case ID_BASE:
					w=GetDlgItem(hWnd, IDC_BASE);
					SendMessage(w, EM_SETSEL, 0, -1);
					SetFocus(w);
					break;
				case IDC_FRACT:
					enableFractions= !enableFractions;
					checkFractButton();
					pressEXE();
					break;
				case IDC_INV:
				case IDC_HYP:
					inv= IsDlgButtonChecked(hWin, IDC_INV);
					hyp= IsDlgButtonChecked(hWin, IDC_HYP);
					initButtons();
					break;
				case ID_VAR1:
				case ID_VAR2:
				case ID_VAR3:
				case ID_VAR4:
				case ID_VAR5:
				case ID_VAR6:
				case ID_VAR7:
				case ID_VAR8:
				case ID_VAR9:
				case ID_VAR10:
					selVar(cmd-ID_VAR1+1);
					break;
				case ID_VARLIST:
					dialogBox(IDD_VARS, (DLGPROC)VarListProc);
					break;
				case ID_HISTORY:
					dialogBox(IDD_VARS, (DLGPROC)HistoryProc);
					break;
				case ID_OPTIONS:
					dialogBox(IDD_OPTIONS, (DLGPROC)OptionsProc);
					break;
				case ID_CUT:
					SendMessage(hIn, WM_CUT, 0, 0);
					break;
				case ID_COPY:
					SendMessage(hIn, WM_COPY, 0, 0);
					break;
				case ID_PASTE:
					hWnd= GetFocus();
					SendMessage(hWnd==GetDlgItem(hWin, idEnter) ? hIn : hWnd, WM_PASTE, 0, 0);
					break;
				case ID_UNDO:
					SendMessage(hIn, WM_UNDO, 0, 0);
					break;
				case ID_COPYRESULT:
					SendMessage(hOut, EM_SETSEL, 0, -1);
					SendMessage(hOut, WM_COPY, 0, 0);
					SendMessage(hOut, EM_SETSEL, (WPARAM)-1, 0);
					break;
				case ID_CLEAR:
					error=1100;
					focusEXE();
					SetWindowText(hIn, "");
					Sleep(20);
					SetWindowText(hOut, "");
					break;
				case ID_STOP:
					error=1100;
					break;
				case ID_OPEN:
					openExpr();
					break;
				case ID_SAVE:
					saveResult();
					break;
				case ID_OPENBUTTONS:
					if(!openDlg(&btnOfn)) break;
					//!
				case ID_RELOAD_BUTTONS:
					loadButtons();
					if(clearInput) focusEXE();
					break;
				case ID_MACRO_OPEN:
					if(modif) wrMacro();
					if(openDlg(&macroOfn)){
						rdMacros();
					}
					break;
				case ID_MACRO_SAVE:
					if(macros.len && saveDlg(&macroOfn)) wrMacro();
					break;
				case ID_MACRO_NEW:
					if(!GetWindowTextLength(hIn)){
						msg(lng(802, "At first, type some expression to the input edit box"));
					}
					else{
						dialogBox(IDD_NEWMACRO, (DLGPROC)NewMacroProc);
					}
					break;
				case ID_MACRO_DEL:
					dialogBox(IDD_DELETEMACRO, (DLGPROC)DeleteMacroProc);
					break;
				case ID_MACRO_RENAME:
					dialogBox(IDD_RENAMEMACRO, (DLGPROC)RenameMacroProc);
					break;

				case ID_HELP:
					showHelp("html/reference.htm");
					break;
				case ID_HELP_CONTENT:
					showHelp(0);
					break;
				case ID_ABOUT:
					dialogBox(IDD_ABOUT, (DLGPROC)AboutProc);
					break;
				case ID_EXIT:
					SendMessage(hWin, WM_CLOSE, 0, 0);
					break;
				case ID_WRINI:
					digits= GetDlgItemInt(hWin, IDC_PRECISION, 0, FALSE);
					fixDigits= GetDlgItemInt(hWin, IDC_FIXDIGITS, 0, FALSE);
					writeini(1);
					break;
				case ID_DELINI:
					if(MessageBox(hWnd, lng(736, "Do you want to delete your settings ?"),
						title, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) ==IDYES){
						deleteini();
					}
					break;
				case ID_FONT:
				{
					static CHOOSEFONT f;
					f.lStructSize=sizeof(CHOOSEFONT);
					f.hwndOwner=hWin;
					f.Flags=CF_SCREENFONTS|CF_INITTOLOGFONTSTRUCT|CF_SCRIPTSONLY|CF_ENABLEHOOK|CF_APPLY;
					f.lpLogFont=&font;
					f.lpfnHook=CFHookProc;
					if(ChooseFont(&f)) setFont();
				}
					break;
				case ID_COLORS:
					dialogBox(IDD_COLORS, (DLGPROC)ColorProc);
					break;
				case ID_HIST_PREV:
					if(!history.isEmpty()){
						if(!curHistory) curHistory=(TstrItem*)history.last();
						curHistory=(TstrItem*)curHistory->prv;
						if((NxtPrv*)curHistory==&history) curHistory=(TstrItem*)history.first();
						SetEditTextUtf8(hIn, curHistory->s);
					}
					break;
				case ID_HIST_NEXT:
					if(!history.isEmpty()){
						if(!curHistory) curHistory=(TstrItem*)history.first();
						else curHistory=(TstrItem*)curHistory->nxt;
						if((NxtPrv*)curHistory==&history) curHistory=(TstrItem*)history.last();
						SetEditTextUtf8(hIn, curHistory->s);
					}
					break;
			}
			break;

		default:
			return FALSE;
	}
	return TRUE;
}
//-----------------------------------------------------------------
void processMessage(MSG &mesg)
{
	if(TranslateAcceleratorW(hWin, haccel, &mesg)==0){
		if(clearInput && (mesg.wParam==VK_LEFT || mesg.wParam==VK_RIGHT) && mesg.message==WM_KEYDOWN
			|| !IsDialogMessageW(hWin, &mesg)){
			TranslateMessage(&mesg);
			DispatchMessageW(&mesg);
		}
	}
}

int pascal WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	inst=hInstance;

	//DPIAware
	typedef BOOL(WINAPI *TSetProcessDPIAware)();
	TSetProcessDPIAware pDPIAware = (TSetProcessDPIAware)GetProcAddress(GetModuleHandleA("user32"), "SetProcessDPIAware");
	if(pDPIAware) pDPIAware();

	initFuncTab();
	ans=ALLOCC(1);

	font.lfHeight=-18;
	font.lfWeight=FW_NORMAL;
	font.lfCharSet=DEFAULT_CHARSET;
	strcpy(font.lfFaceName, "Arial");
	hFontBut=CreateFontIndirect(&font);
	font.lfHeight=-12;

	readini();
	initLang();
	if(!*fnMacro) getExeDir(fnMacro, lng(21, "examples.cal"));

	int w=GetSystemMetrics(SM_CXSCREEN);
	int h=GetSystemMetrics(SM_CYSCREEN);
	aminmax(left, 0, w-100);
	aminmax(top, 0, h-100);
	aminmax(width, 300, w);
	aminmax(height, 200, h-16);

	WNDCLASSW wc;
	wc.style=0;
	wc.lpfnWndProc=(WNDPROC)DefDlgProcW;
	wc.cbClsExtra=0;
	wc.cbWndExtra=DLGWINDOWEXTRA;
	wc.hInstance=hInstance;
	wc.hIcon=LoadIcon(inst, MAKEINTRESOURCE(1));
	wc.hCursor=0;
	wc.hbrBackground=(HBRUSH)COLOR_BTNFACE;
	wc.lpszMenuName=MAKEINTRESOURCEW(IDD_MENU);
	wc.lpszClassName=L"PreciseCalculator";
	if(!RegisterClassW(&wc)){ msg("RegisterClass error"); return 2; }
	HMODULE richLib=LoadLibrary("riched20.dll");
	if(!richLib){ msg("Cannot find RICHED20.DLL"); return 4; }
	CreateDialogW(hInstance, MAKEINTRESOURCEW(IDD_MAIN), 0, (DLGPROC)MainWndProc);
	if(!hWin){ msg("CreateDialog error"); return 3; }

	RECT rc;
	GetClientRect(hWin, &rc);
	oldW= rc.right-rc.left;
	oldH= rc.bottom-rc.top;
	MoveWindow(hWin, left, top, width, height, FALSE);

	// creating tooltip window
	INITCOMMONCONTROLSEX iccs;
	iccs.dwSize= sizeof(INITCOMMONCONTROLSEX);
	iccs.dwICC= ICC_TAB_CLASSES;
	InitCommonControlsEx(&iccs);

	hTt = CreateWindowExW(0, TOOLTIPS_CLASSW, NULL, WS_DISABLED, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hWin, NULL, inst, 0);
	// registering tooltips for permanent controls
	TOOLINFO ti;
	ti.cbSize = sizeof(ti);
	ti.uFlags = TTF_IDISHWND; // TTF_IDISHWND | TTF_SUBCLASS;
	ti.hwnd = hWin;
	ti.hinst = inst;
	ti.lpszText = LPSTR_TEXTCALLBACK;
	static int toolId[]={IDC_FRACT, IDC_INV, IDC_HYP, IDC_FIXDIGITS, 0};
	for (int* i=toolId; *i; i++) {
		ti.uId = (UINT_PTR)GetDlgItem(hWin, *i);
		SendMessage(hTt, TTM_ADDTOOL, 0, (LPARAM)&ti);
	}
	// turning off tooltips if they are disabled in settings
	if(hTt) SendMessage(hTt, TTM_ACTIVATE, tooltipsShow, 0);

	loadButtons();
	ShowWindow(hWin, SW_SHOWDEFAULT);

	rdMacros();
	langChanged();
	haccel=LoadAccelerators(hInstance, MAKEINTRESOURCE(3));
	focusEXE();

	UpdateWindow(hWin);

	if(keyboard){ //locale identifier, can be set only in the registry
		char buf[10];
		sprintf(buf, "%08x", keyboard);
		LoadKeyboardLayout(buf, KLF_ACTIVATE|KLF_SUBSTITUTE_OK);
	}

	MSG mesg;
	while(GetMessageW(&mesg, NULL, 0, 0)>0){
		if(hTt) SendMessageW(hTt, TTM_RELAYEVENT, 0, (LPARAM)&mesg);
		processMessage(mesg);
	}
	DeleteObject(hFont);
	FreeLibrary(richLib);

	//HtmlHelp bug workaround, process freezes in itss.dll CITUnknown::CloseActiveObjects() if help is visible and user closes both windows from the taskbar
	if(helpVisible) Sleep(600);
	return 0;
}
//-----------------------------------------------------------------
#endif
