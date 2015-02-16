/*
	(C) 2004-2012  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include <ctype.h>
#include "preccalc.h"

typedef long Tinterlock;

const unsigned MAX_OUTPUT_SIZE=1000000000;

struct Top {
	const char *name;
	int type;
	void *func;
	void *cfunc;
	void *mfunc;
};

struct Tstack {
	const Top *op;
	const char *inputPtr;
};

struct Tlabel {
	const char *name;
	int nameLen;
	int ind;
};

Tint precision, prec2;
Complex ans, oldAns, retValue;
Pint oldSeedx;
HANDLE thread;
const char *errPos;
int gotoPos;
int cmdNum;
int inParenthesis;
Darray<Complex> numStack;
Darray<Tstack> opStack;
Darray<Tvar> vars;
Darray<Tlabel> labels;
Darray<const char*> gotoPositions;
Darray<Tfunc> funcs;
const Top **funcTabSorted;

void _stdcall SETBASEX(Complex y, const Complex b, const Complex x)
{
	COPYM(y, x);
	baseIn=(int)b.r[0];
}

void _stdcall ANS(Complex y)
{
	COPYM(y, ans);
}

void _fastcall RETM(Complex y)
{
	if(!retValue.r){
		retValue= y;
		numStack--;
		error=1101;
	}
}

void assign(Pint &dest, const Pint src)
{
	if(!src) return;
	if(!dest || dest[-4] < src[-3]){
		FREEX(dest);
		dest= NEWCOPYX(src);
	}
	else{
		COPYX(dest, src);
	}
}

void assign(Complex &dest, const Complex src)
{
	if(!src.r) return;
	if(isMatrix(src)){
		assignM(dest, src);
	}
	else if(!dest.r || isMatrix(dest)){
		FREEM(dest);
		dest=NEWCOPYC(src);
	}
	else{
		assign(dest.r, src.r);
		assign(dest.i, src.i);
	}
}

Complex *deref1(Complex &x)
{
	if(x.r && isVariable(x)){
		Tvar *v= toVariable(x);
		return v->modif ? &v->newx : &v->oldx;
	}
	return 0;
}

bool deref(Complex &y, Complex &x)
{
	if(x.r && (isVariable(x) || isRange(x))){
		Tvar *v= toVariable(x);
		y= v->modif ? v->newx : v->oldx;
		return true;
	}
	else{
		y=x;
		return false;
	}
}

void deref(Complex &x)
{
	Complex v;
	if(deref(v, x)){
		if(isRange(x)){
			Complex y= ALLOCC(precision);
			INDEXM(y, v, (int*)&x.r[1]);
			FREEM(x);
			x=y;
		}
		else{
			COPYM(x, v);
		}
	}
}

void _stdcall ASSIGNM(Complex y, const Complex a, const Complex x)
{
	if(!isVariable(a) && !isRange(a)){
		cerror(960, "There is a numeric expression at the left side of an assignment");
	}
	else{
		COPYM(y, x);
		Tvar *v= toVariable(a);
		if(isRange(a)){
			if(!v->modif) COPYM(v->newx, v->oldx);
			assignRange(v->newx, x, (int*)&a.r[1]);
		}
		else{
			assign(v->newx, x);
		}
		v->modif=true;
	}
}

void INCDEC(Complex &y, const Complex &a, bool inc)
{
	if(!isVariable(a) && !isRange(a)){
		cerror(961, "Increment or decrement of an expression");
	}
	else{
		Tvar *v= toVariable(a);
		Complex &c= v->modif ? v->newx : v->oldx;
		if(isRange(a)){
			if(!v->modif) COPYM(v->newx, v->oldx);
			incdecRange(y, v->newx, inc, (int*)&a.r[1]);
			v->modif=true;
		}
		else{
			if(isMatrix(c)){
				cerror(1042, "Increment or decrement of a matrix");
			}
			else{
				if(inc) PLUSX(y.r, c.r, one);
				else MINUSX(y.r, c.r, one);
				COPYX(y.i, c.i);
				assign(v->newx, y);
				v->modif=true;
			}
		}
	}
}

void _stdcall INCC(Complex y, const Complex a)
{
	INCDEC(y, a, true);
}

void _stdcall DECC(Complex y, const Complex a)
{
	INCDEC(y, a, false);
}

void _stdcall SWAPM(Complex y, Complex ca, Complex cb)
{
	if(!isVariable(ca) || !isVariable(cb)){
		cerror(1061, "Parameters must be variables");
		return;
	}
	Tvar *va= toVariable(ca);
	Tvar *vb= toVariable(cb);
	if(va!=vb){
		if(va->modif && vb->modif){
			Complex w;
			w=va->newx; va->newx=vb->newx; vb->newx=w;
		}
		else{
			Complex wa, wb;
			deref(wa, ca);
			deref(wb, cb);
			assign(va->newx, wb);
			assign(vb->newx, wa);
			va->modif=vb->modif=true;
		}
	}
	ZEROC(y);
}

void errGoto()
{
	cerror(1033, "Invalid number after goto");
}

void _fastcall GOTOX(Pint y)
{
	if(!isDword4(y)) errGoto();
	gotoPos= toDword4(y);
}

void _fastcall GOTORELX(Pint y)
{
	if(!isInt4(y)) errGoto();
	gotoPos= cmdNum+toInt4(y);
	if(gotoPos<0) errGoto();
}

//-------------------------------------------------------------------

const int CMDBASE=3, CMDPLUS=144, CMDMINUS=143, CMDASSIGN=397,
CMDLEFT=401, CMDGOTO=402, CMDRIGHT=455, CMDEND=460, CMDVARARG=500,
	CMDFOR=550;

#define F (void*)

const Top opNeg={0, 9, F NEGX, F NEGC, F NEGM};
const Top opImag={0, 2, 0, F ISUFFIXC, 0};
const Top opTransp={0, 2, 0, 0, F TRANSP2M};

/*
 0=number or variable
 1=constant function
 2=postfix operator
 3=base
 4..7=binary operator
 8=prefix _stdcall operator
 9=prefix _fastcall operator
 10..399=binary operator
 400..449=left parenthesis, commands
 450..499=right parenthesis, comma, semicolon, nul
 500..549=function with variable number of arguments
 503=ternary function
 550..599= for, foreach
 */
