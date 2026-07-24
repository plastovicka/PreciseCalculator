/*
	(C) Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "preccalc.h"
#include "jit.h"

#ifdef _M_X64
/*
 Just-in-time compiler
 The first iteration runs the interpreter with recording enabled and
 captures the linear sequence of stack operations (push number, push variable,
 apply operator, apply function to counted arguments, array index). That
 trace is emitted as a call-sequence which invokes the existing
 math routines through jitStep.
 if(cond,a,b) is compiled as a jitIf instruction whose condition and branches
 are separate sub-traces, so both branches end up compiled even
 when the condition changes between iterations. Nested for/foreach constructs
 are recorded as one jitFor instruction whose loop header expressions
 and body are persistent sub-traces compiled only once. Commands goto and
 gotor set gotoPos just like the interpreter.
*/

bool jitRecording=false;
static Tjit *jitCur=0;

//execute one recorded instruction
static void jitStep(Tcompiled *ins)
{
#if JIT_EMIT
	if(error) return;
#endif
	switch(ins->kind){
		case jitApplyOp:
			errPos= ins->inputPtr;
			doOpCore(ins->op);
			break;
		case jitPushNum:
			*numStack++ = ins->num.r ? NEWCOPYC(ins->num) : Complex{0, 0};
			break;
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
			//(for, foreach, integral, ...) using persistent compiled sub-traces
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
	}
}

void jitRun(Tjit *j)
{
#if JIT_EMIT
	if(j->stubMem){
		((void (*)())j->stubMem)();
		return;
	}
	//fallback when executable memory could not be allocated
#endif
	for(Tcompiled *k=j->code.array, *e = k + j->code.len ; k<e && !error; k++){
		jitStep(k);
	}
}

