/*
	(C) Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "preccalc.h"
#include "jit.h"

/*
 Just-in-time compiler
 The first iteration runs the interpreter with recording enabled and
 captures the linear sequence of stack operations (push number, push variable,
 apply operator, apply function to counted arguments, array index). That
 trace is emitted as a call-sequence which invokes the existing
 math routines through jitRun.
 if(cond,a,b) is compiled as a jitIf instruction whose condition and branches
 are separate sub-traces, so both branches end up compiled even
 when the condition changes between iterations. Nested for/foreach constructs
 are recorded as one jitFor instruction whose loop header expressions
 and body are persistent sub-traces compiled only once. Commands goto and
 gotor set gotoPos just like the interpreter.
*/

Darray<char> *jitBuf;

bool jitAppendValue(Complex y)
{
	Darray<char> &buf = *jitBuf;
	int n=buf.len;
	int digits2= (precision>=prec2) ? digits : int((precision-2)*dwordDigits[base]+1);
	int len = LENM(y, digits2);
	char *a= (buf+=len)-1;
	if((unsigned)buf.len>MAX_OUTPUT_SIZE || !len){
		buf.len=n;
		cerror(1062, "Result is too long");
		return false;
	}
	WRITEM(a, y, digits2, matrixFormat);
	buf.setLen(n+strleni(a));
	return true;
}

//convert double qoutes to single quote
//result is not null-terminated !
static void copyString(char *dest, const char *src)
{
	for(;;){
		char c = *src++;
		if(c=='\"')
		{
			if(*src!='\"') break;
			src++;
		}
		*dest++ = c;
	}
}

void jitAppendText(const char *text, int len, int doubleQuotes)
{
	Darray<char> &buf = *jitBuf;
	char *a=(buf+=len)-1;
	if((unsigned)buf.len > MAX_OUTPUT_SIZE){
		buf-=len;
		cerror(1062, "Result is too long");
		return;
	}
	if(doubleQuotes>0) copyString(a, text);
	else memcpy(a, text, len);
	a[len]=0;
}

void jitAppendSpace()
{
	Darray<char> &buf = *jitBuf;
	char *a=(buf++);
	if((unsigned)buf.len > MAX_OUTPUT_SIZE){
		buf--;
		cerror(1062, "Result is too long");
		return;
	}
	a[-1]=' ';
	a[0]=0;
}

void jitAppendNewLine()
{
	Darray<char> &buf = *jitBuf;
	char *a=(buf+=2);
	if((unsigned)buf.len > MAX_OUTPUT_SIZE){
		buf-=2;
		cerror(1062, "Result is too long");
		return;
	}
	a[-1]='\r';
	a[0]='\n';
	a[1]=0;
}
//---------------------------------------------------------------
#ifdef _M_X64
bool jitRecording=false;
static Tjit *jitCur;
static Darray<Tlen> cmdStart;
const char *jitParam=0;
int *jitErrIndex=0;

