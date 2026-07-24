/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#ifndef JITH
#define JITH

#define JIT_EMIT 1

struct Tjit;
struct Top;

struct Tcompiled {
	int kind;
	Complex num;            //jitPushNum (owned copy of a constant)
	int varIndex;           //jitPushVar (variable index), jitApplyVararg (argument count), jitArrayIdx (index structure flags)
	const Top *op;          //jitApplyOp, jitApplyVararg, jitEvalArgs, jitIf
	const char *inputPtr;   //value for errPos, jitEvalArgs (position of the function name)
	const char *srcCond, *srcThen, *srcElse;  //jitIf (sources of the arguments)
	Tjit *sub;              //jitIf (3 sub-traces), jitEvalArgs (1 sub-trace for the loop body)
};

struct Tjit {
	Darray<Tcompiled> code;
	bool recorded;
	bool compilable;
	bool ready;
#if JIT_EMIT
	unsigned char *stubMem;
#endif
};

struct TjitPrintPart {
	int kind;
	const char *input;
	const char *text;
	int textLen;
	int doubleQuotes;
	bool spaceAfter;
	Tjit *expr;
	const char *end;
};

struct TjitCommand {
	int kind;
	const char *input;
	const char *end;
	Tjit *expr;
	Darray<TjitPrintPart> parts;
};

struct TjitScript {
	Darray<TjitCommand*> cmds;
	bool ready;
};

struct Tstack {
	const Top *op;
	const char *inputPtr;
};

enum { jitExecNext, jitExecGoto, jitExecStop, jitExecError };
enum { jitPushNum, jitPushVar, jitApplyOp, jitApplyVararg, jitEvalArgs, jitArrayIdx, jitIf };
enum { jitCmdEmpty, jitCmdExpr, jitCmdPrint };
enum { jitPrintText, jitPrintExpr };

const int CMDBASE=3, CMDPOWER=5, CMDPLUS=144, CMDMINUS=143, CMDASSIGN=397,
CMDLEFT=401, CMDGOTO=402, CMDRIGHT=455, CMDEND=460, CMDVARARG=500,
	CMDFOR=550;

extern Tint precision, prec2;
extern Complex ans, oldAns, retValue;
extern int gotoPos;
extern int cmdNum;
extern int inParenthesis;
extern Darray<Complex> numStack;
extern Darray<Tstack> opStack;
extern Darray<const char*> gotoPositions;
extern const Top opPowMod;
extern const char *errPos;

extern bool jitRecording, jitCompileOnly;

void doOpCore(const Top *o);
int argsCore(Tstack stk, const char *input, const char **end
#ifdef _M_X64
	, Tjit *bodyJit = 0
#endif
);
void varargApply(const Top *o, unsigned i, const char *opInputPtr);
void arrayIndexApply(int (*D)[2]);
int getIndex();
void jitEvalFormula(Tjit *j, const char *formula, const char **e);
void skipSpaces(const char *&s);
void copyString(char *dest, const char *src);
void cleanup();
void ClearError(int err);
void errGoto();
void checkInfinite(Complex &y, Tint prec);
void deref(Complex &x);
bool deref(Complex &y, Complex &x);

void jitInit(Tjit *j);
void jitFree(Tjit *j);
bool jitCompileScript(TjitScript *script);
void jitFreeScript(TjitScript *script);
void jitCompileFormula(Tjit *j, const char *formula, const char **e);
void jitCompilePushDummy();
void jitCompilePop();
int jitExecuteExpression(Tjit *j, const char *input, const char *compiledEnd, 
	Darray<char> &buf, bool isPrint, bool writeResult, const char *param, int &errIndex);
bool jitAppendText(Darray<char> &buf, const char *text, int len, int doubleQuotes);
bool jitAppendChar(Darray<char> &buf, char c);
bool jitAppendNewLine(Darray<char> &buf);
int args(const char *input, const char **end);
void jitRecArrayIndex(int flags, const char *inputPtr);
void jitRecApplyVararg(const Top *o, unsigned argCount, const char *inputPtr);
void jitRecPushNum(const Complex x);
void jitRecPushVar(int index);
void doOp();
#endif