static bool jitAppendValue(Darray<char> &buf, Complex y)
{
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

bool jitAppendText(Darray<char> &buf, const char *text, int len, int doubleQuotes)
{
	char *a=(buf+=len)-1;
	if((unsigned)buf.len > MAX_OUTPUT_SIZE){
		buf-=len;
		cerror(1062, "Result is too long");
		return false;
	}
	if(doubleQuotes>0) copyString(a, text);
	else memcpy(a, text, len);
	a[len]=0;
	return true;
}

bool jitAppendChar(Darray<char> &buf, char c)
{
	char *a=(buf++);
	if((unsigned)buf.len > MAX_OUTPUT_SIZE){
		buf--;
		cerror(1062, "Result is too long");
		return false;
	}
	a[-1]=c;
	a[0]=0;
	return true;
}

bool jitAppendNewLine(Darray<char> &buf)
{
	char *a=(buf+=2);
	if((unsigned)buf.len > MAX_OUTPUT_SIZE){
		buf-=2;
		cerror(1062, "Result is too long");
		return false;
	}
	a[-1]='\r';
	a[0]='\n';
	a[1]=0;
	return true;
}

int jitExecuteExpression(Tjit *j, const char *input, const char *compiledEnd,
	Darray<char> &buf, bool isPrint, bool writeResult, const char *param, int &errIndex)
{
	const char *e= compiledEnd;
	Complex y;
	gotoPos=-1;
	retValue.r=retValue.i=0;
	inParenthesis=0;
	if(j){
		jitRun(j);
	}
	else{
		parse(input, &e);
	}
	if(numStack.len!=1) cerror(1029, "Fatal error");
	if(!error){
		if(*e==',' && !isPrint) cerror(956, "Unmatched comma");
		if(*e==']') cerror(963, "Unmatched right bracket");
		if(*e==')') cerror(955, "Unmatched parenthesis");
		if(error) errPos=e;
	}
	if(gotoPos>=0){
		cleanup();
		if(gotoPos>=gotoPositions.len){
			errGoto();
			return jitExecError;
		}
		return jitExecGoto;
	}
	if(retValue.r || error==1102){
		cleanup();
		y=retValue;
		if(!y.r){
			ClearError(1102);
			return jitExecStop;
		}
		ClearError(1101);
		writeResult = true;
	}
	else{
		if(error){
			errIndex=int(errPos-param);
			return jitExecError;
		}
		y= *numStack--;
	}
	deref(y);
	if(precision>prec2+30){
		if(!isMatrix(y)){
			checkInfinite(y, prec2);
		}
		else{
			Pmatrix ym= toMatrix(y);
			for(Tlen i=0; i<ym->len; i++){
				checkInfinite(ym->A[i], prec2);
			}
		}
	}
	if(error){
		FREEM(y);
		return jitExecError;
	}
	if(writeResult && !jitAppendValue(buf, y)){
		FREEM(y);
		return jitExecError;
	}
	FREEM(ans);
	ans=y;
	return retValue.r ? jitExecStop : jitExecNext;
}

void jitInit(Tjit *j)
{
	j->code.reset();
#if JIT_EMIT
	j->stubMem=0;
#endif
}

void jitFree(Tjit *j)
{
	for(Tlen k=0; k<j->code.len; k++){
		Tcompiled &c= j->code.array[k];
		if(c.kind==jitPushNum){
			FREEM(c.num);
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
#if JIT_EMIT
	if(j->stubMem){
		VirtualFree(j->stubMem, 0, MEM_RELEASE);
		j->stubMem=0;
	}
#endif
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

void jitRecPushNum(const Complex x)
{
	Tcompiled *c= jitCur->code++;
	c->kind= jitPushNum;
	c->num= x;
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
//emit a native x64 stub that calls jitStep once per recorded instruction
static void jitEmit(Tjit *j)
{
	Tlen n= j->code.len;
	Tcompiled *codeBase= j->code.array;
	SIZE_T size= 15 + (SIZE_T)12*n + 6;
	unsigned char *code= (unsigned char*)VirtualAlloc(0, size, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if(!code) return;
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
	j->stubMem= code;
}
#endif

//compile a formula without evaluating it; the parser records instructions and
//only simulated stack items are discarded afterwards
void jitCompileFormula(Tjit *j, const char *formula, const char **e)
{
	bool savedRecording= jitRecording;
	Tjit *savedCur= jitCur;
	Tlen savedNumStack= numStack.len;
	Tlen savedOpStack= opStack.len;
	jitCur= j;
	jitRecording= true;
	parse(formula, e);
	jitRecording= savedRecording;
	jitCur= savedCur;
	jitRestoreCompileStack(savedNumStack, savedOpStack);
#if JIT_EMIT
	if(!error) jitEmit(j);
#endif
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

static Tjit *jitNewTrace()
{
	Tjit *j= new Tjit;
	jitInit(j);
	return j;
}

static TjitCommand *jitNewCommand()
{
	TjitCommand *c= new TjitCommand;
	c->kind= jitCmdEmpty;
	c->input=0;
	c->end=0;
	c->noNewLine=false;
	c->expr=0;
	return c;
}

static void jitDeleteTrace(Tjit *j)
{
	if(j){
		jitFree(j);
		delete j;
	}
}

static void jitDeleteCommand(TjitCommand *c)
{
	if(!c) return;
	jitDeleteTrace(c->expr);
	for(Tlen i=0; i<c->parts.len; i++){
		jitDeleteTrace(c->parts[i].expr);
	}
	delete c;
}

void jitFreeScript(TjitScript *script)
{
	for(Tlen i=0; i<script->cmds.len; i++){
		jitDeleteCommand(script->cmds[i]);
	}
	script->cmds.reset();
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

bool jitCompileScript(TjitScript *script)
{
	jitFreeScript(script);
	for(Tlen cmd=0; cmd<gotoPositions.len; cmd++){
		TjitCommand *c= jitNewCommand();
		*script->cmds++= c;
		const char *input= jitSkipCommandStart(gotoPositions[cmd]);
		c->input= input;
		if(!*input || *input==';'){
			c->end= input;
			continue;
		}
		if(!_strnicmp(input, "print", 5)){
			c->kind= jitCmdPrint;
			input+=5;
			skipSpaces(input);
			for(;;){
				if(*input=='\"'){
					const char *e=input;
					int doubleQuotes= skipString(e)-1;
					if(error) return false;
					TjitPrintPart *p= c->parts++;
					p->kind= jitPrintText;
					p->input= input;
					p->text= input+1;
					p->textLen= int(e-input-1)-doubleQuotes;
					p->doubleQuotes= doubleQuotes;
					p->spaceAfter=false;
					p->expr=0;
					p->end=e;
					if(*e) e++;
					skipSpaces(e);
					input=e;
				}
				else if(!*input || *input==';'){
					c->end= input;
					break;
				}
				else{
					TjitPrintPart *p= c->parts++;
					p->kind= jitPrintExpr;
					p->input= input;
					p->text=0;
					p->textLen=0;
					p->doubleQuotes=0;
					p->spaceAfter=false;
					p->expr= jitNewTrace();
					jitCompileFormula(p->expr, input, &p->end);
					if(error) return false;
					input=p->end;
				}
				if(*input==','){
					const char *next=input+1;
					skipSpaces(next);
					if(c->parts.len && *next!=';' && *next){
						c->parts[c->parts.len-1].spaceAfter=true;
					}
					else if(!*next || *next==';'){
						c->noNewLine=true;
					}
					input=next;
					continue;
				}
				c->end= input;
				break;
			}
		}
		else{
			c->kind= jitCmdExpr;
			c->expr= jitNewTrace();
			jitCompileFormula(c->expr, input, &c->end);
			if(error) return false;
		}
	}
	return !error;
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
