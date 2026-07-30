/*
	(C) Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "preccalc.h"
#include "jit.h"

/*
 Just-in-time compiler captures the linear sequence of stack operations (push number, push variable,
 apply operator, apply function to counted arguments, array index). 
 That trace is emitted as a call-sequence which invokes the existing math routines through jitRun.
*/

static Darray<Tcompiled> code;
static Darray<Tlen> cmdStart;
Darray<char> outBuf;
const unsigned MAX_OUTPUT_SIZE=1000000000;

//---------------------------------------------------------------
void checkInfinite(Complex &y, Tint prec)
{
	if(y.r[-1]<-prec)
		ZEROX(y.r);
	if(y.i && y.i[-1]<-prec)
		ZEROX(y.i);
	if(y.r[-1]>prec && !isZero(y.r) || y.i && y.i[-1]>prec && !isZero(y.i)){
		cerror(1034, "Infinite result");
	}
}

void checkInfinite(Complex &y)
{
	if(precision>prec2+30) {
		if(!isMatrix(y)) {
			checkInfinite(y, prec2);
		}
		else {
			Pmatrix ym = toMatrix(y);
			for(int i = 0; i<ym->len; i++) {
				checkInfinite(ym->A[i], prec2);
			}
		}
	}
}
//---------------------------------------------------------------
void forExecute(const Top *o, Tcompiled *bodyJit)
{
	unsigned i;
	int j, len=0;
	Complex y, t, u, *stackEnd, *A=0;
	void *fc, *fm;
	bool isEach;

	stackEnd=&numStack[numStack.len];
	i = o->type-CMDFOR - 2;
	for(j=-1; unsigned(-j)<=i; j--){
		deref(stackEnd[j]);
	}
	i++;

	y=ALLOCC(precision);
	fm=o->mfunc;
	fc=o->cfunc;
	if(fm==INTEGRALM){
		INTEGRALM(y, stackEnd[-3], stackEnd[-2], stackEnd[-1], stackEnd[-4], bodyJit);
	}
	else{
		//for cycle
		isEach= o->type!=CMDFOR+4;
		if(isEach){
			//get matrix length and pointer to items
			if(isMatrix(stackEnd[-1])){
				Pmatrix m= toMatrix(stackEnd[-1]);
				len= m->len;
				A= m->A;
			}
			else{
				len=1;
				A=&stackEnd[-1];
			}
		}
		else{
			//first value and last value must be real or complex
			if(isMatrix(stackEnd[-2]) || isMatrix(stackEnd[-1])){
				errMatrix();
				goto lend;
			}
			//assign the first value to variable
			ASSIGNM(stackEnd[-2], stackEnd[-3], stackEnd[-2]);
		}
		if(fc) ((TunaryC0)fc)(y);
		t=ALLOCC(precision);
		u=ALLOCC(precision);
		for(j=0; !error; j++){
			if(isEach){
				if(j>=len) break;
				//assign matrix item to variable
				ASSIGNM(A[j], stackEnd[-2], A[j]);
			}
			else{
				//is variable greater then last value
				if(error || CMPC(toVariable(stackEnd[-3])->newx, stackEnd[-1]) > 0) break;
			}
			//evaluate expression
			jitRun(bodyJit);
			if(error) break;
			stackEnd=&numStack[numStack.len-1];
			deref(*stackEnd);
			//add item to result
			if(fm==FILTERM) {
				if(!isZero(*stackEnd)) {
					CONCATM(u, y, toVariable(stackEnd[isEach ? -2 : -3])->newx);
					std::swap(y, u);
				}
			}
			else if(fm) {
				if(j) {
					ensureImagPart(u);
					((TbinaryC)fm)(u, y, *stackEnd);
					std::swap(y, u);
				} else {
					std::swap(y, *stackEnd);
				}
			}
			FREEM(*numStack--);
			if(!isEach){
				//increment variable
				INCC(t, stackEnd[-3]);
			}
		}
		FREEM(u);
		FREEC(t);
	}
lend:
	//free arguments
	while(i--){
		FREEM(*numStack--);
	}
	//store the result
	*numStack++= y;
}
//---------------------------------------------------------------
//apply a function with a fixed or variable number of arguments
//to the top i values of the stack, the arguments are replaced by the result
void applyVararg(const Top *o, unsigned i, const char *opInputPtr)
{
	unsigned n;
	int j;
	Complex y, *stackEnd;
	void *fr, *fc, *fm;
	bool imag, matrix;

	imag=matrix=false;
	stackEnd=&numStack[numStack.len];
	for(j=-1; unsigned(-j)<=i; j--){
		Complex &v= stackEnd[j];
		if(o->mfunc!=SWAPM) deref(v);
		if(isMatrix(v)) matrix=true;
		if(isImag(v)) imag=true;
	}
	errPos = opInputPtr;
	y=ALLOCR(precision);
	n= o->type-CMDVARARG;
	fm=o->mfunc;
	fc=o->cfunc;
	fr=o->func;
	//call the function
	if(!fr){
		imag=true;
		if(!fc) matrix=true;
	}
	if(matrix){
		fc=fm;
		imag=true;
	}
	if(matrix && !fm){
		errMatrix();
	}
	else if(imag && !fc){
		errImag();
	}
	else{
		if(imag){
			ensureImagPart(y);
		}
		if(n==2){
			if(imag) ((TbinaryC)fc)(y, stackEnd[-2], stackEnd[-1]);
			else ((Tbinary)fr)(y.r, stackEnd[-2].r, stackEnd[-1].r);
		}
		else if(n){
			if(imag) ((TarrayargC)fc)(y, stackEnd-i);
			else ((Tarrayarg)fr)(y.r, stackEnd-i);
		}
		else{
			if(imag) ((TvarargC)fc)(y, i, stackEnd-i);
			else ((Tvararg)fr)(y.r, i, stackEnd-i);
		}
	}
	//free arguments
	while(i--){
		FREEM(*numStack--);
	}
	//store the result
	*numStack++= y;
}
//---------------------------------------------------------------
static int getIndex()
{
	int result=0;
	if(numStack.len){
		Complex &x= *numStack--;
		deref(x);
		if(isImag(x)){
			errImag();
		}
		else if(!isInt4(x.r)){
			cerror(1052, "Index is not integer");
		}
		else{
			result=toInt4(x.r);
			if(result<0){
				cerror(1051, "Index is less than zero");
			}
		}
		FREEM(x);
	}
	return result;
}