const Top funcTab[]={
	{")", CMDRIGHT, 0, 0, 0},
	{"(", CMDLEFT, 0, 0, 0},
	{",", 398, 0, 0, F CONCATM},
	{"\\", 399, 0, 0, F CONCATROWM},
	{"return", 400, 0, 0, F RETM},
	{"goto", CMDGOTO, F GOTOX, 0, 0},
	{"gotor", 400, F GOTORELX, 0, 0},
	{"base", CMDBASE, 0, 0, F SETBASEX},
	{"dec", CMDBASE, 0, 0, F SETBASEX},
	{"hex", CMDBASE, 0, 0, F SETBASEX},
	{"bin", CMDBASE, 0, 0, F SETBASEX},
	{"oct", CMDBASE, 0, 0, F SETBASEX},

	//statistical functions
	{"min", 8, 0, 0, F MINM},
	{"max", 8, 0, 0, F MAXM},
	{"med", 8, 0, 0, F MEDIANM},
	{"mode", 8, 0, 0, F MODEM},
	{"sort", 9, 0, 0, F SORTM},
	{"sortd", 9, 0, 0, F SORTDM},
	{"reverse", 9, 0, 0, F REVERSEM},
	{"sum", 8, 0, 0, F SUM1M},
	{"sumx", 8, 0, 0, F SUMXM},
	{"sumy", 8, 0, 0, F SUMYM},
	{"sumq", 8, 0, 0, F SUM2M},
	{"sumxq", 8, 0, 0, F SUMX2M},
	{"sumyq", 8, 0, 0, F SUMY2M},
	{"sumxy", 8, 0, 0, F SUMXYM},
	{"geom", 8, 0, 0, F GEOMM},
	{"harmon", 8, 0, 0, F HARMONM},
	{"product", 8, 0, 0, F PRODUCTM},
	{"count", 9, 0, 0, F COUNTM},
	{"ave", 8, 0, 0, F AVE1M}, {"mean", 8, 0, 0, F AVE1M},
	{"avex", 8, 0, 0, F AVEXM}, {"meanx", 8, 0, 0, F AVEXM},
	{"avey", 8, 0, 0, F AVEYM}, {"meany", 8, 0, 0, F AVEYM},
	{"aveq", 8, 0, 0, F AVE2M}, {"meanq", 8, 0, 0, F AVE2M},
	{"avexq", 8, 0, 0, F AVEX2M}, {"meanxq", 8, 0, 0, F AVEX2M},
	{"aveyq", 8, 0, 0, F AVEY2M}, {"meanyq", 8, 0, 0, F AVEY2M},
	{"vara", 8, 0, 0, F VAR0M},
	{"varxa", 8, 0, 0, F VARX0M},
	{"varya", 8, 0, 0, F VARY0M},
	{"var", 8, 0, 0, F VAR1M},
	{"varx", 8, 0, 0, F VARX1M},
	{"vary", 8, 0, 0, F VARY1M},
	{"stdeva", 8, 0, 0, F STDEV0M},
	{"stdevxa", 8, 0, 0, F STDEVX0M},
	{"stdevya", 8, 0, 0, F STDEVY0M},
	{"stdev", 8, 0, 0, F STDEV1M},
	{"stdevx", 8, 0, 0, F STDEVX1M},
	{"stdevy", 8, 0, 0, F STDEVY1M},
	{"lra", 8, 0, 0, F LRAM},
	{"lrb", 8, 0, 0, F LRBM},
	{"lrr", 8, 0, 0, F LRRM},
	{"lrx", CMDVARARG+2, 0, 0, F LRXM},
	{"lry", CMDVARARG+2, 0, 0, F LRYM},

	{"lcm", 8, 0, 0, F LCMM},
	{"gcd", 8, 0, 0, F GCDM},
	{"if", CMDVARARG+3, F IFX, 0, 0},

	{"integral", CMDFOR+5, 0, 0, F INTEGRALM},
	{"for", CMDFOR+4, 0, 0, 0},
	{"foreach", CMDFOR+3, 0, 0, 0},
	{"sumfor", CMDFOR+4, 0, 0, F PLUSM},
	{"sumforeach", CMDFOR+3, 0, 0, F PLUSM},
	{"productfor", CMDFOR+4, 0, F ONEC, F MULTM},
	{"productforeach", CMDFOR+3, 0, F ONEC, F MULTM},
	{"listfor", CMDFOR+4, 0, 0, F CONCATM},
	{"listforeach", CMDFOR+3, 0, 0, F CONCATM},
	{"rowsfor", CMDFOR+4, 0, 0, F CONCATROWM},
	{"rowsforeach", CMDFOR+3, 0, 0, F CONCATROWM},
	{"minfor", CMDFOR+4, 0, 0, F MIN3M},
	{"minforeach", CMDFOR+3, 0, 0, F MIN3M},
	{"maxfor", CMDFOR+4, 0, 0, F MAX3M},
	{"maxforeach", CMDFOR+3, 0, 0, F MAX3M},

	{"hypot", CMDVARARG+2, F HYPOTX, 0, 0},
	{"polar", CMDVARARG+2, 0, F POLARC, 0},
	{"complex", CMDVARARG+2, 0, F COMPLEXC, 0},
	{"logn", CMDVARARG+2, 0, F LOGNC, 0},

	{"++", 2, 0, F INCC, 0},
	{"--", 2, 0, F DECC, 0},

	//binary operators
	{"or", 199, F ORX, F ORC, F ORM}, {"|", 199, F ORX, F ORC, F ORM},
	{"nor", 199, F NORX, 0, 0},
	{"bitnor", 199, F NORBX, F NORBC, F NORBM},
	{"xor", 198, F XORX, F XORC, F XORM},
	{"and", 197, F ANDX, F ANDC, F ANDM}, {"&", 197, F ANDX, F ANDC, F ANDM},
	{"nand", 197, F NANDX, 0, 0},
	{"bitnand", 197, F NANDBX, F NANDBC, F NANDBM},
	{"imp", 200, F IMPX, 0, 0}, {"->", 200, F IMPX, 0, 0},
	{"bitimp", 200, F IMPBX, F IMPBC, F IMPBM},
	{"eqv", 201, F EQVX, 0, 0},
	{"biteqv", 201, F EQVBX, F EQVBC, F EQVBM},
	{"==", 171, F EQUALX, F EQUALC, F EQUALM},
	{"<>", 171, F NOTEQUALX, F NOTEQUALC, F NOTEQUALM}, {"!=", 171, F NOTEQUALX, F NOTEQUALC, F NOTEQUALM},
	{">=", 170, F GREATEREQX, 0, 0},
	{"<=", 170, F LESSEQX, 0, 0},
	{"lsh", 155, F LSHX, F LSHC, F LSHM}, {"<<", 155, F LSHX, F LSHC, F LSHM},
	{"rshi", 155, F RSHIX, F RSHIC, F RSHIM}, {">>>", 155, F RSHIX, F RSHIC, F RSHIM},
	{"rsh", 155, F RSHX, F RSHC, F RSHM}, {">>", 155, F RSHX, F RSHC, F RSHM},
	{"=", CMDASSIGN, 0, 0, F ASSIGNM},
	{">", 170, F GREATERX, 0, 0},
	{"<", 170, F LESSX, 0, 0},
	{"+", CMDPLUS, F PLUSX, F PLUSC, F PLUSM},
	{"-", CMDMINUS, F MINUSX, F MINUSC, F MINUSM},
	{"**", 5, 0, F POWC, F POWM},
	{"*", 121, F MULTX, F MULTC, F MULTM},
	{"mod", 121, F MODX, 0, 0}, {"%", 121, F MODX, 0, 0},
	{"div", 121, F IDIVX, 0, 0},
	{"/", 121, F DIVX, F DIVC, F DIVM},
	{"combin", 111, F COMBINX, 0, 0}, {"ncr", 111, F COMBINX, 0, 0},
	{"permut", 111, F PERMUTX, 0, 0}, {"npr", 111, F PERMUTX, 0, 0},
	{"^", 5, 0, F POWC, F POWM},
	{"#", 4, 0, F ROOTC, 0},

	//postfix operators
	{"!!", 2, F FFACTX, 0, 0},
	{"!", 2, F FACTORIALX, 0, 0},
	{"[", 2, 0, 0, F INDEXM},
	{"�", 2, F DEGX, 0, 0},

	//constants
	{"pi", 1, F PIX, 0, 0},
	{"ans", 1, 0, 0, F ANS},
	{"rand", 1, F RANDX, 0, 0},

	//prefix operators
	{"abs", 9, F ABSX, F ABSC, F ABSM},
	{"sign", 9, F SIGNX, F SIGNC, 0},
	{"round", 9, F ROUNDX, F ROUNDC, F ROUNDM},
	{"trunc", 9, F TRUNCX, F TRUNCC, F TRUNCM},
	{"int", 9, F INTX, F INTC, F INTM}, {"floor", 9, F INTX, F INTC, F INTM},
	{"ceil", 9, F CEILX, F CEILC, F CEILM},
	{"frac", 9, F FRACX, F FRACC, F FRACM},
	{"real", 9, 0, F REALC, F REALM},
	{"imag", 9, 0, F IMAGC, F IMAGM},
	{"conjg", 9, 0, F CONJGC, F CONJGM},

	{"exp", 8, F EXPX, F EXPC, 0},
	{"ln", 8, 0, F LNC, 0},
	{"log", 8, 0, F LOG10C, 0},
	{"sqrt", 8, 0, F SQRTC, 0},
	{"ffact", 8, F FFACTX, 0, 0},
	{"fact", 8, F FACTORIALX, 0, 0},
	{"not", 8, F NOTX, F NOTC, F NOTM},
	{"divisor", 8, F DIVISORX, 0, 0},
	{"prime", 8, F PRIMEX, 0, 0},
	{"isprime", 8, F ISPRIMEX, 0, 0},
	{"fibon", 8, F FIBONACCIX, 0, 0},
	{"random", 8, F RANDOMX, 0, 0},
	{"arg", 8, 0, F ARGC, 0},

	{"sin", 8, F SINX, F SINC, 0},
	{"cos", 8, F COSX, F COSC, 0},
	{"tan", 8, F TANX, F TANC, 0}, {"tg", 8, F TANX, F TANC, 0},
	{"cot", 8, F COTGX, F COTGC, 0}, {"cotg", 8, F COTGX, F COTGC, 0},
	{"sec", 8, F SECX, F SECC, 0},
	{"csc", 8, F CSCX, F CSCC, 0}, {"cosec", 8, F CSCX, F CSCC, 0},
	{"asin", 8, 0, F ASINC, 0}, {"arcsin", 8, 0, F ASINC, 0},
	{"acos", 8, 0, F ACOSC, 0}, {"arccos", 8, 0, F ACOSC, 0},
	{"atan", 8, F ATANX, F ATANC, 0}, {"atg", 8, F ATANX, F ATANC, 0}, {"arctan", 8, F ATANX, F ATANC, 0}, {"arctg", 8, F ATANX, F ATANC, 0},
	{"acot", 8, F ACOTGX, F ACOTGC, 0}, {"acotg", 8, F ACOTGX, F ACOTGC, 0}, {"arccot", 8, F ACOTGX, F ACOTGC, 0}, {"arccotg", 8, F ACOTGX, F ACOTGC, 0},
	{"asec", 8, 0, F ASECC, 0},
	{"acsc", 8, 0, F ACSCC, 0},

	{"sinh", 8, F SINHX, F SINHC, 0},
	{"cosh", 8, F COSHX, F COSHC, 0},
	{"tanh", 8, F TANHX, F TANHC, 0}, {"tgh", 8, F TANHX, F TANHC, 0},
	{"coth", 8, F COTGHX, F COTGHC, 0}, {"cotgh", 8, F COTGHX, F COTGHC, 0},
	{"sech", 8, 0, F SECHC, 0},
	{"csch", 8, 0, F CSCHC, 0},
	{"asinh", 8, 0, F ASINHC, 0}, {"argsinh", 8, 0, F ASINHC, 0},
	{"acosh", 8, 0, F ACOSHC, 0}, {"argcosh", 8, 0, F ACOSHC, 0},
	{"atanh", 8, 0, F ATANHC, 0}, {"atgh", 8, 0, F ATANHC, 0}, {"argtanh", 8, 0, F ATANHC, 0}, {"argtgh", 8, 0, F ATANHC, 0},
	{"acoth", 8, 0, F ACOTGHC, 0}, {"acotgh", 8, 0, F ACOTGHC, 0}, {"argcoth", 8, 0, F ACOTGHC, 0}, {"argcotgh", 8, 0, F ACOTGHC, 0},
	{"asech", 8, 0, F ASECHC, 0},
	{"acsch", 8, 0, F ACSCHC, 0},

	{"radtodeg", 8, F RADTODEGX, 0, 0},
	{"degtorad", 8, F DEGTORADX, 0, 0},
	{"radtograd", 8, F RADTOGRADX, 0, 0},
	{"gradtorad", 8, F GRADTORADX, 0, 0},
	{"degtograd", 8, F DEGTOGRADX, 0, 0},
	{"gradtodeg", 8, F GRADTODEGX, 0, 0},
	{"deg", 8, F DEGX, 0, 0},
	{"rad", 8, F RADX, 0, 0},
	{"grad", 8, F GRADX, 0, 0},
	{"todeg", 8, F TODEGX, 0, 0},
	{"torad", 8, F TORADX, 0, 0},
	{"tograd", 8, F TOGRADX, 0, 0},
	{"dms", 8, F DMSTODECX, 0, 0},
	{"todms", 8, F DECTODMSX, 0, 0},
	{"ftoc", 8, F FTOCX, 0, 0},
	{"ctof", 8, F CTOFX, 0, 0},

	//matrix operators
	{"transp", 9, 0, 0, F TRANSPM},
	{"invert", 8, 0, 0, F INVERTM},
	{"elim", 9, 0, 0, F ELIMM},
	{"solve", 9, 0, 0, F EQUSOLVEM},
	{"angle", CMDVARARG+2, 0, 0, F ANGLEM},
	{"vert", 120, 0, 0, F VERTM},
	{"width", 9, 0, 0, F WIDTHM},
	{"height", 9, 0, 0, F HEIGHTM},
	{"det", 8, 0, 0, F DETM},
	{"rank", 8, 0, 0, F RANKM},
	{"polynom", CMDVARARG+2, 0, 0, F POLYNOMM},
	{"matrix", CMDVARARG+2, 0, 0, F MATRIXM},
	{"swap", CMDVARARG+2, 0, 0, F SWAPM},
};

