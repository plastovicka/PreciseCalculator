/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#ifndef JITH
#define JITH

struct Tjit;
struct Top;

struct Tcompiled {
	int kind;
	union {
		int variable;     //jitPushVar (variable index)
		int indexes;	  //jitArrayIdx (index structure flags)
		unsigned argCount;//jitApplyVararg (argument count)
		int length;       //jitPrintText (text length)
		int base;         //jitPushNum (base)
	};
	union {
		Pint num;         //jitPushNum (owned copy of a constant)
		Tint integer;     //jitPushInt (integer constant), jitPushFraction (numerator)
		const Top *op;    //jitApplyOp, jitApplyVararg, jitFor
		int flags;        //jitCmdEnd (writeResult), jitPrintText (doubleQuotes)
		int cmdNum;       //jitCmdStart (command number)
	};
	const char *inputPtr; //value for errPos (position of the function name); jitPrintText: text pointer
	union {
		Tjit *sub;        //jitIf (2 sub-traces), jitFor (1 sub-trace for the loop body)
		Tuint fraction;   //jitPushFraction (denominator)
	};
};

struct Tjit {
	Darray<Tcompiled> code;
};

struct Tstack {
	const Top *op;
	const char *inputPtr;
};

enum { jitPushNum, jitPushInt, jitPushFraction, jitPushVar,
	jitApplyOp, jitApplyVararg, jitFor, jitArrayIdx, jitIf,
	jitCmdStart,    // set cmdNum
	jitCmdEnd,      // pop result, write to buf, set ans; flags: writeResult
	jitPrintText,  // append literal text to buf: inputPtr=text, length=len, flags=doubleQuotes
	jitPrintNewLine, // append \r\n to buf
	jitPrintSpace  // append ' ' to buf
};

const int CMDBASE=3, CMDPOWER=5, CMDPLUS=144, CMDMINUS=143, CMDASSIGN=397,
CMDLEFT=401, CMDGOTO=402, CMDRIGHT=455, CMDEND=460, CMDVARARG=500, CMDFOR=550;

extern Tint precision, prec2;
extern Complex ans, retValue;
extern int gotoPos, cmdNum;
extern Darray<Complex> numStack;
extern Darray<Tstack> opStack;
extern const Top opPowMod;
extern const char *errPos;
extern Darray<char> outBuf;

void skipSpaces(const char *&s);
void cleanup();
void ClearError(int err);
void errGoto();
void deref(Complex &x);
bool deref(Complex &y, Complex &x);

void jitScriptRun(Tjit *j);
void jitRun(Tjit *j);
void jitFree(Tjit *j);
void jitCompileScript(Tjit *j, const char *input);
void jitUpdateNumbers(Tjit *j);
void jitCompileFormula(Tjit *j, const char *formula, const char **e);
void jitCompilePushDummy();
void jitCompilePop();
Tcompiled *jitEmit(int kind);
void doOp();

void _stdcall INCC(Complex y, const Complex a);
void _stdcall DECC(Complex y, const Complex a);
void _fastcall GOTORELX(Pint y);
void FILTERM();

#endif