//apply parsed indexes to the value or variable at the stack top:
//variables become ranges, other values are indexed immediately
void applyArrayIndex(int f)
{
	//pop the recorded index expressions in reverse order of evaluation
	int D[2][2];
	D[0][0]=D[0][1]=D[1][0]=D[1][1]=-1;
	if(f&8) D[1][1]= getIndex();
	if(f&4){ D[1][0]= getIndex(); if(!(f&8)) D[1][1]= D[1][0]; }
	if(f&2) D[0][1]= getIndex();
	if(f&1){ D[0][0]= getIndex(); if(!(f&2)) D[0][1]= D[0][0]; }
	if(!numStack.len) return;
	Complex &x= numStack[numStack.len-1];
	if(x.r && isVariable(x)){
		//create range
		x.r[-3] = -5;
		int *xD = (int*)&x.r[1];
		xD[0] = D[0][0];
		xD[1] = D[0][1];
		xD[2] = D[1][0];
		xD[3] = D[1][1];
	}
	else{
		Complex y, x1;
		deref(x1, x);
		y= ALLOCC(precision);
		INDEXM(y, x1, &D[0][0]);
		FREEM(x);
		x=y;
	}
}
//---------------------------------------------------------------
void UnaryOp(const Top *o)
{
	if(numStack.len==0) return;
	Complex &a1=numStack[numStack.len-1], y;
	void *fr, *fc, *fm;
	fr=o->func;
	fc=o->cfunc;
	if(fc!=DECC && fc!=INCC) deref(a1);
	y=ALLOCR(precision);
	if(isMatrix(a1) || !fc && !fr){
		fm=o->mfunc;
		if(!fm) cerror(1065, "The function requires one parameter");
		else {
			ensureImagPart(y);
			((TunaryC2)fm)(y, a1);
		}
	}
	else if(isImag(a1) || !fr){
		if(!fc) errImag();
		else {
			ensureImagPart(y);
			((TunaryC2)fc)(y, a1);
		}
	}
	else{
		((Tunary2)fr)(y.r, a1.r);
	}
	FREEM(a1);
	a1=y;
}

void UnaryFastOp(const Top *o)
{
	if(numStack.len==0) return;
	Complex &a1=numStack[numStack.len-1];
	void *fr, *fc, *fm;
	fr = o->func;
	fc = o->cfunc;
	deref(a1);
	if(isMatrix(a1) || !fc && !fr) {
		fm = o->mfunc;
		if(!fm) errMatrix();
		else {
			((TunaryC0)fm)(a1);
		}
	}
	else if(isImag(a1) || !fr) {
		if(!fc) errImag();
		else {
			((TunaryC0)fc)(a1);
		}
	}
	else {
		((Tunary0)fr)(a1.r);
	}
}