/*

 Functions with real operand, but imaginary result:
 conjg, polar, ^, #, sqrt, ln, log, asin, acos, acosh, atanh, acoth

 */
//---------------------------------------------------------------
void cerror(int id, char *txt)
{
	if(error) return;
	if((id==1060 || id==1063) && precision<prec2) return; //Trigonometric or MOD function operand is too big
	error=id;
	SetWindowText(hOut, lng(id, txt));
}

void cleanup()
{
	for(int i=0; i<numStack.len; i++){
		FREEM(numStack[i]);
	}
	numStack.len=0;
	opStack.len=0;
}

void Tvar::destroy()
{
	FREEM(oldx);
	FREEM(newx);
	delete[] name;
}

void Tfunc::destroy()
{
	delete[] name;
	delete[] body;
	deleteDarray(args);
}

bool skipComment(const char *&s)
{
	if(*s!='/' || s[1]!='*') return false;
	const char *s0 = s;
	s+=2;
	while(*s!='*' || s[1]!='/'){
		if(!*s){
			errPos= s= s0;
			cerror(964, "End of comment is missing");
			return false;
		}
		s++;
	}
	s++;
	//position is at the ending /
	return true;
}

void skipSpaces(const char *&s)
{
	while(*s==' ' || *s=='\t' || *s=='\r' || *s=='\n' || skipComment(s)) s++;
}