void jitRun(Tjit *j)
{
#if JIT_EMIT
	if(jitEmit(j)) return;
	//fallback when executable memory could not be allocated
#endif
	for(Tcompiled *ins=j->code.array, *e = ins + j->code.len; ins < e && !error; )
	{
		//execute one instruction
		switch(ins->kind){
		case jitApplyOp:
			errPos= ins->inputPtr;
			doOpCore(ins->op);
			break;
		case jitPushNum: {
			Complex x;
			if(ins->num) {
				x = ALLOCC(precision);
				COPYX(x.r, ins->num);
			}
			else x.r = x.i = 0;
			*numStack++ = x;
			break;
		}
		case jitPushInt: {
			Complex x = ALLOCC(precision);
			SETXN(x.r, ins->integer);
			*numStack++ = x;
			break;
		}
		case jitPushFraction: {
			Complex x = ALLOCC(precision);
			x.r[0]= ins->integer;
			x.r[1]= ins->fraction;
			x.r[-1] = (Tuint)ins->integer >= ins->fraction;
			x.r[-3] = -2;
			*numStack++ = x;
			break;
		}
		case jitPushVar:{
			Complex x= ALLOCC(precision);
			x.r[0]= ins->varIndex;
			x.r[-3]= -1;
			*numStack++= x;
			break;
		}
		case jitApplyVararg:
			varargApply(ins->op, (unsigned)ins->varIndex, ins->inputPtr);
			break;
		case jitFor:{
			//re-execute a construct with repeated argument evaluation
			//(for, foreach, integral, ...) using persistent compiled sub-trace
			forExecute(ins->op, ins->inputPtr+strlen(ins->op->name), ins->sub);
			break;
		}
		case jitIf: {
			errPos = ins->inputPtr;
			deref(numStack[numStack.len-1]);
			Complex y = *numStack--;
			bool cond = !isZero(y);
			FREEM(y);
			jitRun(&ins->sub[cond ? 0 : 1]);
			break;
		}
		case jitArrayIdx:{
			//pop the recorded index expressions in reverse order of evaluation
			int D[2][2];
			int f= ins->varIndex;
			errPos= ins->inputPtr;
			D[0][0]=D[0][1]=D[1][0]=D[1][1]=-1;
			if(f&8) D[1][1]= getIndex();
			if(f&4){ D[1][0]= getIndex(); if(!(f&8)) D[1][1]= D[1][0]; }
			if(f&2) D[0][1]= getIndex();
			if(f&1){ D[0][0]= getIndex(); if(!(f&2)) D[0][1]= D[0][0]; }
			arrayIndexApply(D);
			break;
		}
		case jitCmdStart:
			// set current command number for gotor
			cmdNum= ins->cmdNum;
			break;
		case jitCmdEnd:{
			// finish an expression command: check errors, write result, set ans
			// flags: bit0=writeResult
			if(numStack.len!=1){ cerror(1029, "Fatal error"); break; }
			if(gotoPos>=0){
				cleanup();
				if(gotoPos>=cmdStart.len){
					errGoto();
				}
				else{
					// goto: find the target command's start index
					ins= j->code.array + cmdStart[gotoPos];
					gotoPos=-1;
					continue;
				}
			}
			if(error){
				if(jitErrIndex) *jitErrIndex=int(errPos-jitParam);
				break;
			}
			Complex y2= *numStack--;
			deref(y2);
			checkInfinite(y2);
			if(error){ FREEM(y2); break; }
			if((ins->flags & 1) != 0 && !jitAppendValue(y2)){
				FREEM(y2); break;
			}
			FREEM(ans);
			ans=y2;
			break;
		}
		case jitPrintText:
			jitAppendText(ins->inputPtr, ins->length, ins->flags);
			break;
		case jitPrintNewLine:
			jitAppendNewLine();
			break;
		case jitPrintSpace:
			jitAppendSpace();
			break;
		}
		ins++;
	}
}

void jitInit(Tjit *j)
{
	j->code.reset();
}

void jitFree(Tjit *j)
{
	for(Tlen k=0; k<j->code.len; k++){
		Tcompiled &c= j->code.array[k];
		if(c.kind==jitPushNum){
			FREEX(c.num);
		}
		else if(c.kind==jitIf && c.sub){
			for(int m=0; m<2; m++) jitFree(&c.sub[m]);
			delete[] c.sub;
		}
		else if(c.kind==jitFor && c.sub){
			jitFree(&c.sub[0]);
			delete[] c.sub;
		}
	}
	j->code.reset();
}

//---------------------------------------------------------------

void jitCompilePushDummy()
{
	*numStack++ = {0, 0};
}

void jitCompilePop()
{
	if(numStack.len){
		FREEM(*numStack--);
	}
}

static void jitRestoreCompileStack(Tlen savedNumStack, Tlen savedOpStack)
{
	while(numStack.len>savedNumStack){
		FREEM(*numStack--);
	}
	opStack.len=savedOpStack;
}

static void jitCompileSimOp(const Top *o)
{
	int i=o->type;
	if(i==1){
		jitCompilePushDummy();
	}
	else if(numStack.len>0 && (o->func || o->cfunc || o->mfunc)){
		if(i==8 || i==2 || i==9 || i>=400){
			//unary operators update the stack item in place
		}
		else if(o==&opPowMod){
			jitCompilePop();
			jitCompilePop();
		}
		else{
			//binary operator
			jitCompilePop();
			if(i == CMDBASE) baseIn = (int)numStack[numStack.len-1].r[0];
		}
	}
}

