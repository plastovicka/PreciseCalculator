/*
 (C) 2004-2011  Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
*/

#ifndef PRECCALCH
#define PRECCALCH

#include "resource.h"
#include "lang.h"
#include "darray.h"
#include "list2.h"
#include "matrix.h"

struct Tvar {
  char *name;
  Complex oldx,newx;
  bool modif;
  void destroy();
};

struct TstrItem : public NxtPrv {
  char *s;
  TstrItem(char *t);
  ~TstrItem();
};

struct Tfunc {
  char *name;
  char *body;
  Darray<char *> args;
  void destroy();
};

enum { clNumber, clFunction, clDelC, clOperator, clExe, Ncl };

#define WM_INPUTCUR WM_APP+1005
#define ID_MACROS 5000
#define ID_CONSTANTS 6000
#define ID_UNITS 7000
#define MAX_MACRO_LEN 32

#define sizeA(a) (sizeof(a)/sizeof(*a))
#define endA(a) (a+(sizeof(a)/sizeof(*a)))

template <class T> inline void amax(T &x,int h){
 if(int(x)>h) x=h;
}

template <class T> inline void amin(T &x,int l){
 if(int(x)<l) x=l;
}

extern "C"{
HWND WINAPI HtmlHelpA(HWND hwndCaller,LPCSTR pszFile,UINT uCommand,DWORD_PTR dwData);
HWND WINAPI HtmlHelpW(HWND hwndCaller,LPCWSTR pszFile,UINT uCommand,DWORD_PTR dwData);
#ifdef UNICODE
#define HtmlHelp  HtmlHelpW
#else
#define HtmlHelp  HtmlHelpA
#endif
}

int vmsg(char *caption, char *text, int btn, va_list v);
void processMessage(MSG &mesg);
void calc();
void calc(char *input);
void nextAns();
int stop();
void wrAns();
bool isVarLetter(char c);
bool isLetter(char c);
void setSel(int b, int e);
char *getIn(int *startPos=0);
void _stdcall ASSIGNM(Complex y,const Complex a,const Complex x);
void parse(const char *input, const char **e);
DWORD getTickCount();
void initFuncTab();
HANDLE createFile(char *fn, DWORD creation);
void saveResult(HANDLE f, char *fmt, char *input, char *output, bool truncate=false);
int skipString(const char *&s);
bool skipComment(const char *&s);
void skipArg(const char *s, const char **e);

extern const bool dual;
extern char *title;
extern int digits,numFormat,base,maxHistory,historyLen,matrixFormat,log;
extern Complex ans;
extern Darray<Tvar> vars;
extern List2 history;
extern TstrItem *curHistory;
extern HWND hWin,hIn,hOut;
typedef char TfileName[MAX_PATH];
extern TfileName fnLog;


inline Tvar *toVariable(const Complex x){
  return &vars[x.r[0]];
}

#endif