//return 0 if there is no string at s
//return 1 if string does not have double quotes
//return (count of double quotes + 1)
int skipString(const char *&s)
{
	int n=0;
	if(*s=='\"'){
		for(;;){
			n++;
			for(s++; *s!='\"'; s++){
				if(!*s){
					cerror(959, "Missing quote at the end of a printed text");
					return false;
				}
			}
			if(s[1]!='\"') break;
			//double quotes
			s++;
		}
		//position is at the ending "
	}
	return n;
}

//convert double qoutes to single quote
//result is not nul terminated !
void copyString(char *dest, const char *src)
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
//---------------------------------------------------------------
void doOp()
{
	int i;
	const Top *o;
	Tstack *t;
	void *fr, *fc, *fm;
	Complex a1, a2, y;

	if(error || opStack.len==0) return;
	t= opStack--;
	o= t->op;
	errPos = t->inputPtr;
	fr=o->func;
	fc=o->cfunc;
	fm=o->mfunc;
	i=o->type;
	if(i==1){
		//const function
		y=ALLOCC(precision);
		if(fm) ((TnularyC)fm)(y);
		else if(fr) ((Tnulary)fr)(y.r);
		else ((TnularyC)fc)(y);
		*numStack++=y;
	}
	else if(numStack.len>0 && (fr || fc || fm)){
		if(fc!=DECC && fc!=INCC) deref(numStack[numStack.len-1]);
		a1=numStack[numStack.len-1];
		if(i==8 || i==2){
			//unary _stdcall operator
			y=ALLOCC(precision);
			if(isMatrix(a1) || !fc && !fr){
				if(!fm) errMatrix();
				else ((TunaryC2)fm)(y, a1);
			}
			else if(isImag(a1) || !fr){
				if(!fc) errImag();
				else ((TunaryC2)fc)(y, a1);
			}
			else{
				((Tunary2)fr)(y.r, a1.r);
			}
			FREEM(a1);
			numStack[numStack.len-1]=y;
		}
		else if(i==9 || i>=400){
			//unary _fastcall operator
			if(isMatrix(a1) || !fc && !fr){
				if(!fm) errMatrix();
				else ((TunaryC0)fm)(a1);
			}
			else if(isImag(a1) || !fr){
				if(!fc) errImag();
				else ((TunaryC0)fc)(a1);
			}
			else{
				((Tunary0)fr)(a1.r);
			}
		}
		else{
			//binary operator
			if(numStack.len<2) return;
			numStack--;
			if(fm!=ASSIGNM) deref(numStack[numStack.len-1]);
			a2=numStack[numStack.len-1];
			y=ALLOCC(precision);
			if(isMatrix(a1) || isMatrix(a2) || !fc && !fr){
				if(!fm) errMatrix();
				else ((TbinaryC)fm)(y, a2, a1);
			}
			else if(isImag(a1) || isImag(a2) || !fr){
				if(!fc) errImag();
				else ((TbinaryC)fc)(y, a2, a1);
			}
			else{
				((Tbinary)fr)(y.r, a2.r, a1.r);
			}
			FREEM(a2);
			FREEM(a1);
			numStack[numStack.len-1]=y;
		}
	}
}
//---------------------------------------------------------------
bool isLetter(char c)
{
	return c>='a' && c<='z' || c>='A' && c<='Z';
}

