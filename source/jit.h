/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#ifndef JITH
#define JITH

//#define NOJIT

struct Tjit;
struct Top;

struct Tcompiled {
	int kind;
	union {
		int varIndex;         //jitPushVar (variable index), jitApplyVararg (argument count), jitArrayIdx (index structure flags)
		int length;           //jitPrintText (text length)
	};
	union {
		Complex num;      //jitPushNum (owned copy of a constant)
		const Top *op;    //jitApplyOp, jitApplyVararg, jitFor
		int flags;        //jitCmdEnd (bit0=writeResult, bit1=isPrint), jitPrintText (doubleQuotes)
		int cmdNum;       //jitCmdStart (command number)
	};
	const char *inputPtr; //value for errPos (position of the function name); jitPrintText: text pointer
	Tjit *sub;            //jitIf (2 sub-traces), jitFor (1 sub-trace for the loop body)
};

struct Tjit {
	Darray<Tcompiled> code;
};

struct Tstack {
	const Top *op;
	const char *inputPtr;
};

enum { jitPushNum, jitPushVar, jitApplyOp, jitApplyVararg, jitFor, jitArrayIdx, jitIf,
	jitCmdStart,    // set cmdNum
	jitCmdEnd,      // pop result, write to buf, set ans; flags: bit0=writeResult, bit1=isPrint
	jitPrintText,  // append literal text to buf: inputPtr=text, length=len, flags=doubleQuotes
	jitPrintNewLine, // append \r\n to buf
	jitPrintSpace  // append ' ' to buf
};

const int CMDBASE=3, CMDPOWER=5, CMDPLUS=144, CMDMINUS=143, CMDASSIGN=397,
CMDLEFT=401, CMDGOTO=402, CMDRIGHT=455, CMDEND=460, CMDVARARG=500, CMDFOR=550;

extern Tint precision, prec2;
extern Complex ans, retValue;
extern int gotoPos;
extern int cmdNum;
extern Darray<Complex> numStack;
extern Darray<Tstack> opStack;
extern Darray<const char*> gotoPositions;
extern const Top opPowMod;
extern const char *errPos;
extern Darray<char> *jitBuf;
extern const char *jitParam;
extern int *jitErrIndex;
extern bool jitRecording;

void doOpCore(const Top *o);
void varargApply(const Top *o, unsigned i, const char *opInputPtr);
void arrayIndexApply(int (*D)[2]);
int getIndex();
void forExecute(const Top *o, const char *formula
#ifdef _M_X64
	, Tjit *bodyJit = 0
#endif
);
void skipSpaces(const char *&s);
void cleanup();
void ClearError(int err);
void errGoto();
void checkInfinite(Complex &y);
void deref(Complex &x);
bool deref(Complex &y, Complex &x);
void _fastcall GOTORELX(Pint y);

void jitRun(Tjit *j);
void jitInit(Tjit *j);
void jitFree(Tjit *j);
void jitCompileScript(Tjit *j);
void jitFreeScript(Tjit *j);
void jitCompileFormula(Tjit *j, const char *formula, const char **e);
void jitCompilePushDummy();
void jitCompilePop();
bool jitAppendValue(Complex y);
void jitAppendText(const char *text, int len, int doubleQuotes);
void jitAppendNewLine();
void jitAppendSpace();
Tcompiled *jitRecFor(const Top *o, const char *inputPtr);
Tcompiled *jitRecIf(const char *inputPtr);
void jitRecArrayIndex(int flags, const char *inputPtr);
void jitRecApplyVararg(const Top *o, unsigned argCount, const char *inputPtr);
void jitRecPushNum(const Complex x);
void jitRecPushVar(int index);
void doOp();

const unsigned MAX_OUTPUT_SIZE=1000000000;

#endif