void BinaryOp(const Top *o)
{
	if(numStack.len<2) return;
	Complex &a1 = numStack[numStack.len-1], &a2 = *(&a1-1), y;
	deref(a1);
	void *fr, *fc, *fm;
	fr = o->func;
	fc = o->cfunc;
	fm = o->mfunc;
	if(fm!=ASSIGNM) deref(a2);
	y = ALLOCR(precision);
	if(isMatrix(a1) || isMatrix(a2) || !fc && !fr) {
		if(!fm) errMatrix();
		else {
			ensureImagPart(y);
			((TbinaryC)fm)(y, a2, a1);
		}
	}
	else if(isImag(a1) || isImag(a2) || !fr) {
		if(!fc) errImag();
		else {
			ensureImagPart(y);
			((TbinaryC)fc)(y, a2, a1);
		}
	}
	else {
		((Tbinary)fr)(y.r, a2.r, a1.r);
	}
	FREEM(a2);
	FREEM(a1);
	a2 = y;
	numStack--;
}

void TernaryOp(const Top *o)
{
	if(numStack.len<3) return;
	Complex &a1=numStack[numStack.len-1], &a2=*(&a1-1), &a3=*(&a1-2), y;
	deref(a1);
	deref(a2);
	deref(a3);
	y = ALLOCR(precision);
	if(isMatrix(a1) || isMatrix(a2) || isMatrix(a3)) {
		errMatrix();
	}
	else if(isImag(a1) || isImag(a2) || isImag(a3)) {
		errImag();
	}
	else {
		((Tternary)o->func)(y.r, a3.r, a2.r, a1.r);
	}
	FREEM(a3);
	FREEM(a2);
	FREEM(a1);
	a3 = y;
	numStack -= 2;
}