bool isVarLetter(char c)
{
	return isLetter(c) || c=='_' || signed(c)<0
		|| c>='0' && c<='9';
}

Tvar *findVar(const char *s, const char **e)
{
	Tlen i;
	Tvar *v;
	const char *d, *a;
	char c;

	for(i=vars.len-1; i>=0; i--){
		v=&vars[i];
		for(d=v->name, a=s;; d++, a++){
			c=(char)tolower(*a);
			if(c!=tolower(*d) || !c) break;
		}
		if(*d=='\0' && !isVarLetter(c)){
			if(e) *e=a;
			return v;
		}
	}
	return 0;
}

//---------------------------------------------------------------
int findLabel(const char *s, int len)
{
	Tlen i;
	Tlabel *a;

	for(i=labels.len-1; i>=0; i--){
		a=&labels[i];
		if(len==a->nameLen && !strncmp(s, a->name, a->nameLen)){
			return a->ind;
		}
	}
	return -1;
}
//---------------------------------------------------------------
int getIndex()
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
		FREEC(x);
	}
	return result;
}


void arrayIndex(const char *&s)
{
	int D[2][2];
	int i;
	Complex y, x1;

	opStack--;
	D[0][0]=D[1][0]=-1;
	for(i=0;; i++){
		skipSpaces(s);
		if(*s!=']'){
			parse(s, &s);
			if(error) return;
			D[i][0]=D[i][1]=getIndex();
			if(*s==','){
				parse(s+1, &s);
				if(error) return;
				D[i][1]=getIndex();
			}
			if(*s!=']'){
				cerror(962, "] expected");
				return;
			}
		}
		s++;
		skipSpaces(s);
		if(i || *s!='[') break;
		s++;
	}

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
		deref(x1, x);
		y= ALLOCC(precision);
		INDEXM(y, x1, &D[0][0]);
		FREEM(x);
		x=y;
	}
}
//---------------------------------------------------------------
static const Top **opLastCompared;

struct TsearchInfo {
	char c1;
	const char *s;
};

int __cdecl cmpSearchOp(TsearchInfo *si, const Top **po)
{
	const char *d, *a;
	char c;
	int i;

	//compare the first character
	d=(*po)->name;
	i= (unsigned char)si->c1 - *(const unsigned char*)d;
	if(!i){
		//compare other characters
		for(a= si->s + 1, d++;; d++, a++){
			c=(char)tolower(*a);
			i= (unsigned char)c - *(const unsigned char*)d;
			if(i || !c) break;
		}
		if(*d=='\0' && (c<'a' || c>'z' || d[-1]<'a' || d[-1]>'z')) return 0; //match found
	}
	opLastCompared=po;
	return i;
}

int __cdecl cmpSearchOpS(const char *s, const Top **po)
{
	TsearchInfo si;
	si.s=s;
	si.c1=*s;
	return cmpSearchOp(&si, po);
}