void jitRecPushNum(const Complex x, const char *inputPtr)
{
	Tcompiled *c= jitCur->code++;
	if(isInt(x)) {
		c->kind = jitPushInt;
		c->integer = toInt(x.r);
		FREEM(x);
	}
	else if(isFraction(x.r) && !isImag(x) && x.r[-2]==0) {
		c->kind = jitPushFraction;
		c->integer = x.r[0];
		c->fraction = x.r[1];
		FREEM(x);
	}
	else {
		c->kind = jitPushNum;
		c->num = x.r;
		c->inputPtr= inputPtr;
		c->base = baseIn;
		FREEX(x.i);
	}
	numStack[numStack.len-1] = {0, 0};
}

void jitRecPushVar(int index)
{
	Tcompiled *c= jitCur->code++;
	c->kind= jitPushVar;
	c->varIndex= index;
}

void jitRecApplyOp(const Top *o, const char *inputPtr)
{
	if(o->type==CMDBASE) return;
	if(o->func==GOTORELX) {
		//relative goto, record the current command number
		Tcompiled *c = jitCur->code++;
		c->kind = jitCmdStart;
		c->cmdNum = cmdNum;
	}
	Tcompiled *c= jitCur->code++;
	c->kind= jitApplyOp;
	c->op= o;
	c->inputPtr= inputPtr;
}

void jitRecApplyVararg(const Top *o, unsigned argCount, const char *inputPtr)
{
	Tcompiled *c= jitCur->code++;
	c->kind= jitApplyVararg;
	c->op= o;
	c->varIndex= (int)argCount;
	c->inputPtr= inputPtr;
}

//record a construct whose arguments are evaluated conditionally or repeatedly
//(for, foreach, integral, ...); the loop body is
//persistent sub-trace compiled once and replayed many times
Tcompiled *jitRecFor(const Top *o, const char *inputPtr)
{
	Tcompiled *c= jitCur->code++;
	c->kind= jitFor;
	c->op= o;
	c->inputPtr= inputPtr;
	c->sub= new Tjit[1];
	jitInit(&c->sub[0]);
	return c;
}

void jitRecArrayIndex(int flags, const char *inputPtr)
{
	Tcompiled *c= jitCur->code++;
	c->kind= jitArrayIdx;
	c->varIndex= flags;
	c->inputPtr= inputPtr;
}