void ConstOp(const Top *o)
{
	void *fr, *fc, *fm;
	fr = o->func;
	fc = o->cfunc;
	fm = o->mfunc;
	Complex y = ALLOCC(precision);
	if(fm) ((TnularyC)fm)(y);
	else if(fr) ((Tnulary)fr)(y.r);
	else ((TnularyC)fc)(y);
	*numStack++ = y;
}
//---------------------------------------------------------------
bool printValue(Complex &y)
{
	Darray<char> &buf = outBuf;
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

void printText(const char *text, int len, int doubleQuotes)
{
	Darray<char> &buf = outBuf;
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

void printSpace()
{
	Darray<char> &buf = outBuf;
	char *a=(buf++);
	if((unsigned)buf.len > MAX_OUTPUT_SIZE){
		buf--;
		cerror(1062, "Result is too long");
		return;
	}
	a[-1]=' ';
	a[0]=0;
}

void printNewLine()
{
	Darray<char> &buf = outBuf;
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

void jitRun(Tcompiled *j)
{
	for(Tcompiled *ins = j; !error; )
	{
		//execute one instruction
		switch(ins->kind){
		case jitPushNum: {
			Complex x;
			if(ins->num) {
				x = ALLOCR(precision);
				COPYX(x.r, ins->num);
			}
			else x.r = x.i = 0;
			*numStack++ = x;
			break;
		}
		case jitPushInt: {
			Complex x = ALLOCR(precision);
			SETXN(x.r, ins->integer);
			*numStack++ = x;
			break;
		}
		case jitPushFraction: {
			Complex x = ALLOCR(precision);
			x.r[0]= ins->integer;
			x.r[1]= ins->fraction;
			x.r[-1] = (Tuint)ins->integer >= ins->fraction;
			x.r[-3] = -2;
			*numStack++ = x;
			break;
		}
		case jitPushVar:{
			Complex x= ALLOCC(precision); //deref needs this precision
			x.r[0]= ins->variable;
			x.r[-3]= -1;
			*numStack++= x;
			break;
		}
		case jitUnaryOp:
			errPos= ins->inputPtr;
			UnaryOp(ins->op);
			break;
		case jitUnaryFastOp:
			errPos= ins->inputPtr;
			UnaryFastOp(ins->op);
			break;
		case jitBinaryOp:
			errPos= ins->inputPtr;
			BinaryOp(ins->op);
			break;
		case jitTernaryOp:
			errPos= ins->inputPtr;
			TernaryOp(ins->op);
			break;
		case jitConst:
			errPos= ins->inputPtr;
			ConstOp(ins->op);
			break;
		case jitApplyVararg:
			applyVararg(ins->op, ins->argCount, ins->inputPtr);
			break;
		case jitFor:
			//execute a construct with repeated argument evaluation (for, foreach, integral, ...) using compiled sub-trace
			forExecute(ins->op, ins+1);
			ins += ins->subLen;
			break;
		case jitIf: {
			errPos = ins->inputPtr;
			deref(numStack[numStack.len-1]);
			Complex y = *numStack--;
			bool cond = !isZero(y);
			FREEM(y);
			if(cond) {
				jitRun(ins+1);
				ins += ins->length;
			}
			else
				ins += ins->subLen;
			break;
		}
		case jitArrayIdx:{
			errPos= ins->inputPtr;
			applyArrayIndex(ins->indexes);
			break;
		}
		case jitCmdStart:
			// set current command number for gotor
			cmdNum= ins->cmdNum;
			break;
		case jitCmdEnd:{
			// finish an expression command: check errors, write result, set ans
			if(numStack.len!=1){ cerror(1029, "Fatal error"); break; }
			if(gotoPos>=0){
				cleanup();
				if(gotoPos>=cmdStart.len){
					errGoto();
				}
				else{
					// goto: find the target command's start index
					ins= j + cmdStart[gotoPos];
					gotoPos=-1;
					continue;
				}
			}
			if(error) break;
			Complex y2= *numStack--;
			deref(y2);
			checkInfinite(y2);
			if(error){ FREEM(y2); break; }
			if(ins->flags && !printValue(y2)){
				FREEM(y2); break;
			}
			FREEM(ans);
			ans=y2;
			break;
		}
		case jitPrintText:
			printText(ins->inputPtr, ins->length, ins->flags);
			break;
		case jitPrintNewLine:
			printNewLine();
			break;
		case jitPrintSpace:
			printSpace();
			break;
		case jitEnd:
			return;
		}
		ins++;
	}
}

void jitScriptRun()
{
	gotoPos=-1;
	retValue.r = retValue.i = 0;

	jitRun(code);

	//command "return"
	if(retValue.r || error==1102){
		cleanup();
		Complex y=retValue;
		if(!y.r){
			ClearError(1102);
		}
		else {
			retValue.r = retValue.i = 0;
			ClearError(1101);
			checkInfinite(y);
			if(error){
				FREEM(y);
			}
			else {
				printValue(y);
				FREEM(ans); 
				ans = y;
			}
		}
	}
}

void jitFree()
{
	for(Tlen k=0; k<code.len; k++){
		Tcompiled &c= code[k];
		if(c.kind==jitPushNum){
			FREEX(c.num);
		}
	}
	code.reset();
}
//---------------------------------------------------------------
#if JIT_EMIT
#error goto not implemented in jitEmit yet
//emit a native x64 stub that calls jitStep once per recorded instruction
static bool jitEmit()
{
	Tlen n= jitCodeLen();
	Tcompiled *codeBase= code;
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
//---------------------------------------------------------------
// emit one instruction into the flat trace
Tcompiled *jitEmit(int kind)
{
	Tcompiled *c= code++;
	c->kind= kind;
	return c;
}

Tlen jitCodeLen()
{
	return code.len;
}

Tcompiled *jitCurGet(Tlen idx)
{
	return &code[idx];
}

void jitCompileScript(const char *input)
{
	const char *e;

	cmdStart.reset();

	for(cmdNum=0; !error; cmdNum++)
	{
		// record where this command begins
		*cmdStart++= code.len;
		//skip label
		skipSpaces(input);
		for(e=input; isVarLetter(*e); e++);
		if(*e==':'){
			input=e+1;
			skipSpaces(input);
		}
		if(!*input) break; // end of script
		if(*input==';') continue; // empty command

		if(!_strnicmp(input, "print", 5)){
			input+=5;
			skipSpaces(input);
			bool pendingSpace=false;
			bool noNewLine=false;
			for(;;){
				if(*input=='\"'){
					//literal text
					e=input;
					int doubleQuotes= skipString(e)-1;
					if(error) return;
					if(pendingSpace){
						jitEmit(jitPrintSpace);
						pendingSpace=false;
					}
					Tcompiled *p= jitEmit(jitPrintText);
					p->inputPtr= ++input;
					p->length= int(e-input)-doubleQuotes;
					p->flags= doubleQuotes;
					if(*e) e++;
					skipSpaces(e);
					input=e;
				}
				else if(!*input || *input==';'){
					e = input;
					break;
				}
				else{
					// print expression
					if(pendingSpace){
						jitEmit(jitPrintSpace);
						pendingSpace=false;
					}
					parse(input, &e);
					if(error) return;
					jitEmit(jitCmdEnd)->flags= 1; // writeResult
					input=e;
				}
				if(*input!=',') break;
				input++;
				skipSpaces(input);
				if(!*input || *input==';'){
					// trailing comma = suppress newline
					noNewLine=true;
					e = input;
					break;
				}
				pendingSpace=true;
			}
			if(!noNewLine) jitEmit(jitPrintNewLine);
		}
		else{
			// expression
			parse(input, &e);
			if(error) return;
			// writeResult when not ending with ';'
			jitEmit(jitCmdEnd)->flags= (*e!=';');
		}
		if(!error) errPos = e;
		if(*e==',') cerror(956, "Unmatched comma");
		if(*e==']') cerror(963, "Unmatched right bracket");
		if(*e==')') cerror(955, "Unmatched parenthesis");
		if(*e) e++;
		input=e;
	}
	jitEmit(jitEnd);
}

void jitUpdateNumbers()
{
	for(Tlen k = 0; k<code.len; k++) {
		Tcompiled &c = code[k];
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