int token(const char *&s, bool isFor=false, bool isPostfix=false)
{
	char c;
	const char *a;
	Complex x;
	const Top *o, **po;
	Tstack *t;
	Tvar *v;
	int varNameLen;
	TsearchInfo si;

	if(error) return -3;

	skipSpaces(s);
	errPos=s;
	c=*s;
	if(c=='\0' || c==';' || c==']' ||
		(c==',' && !inParenthesis)) return CMDEND;
	//read a number
	if(c>='0' && c<='9' || c=='.'){
		x=ALLOCC(precision);
		s=READX(x.r, s);
		if(*s>='0' && *s<='9'){
			errPos=s;
			cerror(954, "The digit is outside the selected base");
		}
		*numStack++=x;
		return 0;
	}
	if(c=='(') inParenthesis++;
	if(c==')') inParenthesis--;

	//store the first character to c1 for faster comparisons
	si.c1 = (char)tolower(*s);
	if(!isLetter(*s) || isLetter(s[1])){  //function name cannot be a single letter
		//find a function or an operator
		si.s = s;
		po= (const Top**)bsearch(&si, funcTabSorted, sizeA(funcTab), sizeof(Top*),
			(int(__cdecl *)(const void*, const void*)) cmpSearchOp);
		if(!po){
			po=opLastCompared;
			if(isLetter(si.c1)){
				//compare prefixes
				if(po>funcTabSorted+1 && !cmpSearchOpS(po[-1]->name, po-2)) po--;
				while(po>funcTabSorted && !cmpSearchOpS((*po)->name, po-1)) po--;
			}
			else{
				//compare all operators which start with the same character
				while(po>funcTabSorted && po[-1]->name[0]==si.c1 && cmpSearchOp(&si, po)) po--;
			}
			if(cmpSearchOp(&si, po)) po=0;
		}
		if(po){
			//one function can be prefix of another function,
			// find the longest function that match input string
			while(po < funcTabSorted+sizeA(funcTab)-1 && !cmpSearchOp(&si, po+1)) po++;
			//add operation to the stack
			t= opStack++;
			t->inputPtr= s;
			o= *po;
			t->op= o;
			//skip this token
			s+=strlen(o->name);
			return o->type;
		}
	}
	//operators i,j,'
	if(isPostfix){
		if(si.c1=='i' || si.c1=='j'){
			imagChar=si.c1;
			if(!isLetter(s[1])){
				t= opStack++;
				t->inputPtr= s;
				t->op= &opImag;
				s++;
				return opImag.type;
			}
		}
		if(si.c1=='\''){
			t= opStack++;
			t->inputPtr= s;
			t->op= &opTransp;
			s++;
			return opTransp.type;
		}
	}
	//variable
	varNameLen=0;
	for(a=s; isVarLetter(*a); a++) varNameLen++;
	if(varNameLen){
		v= findVar(s, 0);
		skipSpaces(a);
		if(!v && (*a=='=' && *(a+1)!='=' || isFor)){
			//create a new variable
			v= vars++;
			v->name= new char[varNameLen+1];
			v->name[varNameLen]=0;
			memcpy(v->name, s, varNameLen);
			v->newx.r= v->newx.i=0;
			v->oldx= ALLOCC(1);
			v->modif= false;
		}
		if(v){
			s=a;
			*numStack++= x= ALLOCC(precision); //deref needs this precision
			x.r[0]= int(v-vars);
			x.r[-3]= -1;
			return 0;
		}
	}
	//identifier not found
	c=*s;
	if(isLetter(c)){
		cerror(950, "Unknown function or variable");
	}
	else{
		cerror(951, "Unknown operator");
	}
	return -2;
}
//---------------------------------------------------------------
void skipArg(const char *s, const char **e)
{
	int d, b;
	char c;

	for(d=b=0;; s++){
		skipComment(s);
		skipString(s);
		c=*s;
		if(c=='(') d++;
		if(c==')'){
			if(!d) break;
			d--;
		}
		if(c=='[') b++;
		if(c==']') b--;
		if(c==0 || c==';' || (c==',' && !d && !b)) break;
	}
	*e=s;
}
//---------------------------------------------------------------
int args(const char *input, const char **end)
{
	unsigned i, n, ifCond=0;
	int j, len=0;
	const char *s, *e, *formula=0;
	Complex y, t, u, w, *stackEnd, *A=0;
	const Top *o;
	void *fr, *fc, *fm;
	Tstack stk;
	bool imag, matrix, isIf, isEach;

	stk = *opStack--;
	o= stk.op;
	isIf= o->func==IFX;
	s=input;
	skipSpaces(s);
	if(*s!='('){
		errPos=s;
		cerror(957, "Left parenthesis expected");
		return -2;
	}
	s++;

	for(i=1;; i++){
		if(unsigned(o->type)==CMDFOR+i){
			formula=s;
			skipArg(s, &e);
		}
		else if(o->type>=CMDFOR && i==1){
			j=token(s, true);
			e=s;
			if(j!=0 || !isVariable(numStack[numStack.len-1])){
				cerror(1057, "The first parameter has to be a variable");
			}
		}
		else{
			parse(s, &e);
		}
		if(error) return -1;
		if(*e!=',') break;
		s=e+1;
		if(isIf){
			if(i==1){
				deref(numStack[numStack.len-1]);
				y= *numStack--;
				ifCond= !isZero(y);
				FREEM(y);
			}
			if(i==1+ifCond){
				skipArg(s, &e);
				i++;
				if(*e!=',') break;
				s=e+1;
			}
		}
	}
	if(*e==')') e++;
	if(end) *end=e;
	n= o->type-CMDVARARG;
	if(n>50) n-=CMDFOR-CMDVARARG;
	if(n && i!=n){
		errPos=input;
		cerror(958, "Wrong number of arguments");
		return -3;
	}
	if(isIf) return 0;

	imag=matrix=false;
	stackEnd=&numStack[numStack.len];
	j=-1;
	if(formula) i-=2;
	for(; unsigned(-j)<=i; j--){
		Complex &s= stackEnd[j];
		if(o->mfunc!=SWAPM) deref(s);
		if(isMatrix(s)) matrix=true;
		if(isImag(s)) imag=true;
	}
	if(formula) i++;

	errPos = stk.inputPtr;
	y=ALLOCC(precision);
	fm=o->mfunc;
	fc=o->cfunc;
	if(o->type>=CMDFOR){
		if(fm==INTEGRALM){
			((void(*)(Complex, Complex, Complex, Complex, Complex, const char*))o->mfunc)(y, stackEnd[-3], stackEnd[-2], stackEnd[-1], stackEnd[-4], formula);
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
			for(j=0;; j++){
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
				parse(formula, &e);
				if(error) break;
				deref(*stackEnd);
				//add item to result
				if(fm){
					if(j){
						((TbinaryC)fm)(u, y, *stackEnd);
						w=y; y=u; u=w;
					}
					else{
						w=y; y=*stackEnd; *stackEnd=w;
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
	}
	else{
		//call the function           
		fr=o->func;
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
		else if(n==2){
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
lend:
	//free arguments
	while(i--){
		FREEM(*numStack--);
	}
	//store the result
	*numStack++= y;
	return 0;
}
//---------------------------------------------------------------
void parse(const char *input, const char **e)
{
	int t, u, inPar;
	Tlen stackBeg=opStack.len;
	Tstack stk;
	const char *s=input, *b;
	Complex a;

	inPar=inParenthesis;
	inParenthesis=0;
	for(;;){
		//prefix operators
		do{
			t=token(s);
			if(t==CMDPLUS){ t=8; opStack--; }
			if(t==CMDMINUS){
				opStack[opStack.len-1].op=&opNeg;
				t=opNeg.type;
			}
			if(t>=CMDVARARG && t<CMDVARARG+100){
				args(s, &s);
				t=0;
			}
			if(t==CMDBASE){
				//save current base
				*numStack++= a= ALLOCC(1);
				a.r[0]=baseIn;
				//set new base
				switch(errPos[2]){
					default:
						baseIn=0;
						while(*s>='0' && *s<='9'){
							baseIn= baseIn*10 + (*s-'0');
							s++;
						}
						break;
					case 'c': case 'C': baseIn=10; break;
					case 'x': case 'X': baseIn=16; break;
					case 'n': case 'N': baseIn=2; break;
					case 't': case 'T': baseIn=8; break;
				}
				if(!baseIn) errPos=s;
				else{
					skipSpaces(s);
					if(*s==';' || *s==0){
						t=0;
						opStack--;
						a.r[1]=1;
						a.r[-3]=-2;
					}
					else{
						t=8;
					}
				}
			}
			if(t==CMDGOTO){
				skipSpaces(s);
				for(b=s; isVarLetter(*b); b++);
				u=findLabel(s, int(b-s));
				if(u>=0){
					*numStack++= a= ALLOCC(2);
					SETC(a, u);
					s=b;
					t=0;
				}
			}
		} while(t>=8 && t<=9 || t>=400 && t<450);

		//constant function
		if(t==1) doOp();
		if(t>1){
			if(opStack.len>0 && opStack[opStack.len-1].op->mfunc==RETM
				&& (t==CMDEND || t==CMDRIGHT)){
				error=1102;
			}
			else{
				cerror(952, "Number, variable or prefix function expected");
			}
		}
	l2:
		//postfix operators
		t=token(s, false, true);
		if(t==2){
			if(opStack.len && opStack[opStack.len-1].op->mfunc==INDEXM){
				arrayIndex(s);
			}
			else{
				doOp();
			}
			goto l2;
		}
		//binary operators
		if(t<3 || t>=8 && t<=9 || t>=CMDVARARG || t>=400 && t<450){
			cerror(953, "Binary operator expected");
		}
		if(error) break;
		stk.op=0;
		stk.inputPtr=0;
		if(t!=CMDEND) stk=*opStack--;
		while(opStack.len>stackBeg && !error){
			u=opStack[opStack.len-1].op->type;
			if(t<u || t==CMDASSIGN && u==CMDASSIGN) break;
			doOp();
			if(u==CMDLEFT && t==CMDRIGHT){
				//prefix operators before parenthesis
				if(opStack.len>stackBeg && !error){
					u=opStack[opStack.len-1].op->type;
					if(u>=8 && u<=9) doOp();
				}
				goto l2;
			}
		}
		if(!stk.op){
			if(e) *e=s;
			break;
		}
		if(t==CMDRIGHT){
			if(e) *e=stk.inputPtr;
			break;
		}
		*opStack++=stk;
	}
	inParenthesis=inPar;
}
//---------------------------------------------------------------
void wrAns()
{
	if(error) return;
	char *buf= AWRITEM(ans, digits, matrixFormat);
	SetWindowText(hOut, buf);
	delete[] buf;
}
//---------------------------------------------------------------
void initLabels(const char *s)
{
	char c;
	bool quot=false;
	int n=0;
	const char *t;
	Tlabel *a;

	gotoPositions.reset();
	*gotoPositions++= s;
	labels.reset();

	for(;;){
		skipSpaces(s);
		//label
		for(t=s; isVarLetter(*t); t++);
		if(*t==':' && t>s){
			a= labels++;
			a->ind=n;
			a->name=s;
			a->nameLen=int(t-s);
		}
		//find semi-colon
		for(;;){
			c= *s;
			if(c==0){
				*gotoPositions++= s;
				return;
			}
			s++;
			if(c==';' && !quot){
				*gotoPositions++= s;
				n++;
				if(*s==0) return;
				break;
			}
			if(c=='\"') quot=!quot;
		}
	}
}
//---------------------------------------------------------------
void checkInfinite(Complex &y, Tint prec)
{
	if(y.r[-1]<-prec)
		ZEROX(y.r);
	if(y.i[-1]<-prec)
		ZEROX(y.i);
	if(y.r[-1]>prec && !isZero(y.r) || y.i[-1]>prec && !isZero(y.i)){
		cerror(1034, "Infinite result");
	}
}
//---------------------------------------------------------------
DWORD WINAPI threadLoop(char *param)
{
	DWORD time= getTickCount();
	char *output, *a;
	const char *e, *input, *s;
	char t[16];
	bool b, isPrint;
	Tvar *v;
	Complex y;
	Tlen i;
	int baseOld;
	int n;
	Darray<char> buf;

	SetDlgItemText(hWin, IDC_TIME, "");
	SetWindowText(hOut, "");
	output=0;
	cleanup();
	initLabels(param);

	//set the precision
	prec2= int(digits/dwordDigits[base])+2;
	amin(prec2, 8*32/TintBits);
	Nseed=prec2;
	baseOld=baseIn;

	for(precision= (prec2>60) ? (8*32/TintBits+1) : prec2;;){
		baseIn=baseOld;
		input=param;
		buf.setLen(1);
		buf[0]=0;
		for(i=vars.len-1; i>=0; i--){
			v=&vars[i];
			v->modif=false;
		}
		if(oldAns.r) assign(ans, oldAns);
		else assign(oldAns, ans);
		if(oldSeedx) assign(seedx, oldSeedx);
		else if(seedx) assign(oldSeedx, seedx);

		for(cmdNum=0;; cmdNum++){ //command cycle
			//label
			skipSpaces(input);
			for(s=input; isVarLetter(*s); s++);
			if(*s==':'){
				input=s+1;
				skipSpaces(input);
			}
			//print command
			isPrint= !_strnicmp(input, "print", 5);
			if(isPrint){
				input+=5;
				skipSpaces(input);
			}
		lcmd:
			if(*input=='\"' && isPrint){
				e=input;
				int doubleQuotes = skipString(e) - 1;
				input++;
				n= int(e-input)-doubleQuotes;
				a=(buf+=n)-1;
				if((unsigned)buf.len > MAX_OUTPUT_SIZE){
					buf-=n;
					cerror(1062, "Result is too long");
				}
				else{
					if(doubleQuotes>0) copyString(a, input);
					else memcpy(a, input, n);
					a[n]=0;
				}
				if(*e) e++;
				skipSpaces(e);
			}
			else if(!*input || *input==';'){
				//empty command
				e=input;
			}
			else{
				gotoPos=-1;
				retValue.r=retValue.i=0;
				inParenthesis=0;
				parse(input, &e);
				if(numStack.len!=1) cerror(1029, "Fatal error");
				if(!error){
					if(*e==',' && !isPrint) cerror(956, "Unmatched comma");
					if(*e==']') cerror(963, "Unmatched right bracket");
					if(*e==')') cerror(955, "Unmatched parenthesis");
				}
				if(gotoPos>=0){
					cleanup();
					if(gotoPos>=gotoPositions.len){
						errGoto();
					}
					else{
						input= gotoPositions[gotoPos];
						cmdNum=gotoPos-1;
						continue;
					}
				}
				if(retValue.r || error==1102){
					cleanup();
					y=retValue;
					e=strchr(input, 0);
					if(!y.r){
						InterlockedCompareExchange((Tinterlock*)&error, 0, (Tinterlock)1102);
						goto lout;
					}
					InterlockedCompareExchange((Tinterlock*)&error, 0, (Tinterlock)1101);
				}
				else{
					if(error){
						PostMessage(hWin, WM_INPUTCUR, 0, int(errPos-param));
						break;
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
						for(i=0; i<ym->len; i++){
							checkInfinite(ym->A[i], prec2);
						}
					}
				}
				if(error){
					FREEM(y);
					break;
				}
				if(*e!=';' || isPrint){
					//convert the result to a string
					n=buf.len;
					a= (buf+=LENM(y, digits))-1;
					if((unsigned)buf.len>MAX_OUTPUT_SIZE){
						buf.len=n;
						cerror(1062, "Result is too long");
					}
					else{
						WRITEM(a, y, digits, matrixFormat);
						buf.setLen(n+strleni(a));
					}
				}
				//set Ans
				FREEM(ans);
				ans=y;
			}
			if(error) break;
			input=e+1;
			if(isPrint){
				if(*e==','){
					skipSpaces(input);
					if(*input!=';' && *input){
						//space
						a=(buf++);
						a[-1]=' ';
						a[0]=0;
						goto lcmd;
					}
				}
				else{
					//new line
					a=(buf+=2);
					a[-1]='\r';
					a[0]='\n';
					a[1]=0;
				}
			}
			if(!*e) break;
		}
	lout:
		if(error) break;
		//compare with the previous result
		b= output && !strcmp(output, buf);
		delete[] output;
		output=buf.array;
		buf.array= new char[buf.capacity];
		if(b) break;
		SetWindowText(hOut, output);
		if(precision>=prec2){
			precision++;
		}
		else{
			precision=prec2;
		}
	}
	if(!error || error==1100){
		//display calculation time
		sprintf(t, "%u ms", getTickCount()-time);
		SetDlgItemText(hWin, IDC_TIME, t);
		//save to log file
		if(log && *fnLog && output){
			HANDLE f= createFile(fnLog, OPEN_ALWAYS);
			SetFilePointer(f, 0, 0, FILE_END);
			saveResult(f, "%1\r\n =\r\n%2\r\n--------------------\r\n", /* format string is in saveResult, too */
				param, output, true);
		}
	}
	else{
		for(i=vars.len-1; i>=0; i--){
			vars[i].modif=false;
		}
	}
	delete[] output;
	delete[] param;
	return 0;
}
//---------------------------------------------------------------
int stop()
{
	static int lock=0;

	if(thread){
		if(lock) return 1;
		lock++;
		//abort previous calculation
		error=1100;
		while(MsgWaitForMultipleObjects(1, &thread, FALSE, INFINITE, QS_ALLINPUT)==
			WAIT_OBJECT_0+1){
			MSG mesg;
			while(PeekMessage(&mesg, NULL, 0, 0, PM_REMOVE)){
				if(mesg.message==WM_CLOSE){
					PostMessage(hWin, WM_CLOSE, 0, 0);
					return 2;
				}
				processMessage(mesg);
			}
		}
		CloseHandle(thread);
		thread=0;
		lock--;
	}
	return 0;
}

void calc(char *input)
{
	if(stop()) return;
	//get the current base and precision
	base= GetDlgItemInt(hWin, IDC_BASE, 0, FALSE);
	if(base<2 || base>36){
		base= base<2 ? 10 : 36;
		SetDlgItemInt(hWin, IDC_BASE, base, FALSE);
	}
	baseIn= base;
	digits= GetDlgItemInt(hWin, IDC_PRECISION, 0, FALSE);
	fixDigits= GetDlgItemInt(hWin, IDC_FIXDIGITS, 0, FALSE);
	error=0;
	//start a working thread
	DWORD threadId;
	thread= CreateThread(0, 0, (LPTHREAD_START_ROUTINE)threadLoop, input, 0, &threadId);
}

void calc()
{
	//copy content of the input edit box
	char *input= getIn();
	//add to history
	forl2(TstrItem, history){
		if(!strcmp(input, item->s)){
			delete item;
			break;
		}
	}
	history.append(new TstrItem(input));
	curHistory=0;
	amin(maxHistory, 2);
	while(historyLen>maxHistory){
		delete history.remove();
	}
	//calculate
	calc(input);
}

void nextAns()
{
	if(stop()) return;
	assign(oldAns, ans);
	assign(oldSeedx, seedx);
	for(Tlen i=vars.len-1; i>=0; i--){
		Tvar *v=&vars[i];
		if(v->modif){
			Complex w=v->newx; v->newx=v->oldx; v->oldx=w;
			v->modif=false;
		}
	}
}

static int __cdecl cmpOp(const void *a, const void *b)
{
	return _stricmp((*(Top**)a)->name, (*(Top**)b)->name);
}

void initFuncTab()
{
	funcTabSorted = new const Top*[sizeA(funcTab)];
	for(int i=0; i<sizeA(funcTab); i++){
		funcTabSorted[i]= funcTab + i;
	}
	qsort(funcTabSorted, sizeA(funcTab), sizeof(Top*), cmpOp);


	//FILE *f=fopen("C:\\a\\fnc.txt", "wt");
	//char H=' ';
	//for(int i=0; i<sizeA(funcTab); i++){
	// char c=funcTabSorted[i]->name[0];
	// if(c!=H){
	//  H=c;
	//  if(c>='a' && c<='z'){

	//	fprintf(f,"<b>%c</b><br>\n",c + 'A'-'a');
	//  }
	// }
	// fprintf(f,"<a href=\"#%s\">%s</a><br>\n", funcTabSorted[i]->name, funcTabSorted[i]->name);
	//}
	//fclose(f);
}

//---------------------------------------------------------------