#if JIT_EMIT
#error goto not implemented in jitEmit yet
//emit a native x64 stub that calls jitStep once per recorded instruction
static bool jitEmit(Tjit *j)
{
	Tlen n= j->code.len;
	Tcompiled *codeBase= j->code.array;
	SIZE_T size= 15 + (SIZE_T)12*n + 6;
	unsigned char *code= (unsigned char*)VirtualAlloc(0, size, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if(!code) return false;
	unsigned char *p= code;
	*p++=0x53;                                   //push rbx
	*p++=0x48; *p++=0x83; *p++=0xEC; *p++=0x20;  //sub rsp,20h (shadow space)
	*p++=0x48; *p++=0xBB;                         //mov rbx,&jitStep
	*(unsigned __int64*)p=(unsigned __int64)&jitStep; p+=8;
	for(Tlen k=0; k<n; k++){
		*p++=0x48; *p++=0xB9;                     //mov rcx,&code[k]
		*(unsigned __int64*)p=(unsigned __int64)(codeBase+k); p+=8;
		*p++=0xFF; *p++=0xD3;                     //call rbx
	}
	*p++=0x48; *p++=0x83; *p++=0xC4; *p++=0x20;  //add rsp,20h
	*p++=0x5B;                                    //pop rbx
	*p++=0xC3;                                    //ret

	((void (*)())code)();
	VirtualFree(code, 0, MEM_RELEASE);
	return true;
}
#endif

//compile a formula without evaluating it; the parser records instructions and
//only simulated stack items are discarded afterwards
void jitCompileFormula(Tjit *j, const char *formula, const char **e)
{
	Tjit *savedCur= jitCur;
	Tlen savedNumStack= numStack.len;
	Tlen savedOpStack= opStack.len;
	jitCur= j;
	parse(formula, e);
	jitCur= savedCur;
	jitRestoreCompileStack(savedNumStack, savedOpStack);
}

//record if(cond,a,b) as a jitIf instruction
Tcompiled *jitRecIf(const char *inputPtr)
{
	Tcompiled *c= jitCur->code++;
	c->kind= jitIf;
	c->inputPtr= inputPtr;
	c->sub= new Tjit[2];
	jitInit(&c->sub[0]);
	jitInit(&c->sub[1]);
	return c;
}

void jitFreeScript(Tjit *j)
{
	jitFree(j);
	cmdStart.reset();
}

static const char *jitSkipCommandStart(const char *s)
{
	const char *t;
	skipSpaces(s);
	for(t=s; isVarLetter(*t); t++);
	if(*t==':'){
		s=t+1;
		skipSpaces(s);
	}
	return s;
}

// emit one script-level instruction into the flat trace
static Tcompiled *jitScriptEmit(int kind)
{
	Tcompiled *c= jitCur->code++;
	c->kind= kind;
	return c;
}

void jitCompileScript(Tjit *j)
{
	jitFreeScript(j);
	jitInit(j);
	jitCur = j;

	for(Tlen cmd=0; cmd<gotoPositions.len; cmd++)
	{
		const char *input= jitSkipCommandStart(gotoPositions[cmd]);
		cmdNum = (int)cmd;

		// record where this command begins in the flat trace
		*cmdStart++= j->code.len;

		if(!*input || *input==';'){
			// empty command — nothing more needed
			continue;
		}
		const char *exprEnd;

		if(!_strnicmp(input, "print", 5)){
			input+=5;
			skipSpaces(input);
			bool pendingSpace=false;
			bool noNewLine=false;
			for(;;){
				if(*input=='\"'){
					exprEnd=input;
					int doubleQuotes= skipString(exprEnd)-1;
					if(error) return;
					if(pendingSpace){
						jitScriptEmit(jitPrintSpace);
						pendingSpace=false;
					}
					Tcompiled *p= jitScriptEmit(jitPrintText);
					p->inputPtr= input+1;
					p->length= int(exprEnd-input-1)-doubleQuotes;
					p->flags= doubleQuotes;
					if(*exprEnd) exprEnd++;
					skipSpaces(exprEnd);
					input=exprEnd;
				}
				else if(!*input || *input==';'){
					exprEnd = input;
					break;
				}
				else{
					// expression part
					if(pendingSpace){
						jitScriptEmit(jitPrintSpace);
						pendingSpace=false;
					}
					jitCompileFormula(j, input, &exprEnd);
					if(error) return;
					// jitCmdEnd: isPrint=true, writeResult=true
					Tcompiled *c= jitScriptEmit(jitCmdEnd);
					c->flags= 1|2; // writeResult | isPrint
					input=exprEnd;
				}
				if(*input!=',') break;
				input++;
				skipSpaces(input);
				if(!*input || *input==';'){
					// trailing comma = suppress newline
					noNewLine=true;
					exprEnd = input;
					break;
				}
				pendingSpace=true;
			}
			if(!noNewLine) jitScriptEmit(jitPrintNewLine);
		}
		else{
			// expression command
			jitCompileFormula(j, input, &exprEnd);
			if(error) return;
			// jitCmdEnd: writeResult when not ending with ';', isPrint=false
			Tcompiled *c= jitScriptEmit(jitCmdEnd);
			c->flags= (*exprEnd!=';') ? 1 : 0; // writeResult
		}
		if(!error){
			if(*exprEnd==',') cerror(956, "Unmatched comma");
			if(*exprEnd==']') cerror(963, "Unmatched right bracket");
			if(*exprEnd==')') cerror(955, "Unmatched parenthesis");
			if(error) errPos=exprEnd;
		}
	}
}

void jitUpdateNumbers(Tjit *j)
{
	for(Tlen k = 0; k<j->code.len; k++) {
		Tcompiled &c = j->code.array[k];
		if(c.kind==jitPushNum && c.num) {
			FREEX(c.num);
			c.num = ALLOCX(precision);
			int oldBase = baseIn;
			baseIn = c.base;
			READX(c.num, c.inputPtr);
			baseIn = oldBase;
		}
	}
}

#endif //_M_X64

void doOp()
{
	Tstack *t;

	if(error || opStack.len==0) return;
	t= opStack--;
	errPos = t->inputPtr;
#ifdef _M_X64
	if(jitRecording){
		jitRecApplyOp(t->op, t->inputPtr);
		jitCompileSimOp(t->op);
	}
	else
#endif
	doOpCore(t->op);
}
