/*
	(C) Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include <ctype.h>
#include "preccalc.h"

typedef long Tinterlock;

const unsigned MAX_OUTPUT_SIZE=1000000000;

struct Tstack {
	const Top *op;
	const char *inputPtr;
};

struct Tlabel {
	const char *name;
	int nameLen;
	int ind;
};

int digits=40;
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

extern "C" void *Alloc(int size)
{
	return operator new(size);
}
extern "C" void Free(void *s)
{
	operator delete(s);
}

int strleni(char *s)
{
	size_t len = strlen(s);
	if(len>0x7fffff00){ exit(99); }
	return (int)len;
}

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

void FILTERM() {};

//-------------------------------------------------------------------

const int CMDBASE=3, CMDPLUS=144, CMDMINUS=143, CMDASSIGN=397,
CMDLEFT=401, CMDGOTO=402, CMDRIGHT=455, CMDEND=460, CMDVARARG=500,
	CMDFOR=550;

#define F (void*)

const Top opNeg={0, 9, F NEGX, F NEGC, F NEGM};
const Top opImag={0, 2, 0, F ISUFFIXC, 0};
const Top opTransp={0, 2, 0, 0, F TRANSP2M};
char degSymbol[4];

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
// unlisted names and descriptions of buttons are defined in preccalc.cpp near comment "custom buttons"
// NULL description means same description as in previous member
const Top funcTab[]={
	{")", CMDRIGHT, 0, 0, 0, 2036, "Closing parenthesis"},
	{"(", CMDLEFT, 0, 0, 0, 2034, "Opening parenthesis"},
	{",", 398, 0, 0, F CONCATM, 2075, "List divider"},
	{"\\", 399, 0, 0, F CONCATROWM, 2094, "Matrix column divider"},
	{"return", 400, 0, 0, F RETM, 2091, "Stop computation and display result"},
	{"goto", CMDGOTO, F GOTOX, 0, 0, 2069, "Jump to label or to n-th semicolon"},
	{"gotor", 400, F GOTORELX, 0, 0, 2089, "Relative jump to command"},
	{"base", CMDBASE, 0, 0, F SETBASEX, 2095, "N-th number base"},
	{"dec", CMDBASE, 0, 0, F SETBASEX, 2096, "Decimal number"},
	{"hex", CMDBASE, 0, 0, F SETBASEX, 2097, "Hexadecimal number"},
	{"bin", CMDBASE, 0, 0, F SETBASEX, 2098, "Binary number"},
	{"oct", CMDBASE, 0, 0, F SETBASEX, 2099, "Octal number"},

	//statistical functions
	{"min", 8, 0, 0, F MINM, 2041, "Minimal value"},
	{"max", 8, 0, 0, F MAXM, 2043, "Maximal value"},
	{"med", 8, 0, 0, F MEDIANM, 2045, "Median value"},
	{"mode", 8, 0, 0, F MODEM, 2081, "Most frequent value"},
	{"sort", 9, 0, 0, F SORTM, 2100, "Sort items from lesser to greater"},
	{"sortd", 9, 0, 0, F SORTDM, 2101, "Sort items from greater to lesser"},
	{"reverse", 9, 0, 0, F REVERSEM, 2102, "Reverse order of items from last to first"},
	{"sum", 8, 0, 0, F SUM1M, 2051, "Sum"},
	{"sumx", 8, 0, 0, F SUMXM, 2127, "Sum of column X of 2-column matrix"},
	{"sumy", 8, 0, 0, F SUMYM, 2128, "Sum of column Y of 2-column matrix"},
	{"sumq", 8, 0, 0, F SUM2M, 2053, "Sum of squares"},
	{"sumxq", 8, 0, 0, F SUMX2M, 2131, "Sum of squared column X of 2-column matrix"},
	{"sumyq", 8, 0, 0, F SUMY2M, 2132, "Sum of squared column Y of 2-column matrix"},
	{"sumxy", 8, 0, 0, F SUMXYM, 2133, "Sum of X*Y columns of 2-column matrix"},
	{"geom", 8, 0, 0, F GEOMM, 2103, "Geometric mean, N-th root of product of N items"},
	{"agm", CMDVARARG+2, F AGMX, 0, 0, 2040, "Arithmetic–geometric mean"},
	{"harmon", 8, 0, 0, F HARMONM, 2104, "Harmonic mean"},
	{"product", 8, 0, 0, F PRODUCTM, 2105, "Product of items"},
	{"count", 9, 0, 0, F COUNTM, 2067, "Count of matrix elements"},
	{"ave", 8, 0, 0, F AVE1M, 2061, "Average (mean)"}, {"mean", 8, 0, 0, F AVE1M, 2061, "Mean (average)"},
	{"avex", 8, 0, 0, F AVEXM, 2129, "Mean of column X of 2-column matrix"}, {"meanx", 8, 0, 0, F AVEXM, 0, NULL},
	{"avey", 8, 0, 0, F AVEYM, 2130, "Mean of column Y of 2-column matrix"}, {"meany", 8, 0, 0, F AVEYM, 0, NULL},
	{"aveq", 8, 0, 0, F AVE2M, 2063, "Mean of squares"}, {"meanq", 8, 0, 0, F AVE2M, 0, NULL},
	{"avexq", 8, 0, 0, F AVEX2M, 2134, "Mean of squared column X of 2-column matrix"}, {"meanxq", 8, 0, 0, F AVEX2M, 0, NULL},
	{"aveyq", 8, 0, 0, F AVEY2M, 2135, "Mean of squared column Y of 2-column matrix"}, {"meanyq", 8, 0, 0, F AVEY2M, 0, NULL},
	{"vara", 8, 0, 0, F VAR0M, 2080, "Variance of population"},
	{"varxa", 8, 0, 0, F VARX0M, 2138, "Variance of column X population of 2-column matrix"},
	{"varya", 8, 0, 0, F VARY0M, 2139, "Variance of column Y population of 2-column matrix"},
	{"var", 8, 0, 0, F VAR1M, 2055, "Variance"},
	{"varx", 8, 0, 0, F VARX1M, 2136, "Variance of column X of 2-column matrix"},
	{"vary", 8, 0, 0, F VARY1M, 2137, "Variance of column Y of 2-column matrix"},
	{"stdeva", 8, 0, 0, F STDEV0M, 2049, "Standard deviation of population"},
	{"stdevxa", 8, 0, 0, F STDEVX0M, 2140, "Standard deviation of column X population of 2-column matrix"},
	{"stdevya", 8, 0, 0, F STDEVY0M, 2141, "Standard deviation of column Y population of 2-column matrix"},
	{"stdev", 8, 0, 0, F STDEV1M, 2065, "Standard deviation"},
	{"stdevx", 8, 0, 0, F STDEVX1M, 2142, "Standard deviation of column X of 2-column matrix"},
	{"stdevy", 8, 0, 0, F STDEVY1M, 2143, "Standard deviation of column Y of 2-column matrix"},
	{"lra", 8, 0, 0, F LRAM, 2148, "Linear regression coeficient A of y=A+B*x"},
	{"lrb", 8, 0, 0, F LRBM, 2149, "Linear regression coeficient B of y=A+B*x"},
	{"lrr", 8, 0, 0, F LRRM, 2150, "Linear regression correlation coeficient"},
	{"lrx", CMDVARARG+2, 0, 0, F LRXM, 2151, "X corresponding to Y for y=A+B*x"},
	{"lry", CMDVARARG+2, 0, 0, F LRYM, 2152, "Y corresponding to X for y=A+B*x"},

	{"lcm", 8, 0, 0, F LCMM, 2006, "Least common multiplier"},
	{"gcd", 8, 0, 0, F GCDM, 2005, "Greatest common divisor"},
	{"if", CMDVARARG+3, F IFX, 0, 0, 2088, "Conditional selection"},

	{"integral", CMDFOR+5, 0, 0, F INTEGRALM, 2153, "(x,a,b,n,f()); Integral from f() for x = a...b; n = precision"},
	{"for", CMDFOR+4, 0, 0, 0, 2154, "(x,a,b,f()); Executes f() for x = a...b, returns 0"},
	{"foreach", CMDFOR+3, 0, 0, 0, 2155, "(x,A,f()); Executes f() for x = every element of matrix A, returns 0"},
	{"sumfor", CMDFOR+4, 0, 0, F PLUSM, 2156, "(x,a,b,f()); Sum of f() for x = a...b"},
	{"sumforeach", CMDFOR+3, 0, 0, F PLUSM, 2157, "(x,A,f()); Sum of f() for x = every element of matrix A"},
	{"productfor", CMDFOR+4, 0, F ONEC, F MULTM, 2158, "(x,a,b,f()); Product of f() for x = a...b"},
	{"productforeach", CMDFOR+3, 0, F ONEC, F MULTM, 2159, "(x,A,f()); Product of f() for x = every element of matrix A"},
	{"listfor", CMDFOR+4, 0, 0, F CONCATM, 2160, "(x,a,b,f()); Row vector of f() for x = a...b"},
	{"listforeach", CMDFOR+3, 0, 0, F CONCATM, 2161, "(x,A,f()); Row vector of f() for x = every element of matrix A"},
	{"rowsfor", CMDFOR+4, 0, 0, F CONCATROWM, 2162, "(x,a,b,f()); Column vector of f() for x = a...b"},
	{"rowsforeach", CMDFOR+3, 0, 0, F CONCATROWM, 2163, "(x,A,f()); Column vector of f() for x = every element of matrix A"},
	{"minfor", CMDFOR+4, 0, 0, F MIN3M, 2164, "(x,a,b,f()); Minimal result of f() for x = a...b"},
	{"minforeach", CMDFOR+3, 0, 0, F MIN3M, 2165, "(x,A,f()); Minimal result of of f() for x = every element of matrix A"},
	{"maxfor", CMDFOR+4, 0, 0, F MAX3M, 2166, "(x,a,b,f()); Maximal result of f() for x = a...b"},
	{"maxforeach", CMDFOR+3, 0, 0, F MAX3M, 2167, "(x,A,f()); Maximal result of f() for x = every element of matrix A"},
	{"filterfor", CMDFOR+4, 0, F EMPTYM, F FILTERM, 2054, "Values that meet the condition"},
	{"filterforeach", CMDFOR+3, 0, F EMPTYM, F FILTERM, 2060, "Filter matrix by condition"},

	{"hypot", CMDVARARG+2, F HYPOTX, 0, 0, 2106, "Hypotenuse"},
	{"polar", CMDVARARG+2, 0, F POLARC, 0, 2107, "Convert to polar coordinates"},
	{"complex", CMDVARARG+2, 0, F COMPLEXC, 0, 2108, "Make complex number"},
	{"logn", CMDVARARG+2, 0, F LOGNC, 0, 2109, "Logarithm"},

	{"++", 2, 0, F INCC, 0, 2110, "Increment"},
	{"--", 2, 0, F DECC, 0, 2111, "Decrement"},

	//binary operators
	{"or", 199, F ORX, F ORC, F ORM, 2013, "Bitwise OR (inclusive OR, bitwise addition)"}, {"|", 199, F ORX, F ORC, F ORM, 0, NULL},
	{"nor", 199, F NORX, 0, 0, 2168, "NOT OR   =   0==(x!=0 | y!=0)   =   x==0 & y==0"},
	{"bitnor", 199, F NORBX, F NORBC, F NORBM, 2169, "Bitwise NOT OR   =   not(x | y)   =   not x & not y"},
	{"xor", 198, F XORX, F XORC, F XORM, 2014, "Bitwise XOR (eXclusive OR, bitwise difference)"},
	{"and", 197, F ANDX, F ANDC, F ANDM, 2011, "Bitwise AND (bitwise multiplication)"}, {"&", 197, F ANDX, F ANDC, F ANDM, 0, NULL},
	{"nand", 197, F NANDX, 0, 0, 2170, "NOT AND   =   0==(x!=0 & y!=0)   =   x==0 | y==0"},
	{"bitnand", 197, F NANDBX, F NANDBC, F NANDBM, 2171, "Bitwise NOT AND   =   not(x & y)   =   not x | not y"},
	{"imp", 200, F IMPX, 0, 0, 2172, "x==0 | y!=0"}, {"->", 200, F IMPX, 0, 0, 0, NULL},
	{"bitimp", 200, F IMPBX, F IMPBC, F IMPBM, 2173, "Bitwise (y or not x)"},
	{"eqv", 201, F EQVX, 0, 0, 2112, "Both zero or both non-zero"},
	{"biteqv", 201, F EQVBX, F EQVBC, F EQVBM, 2113, "Equal bits in both numbers"},
	{"==", 171, F EQUALX, F EQUALC, F EQUALM, 2082, "Equal"},
	{"<>", 171, F NOTEQUALX, F NOTEQUALC, F NOTEQUALM, 2083, "Not equal"}, {"!=", 171, F NOTEQUALX, F NOTEQUALC, F NOTEQUALM, 0, NULL},
	{">=", 170, F GREATEREQX, 0, 0, 2087, "Greater or equal"},
	{"<=", 170, F LESSEQX, 0, 0, 2086, "Less or equal"},
	{"lsh", 155, F LSHX, F LSHC, F LSHM, 2015, "Shift bits left"}, {"shl", 155, F LSHX, F LSHC, F LSHM, 0, NULL}, {"<<", 155, F LSHX, F LSHC, F LSHM, 0, NULL},
	{"rshi", 155, F RSHIX, F RSHIC, F RSHIM, 2016, "Shift bits right, discard fractional part"}, {"shri", 155, F RSHIX, F RSHIC, F RSHIM, 0, NULL}, {">>>", 155, F RSHIX, F RSHIC, F RSHIM, 0, NULL},
	{"rsh", 155, F RSHX, F RSHC, F RSHM, 2114, "Shift bits right"}, {"shr", 155, F RSHX, F RSHC, F RSHM, 0, NULL}, {">>", 155, F RSHX, F RSHC, F RSHM, 0, NULL},
	{"=", CMDASSIGN, 0, 0, F ASSIGNM, 2071, "Equation sign, assignment"},
	{">", 170, F GREATERX, 0, 0, 2085, "Greater"},
	{"<", 170, F LESSX, 0, 0, 2084, "Less"},
	{"+", CMDPLUS, F PLUSX, F PLUSC, F PLUSM, 2066, "Addition"},
	{"-", CMDMINUS, F MINUSX, F MINUSC, F MINUSM, 2068, "Substraction"},
	{"**", 5, 0, F POWC, F POWM, 2038, "Power"}, {"^", 5, 0, F POWC, F POWM, 0, NULL},
	{"*", 121, F MULTX, F MULTC, F MULTM, 2056, "Multiplication"},
	{"mod", 121, F MODX, 0, 0, 2059, "Remainder after division"}, {"%", 121, F MODX, 0, 0, 0, NULL},
	{"div", 121, F IDIVX, 0, 0, 2115, "Integer division"},
	{"/", 121, F DIVX, F DIVC, F DIVM, 2058, "Division"},
	{"combin", 111, F COMBINX, 0, 0, 2007, "Number of combinations"}, {"ncr", 111, F COMBINX, 0, 0, 0, NULL},
	{"permut", 111, F PERMUTX, 0, 0, 2008, "Number of permutations"}, {"npr", 111, F PERMUTX, 0, 0, 0, NULL},
	{"#", 4, 0, F ROOTC, 0, 2039, "Root"},

	//postfix operators
	{"!!", 2, F FFACTX, 0, 0, 2042, "Double factorial"},
	{"!", 2, F FACTORIALX, 0, 0, 2003, "Factorial"},
	{"[", 2, 0, 0, F INDEXM, 2174, "Index of matrix element"},
	{degSymbol, 2, F DEGX, 0, 0, 2116, "Degrees symbol"},

	//constants
	{"pi", 1, F PIX, 0, 0, 2024, "Pi constant"},
	{"ans", 1, 0, 0, F ANS, 2076, "Result of previous calculation"},
	{"rand", 1, F RANDX, 0, 0, 2057, "Random fractional number, 0 =< ... < 1"},
	{"()", 1, 0, 0, F EMPTYM, 2062, "Empty matrix"},

	//prefix operators
	{"abs", 9, F ABSX, F ABSC, F ABSM, 2032, "Absolute value"},
	{"sign", 9, F SIGNX, F SIGNC, 0, 2033, "Sign of number"},
	{"round", 9, F ROUNDX, F ROUNDC, F ROUNDM, 2001, "Round to nearest integer"},
	{"trunc", 9, F TRUNCX, F TRUNCC, F TRUNCM, 2010, "Truncate to integer"},
	{"int", 9, F INTX, F INTC, F INTM, 2002, "Round to smaller integer"}, {"floor", 9, F INTX, F INTC, F INTM, 0, NULL},
	{"ceil", 9, F CEILX, F CEILC, F CEILM, 2004, "Round to larger integer"},
	{"frac", 9, F FRACX, F FRACC, F FRACM, 2009, "Fractional part"},
	{"real", 9, 0, F REALC, F REALM, 2035, "Real part of number"},
	{"imag", 9, 0, F IMAGC, F IMAGM, 2037, "Imaginary part of number"},
	{"conjg", 9, 0, F CONJGC, F CONJGM, 2117, "Conjugate"},

	{"exp", 8, F EXPX, F EXPC, 0, 2019, "Exponential function (2.718 ^ X)"},
	{"ln", 8, 0, F LNC, 0, 2021, "Natural logarithm"},
	{"log", 8, 0, F LOG10C, 0, 2022, "Common logarithm"},
	{"sqrt", 8, 0, F SQRTC, 0, 2017, "Square root"},
	{"ffact", 8, F FFACTX, 0, 0, 2042, "Double factorial"},
	{"fact", 8, F FACTORIALX, 0, 0, 2003, "Factorial"},
	{"not", 8, F NOTX, F NOTC, F NOTM, 2012, "Bitwise NOT (inversion)"},
	{"divisor", 8, F DIVISORX, 0, 0, 2092, "The least prime divisor of an operand"},
	{"prime", 8, F PRIMEX, 0, 0, 2093, "1st prime number greater than operand"},
	{"isprime", 8, F ISPRIMEX, 0, 0, 2118, "Check whether the number is prime"},
	{"fibon", 8, F FIBONACCIX, 0, 0, 2119, "Fibonacci number, 0,1,1,2,3,5,8,..."},
	{"random", 8, F RANDOMX, 0, 0, 2079, "Random integer, 0 =< ... < N"},
	{"arg", 8, 0, F ARGC, 0, 2120, "Phase angle"},

	{"sin", 8, F SINX, F SINC, 0, 2026, "sine"},
	{"cos", 8, F COSX, F COSC, 0, 2028, "cosine"},
	{"tan", 8, F TANX, F TANC, 0, 2030, "tangent"}, {"tg", 8, F TANX, F TANC, 0, 0, NULL},
	{"cot", 8, F COTGX, F COTGC, 0, 2121, "cotangent"}, {"cotg", 8, F COTGX, F COTGC, 0, 0, NULL},
	{"sec", 8, F SECX, F SECC, 0, 2122, "secant"},
	{"csc", 8, F CSCX, F CSCC, 0, 2123, "cosecant"}, {"cosec", 8, F CSCX, F CSCC, 0, 0, NULL},
	{"asin", 8, 0, F ASINC, 0, 2027, "inverse sine"}, {"arcsin", 8, 0, F ASINC, 0, 0, NULL},
	{"acos", 8, 0, F ACOSC, 0, 2029, "inverse cosine"}, {"arccos", 8, 0, F ACOSC, 0, 0, NULL},
	{"atan", 8, F ATANX, F ATANC, 0, 2031, "inverse tangent"}, {"atg", 8, F ATANX, F ATANC, 0, 0, NULL}, {"arctan", 8, F ATANX, F ATANC, 0, 0, NULL}, {"arctg", 8, F ATANX, F ATANC, 0, 0, NULL},
	{"acot", 8, F ACOTGX, F ACOTGC, 0, 2124, "inverse cotangent"}, {"acotg", 8, F ACOTGX, F ACOTGC, 0, 0, NULL}, {"arccot", 8, F ACOTGX, F ACOTGC, 0, 0, NULL}, {"arccotg", 8, F ACOTGX, F ACOTGC, 0, 0, NULL},
	{"asec", 8, 0, F ASECC, 0, 2125, "inverse secant"},
	{"acsc", 8, 0, F ACSCC, 0, 2126, "inverse cosecant"},

	{"sinh", 8, F SINHX, F SINHC, 0, 2144, "hyperbolic sine"},
	{"cosh", 8, F COSHX, F COSHC, 0, 2145, "hyperbolic cosine"},
	{"tanh", 8, F TANHX, F TANHC, 0, 2146, "hyperbolic tangent"}, {"tgh", 8, F TANHX, F TANHC, 0, 0, NULL},
	{"coth", 8, F COTGHX, F COTGHC, 0, 2147, "hyperbolic cotangent"}, {"cotgh", 8, F COTGHX, F COTGHC, 0, 0, NULL},
	{"sech", 8, 0, F SECHC, 0, 2175, "hyperbolic secant"},
	{"csch", 8, 0, F CSCHC, 0, 2176, "hyperbolic cosecant"},
	{"asinh", 8, 0, F ASINHC, 0, 2177, "hyperbolic inverse sine"}, {"argsinh", 8, 0, F ASINHC, 0, 0, NULL},
	{"acosh", 8, 0, F ACOSHC, 0, 2178, "hyperbolic inverse cosine"}, {"argcosh", 8, 0, F ACOSHC, 0, 0, NULL},
	{"atanh", 8, 0, F ATANHC, 0, 2179, "hyperbolic inverse tangent"}, {"atgh", 8, 0, F ATANHC, 0, 0, NULL}, {"argtanh", 8, 0, F ATANHC, 0, 0, NULL}, {"argtgh", 8, 0, F ATANHC, 0, 0, NULL},
	{"acoth", 8, 0, F ACOTGHC, 0, 2180, "hyperbolic inverse cotangent"}, {"acotgh", 8, 0, F ACOTGHC, 0, 0, NULL}, {"argcoth", 8, 0, F ACOTGHC, 0, 0, NULL}, {"argcotgh", 8, 0, F ACOTGHC, 0, 0, NULL},
	{"asech", 8, 0, F ASECHC, 0, 2181, "hyperbolic inverse secant"},
	{"acsch", 8, 0, F ACSCHC, 0, 2182, "hyperbolic inverse cosecant"},

	{"radtodeg", 8, F RADTODEGX, 0, 0, 2183, "Radians => degrees (pi == 180 deg)"},
	{"degtorad", 8, F DEGTORADX, 0, 0, 2184, "Degrees => radians (180 deg == pi)"},
	{"radtograd", 8, F RADTOGRADX, 0, 0, 2185, "Radians => gradients (pi == 200 grad)"},
	{"gradtorad", 8, F GRADTORADX, 0, 0, 2186, "Gradients => radians (200 grad == pi)"},
	{"degtograd", 8, F DEGTOGRADX, 0, 0, 2187, "Degrees => gradients (90 deg == 100 grad)"},
	{"gradtodeg", 8, F GRADTODEGX, 0, 0, 2188, "Gradients => degrees (100 grad == 90 deg)"},
	{"deg", 8, F DEGX, 0, 0, 2189, "Next value is in degrees"},
	{"rad", 8, F RADX, 0, 0, 2190, "Next value is in radians"},
	{"grad", 8, F GRADX, 0, 0, 2191, "Next value is in gradients"},
	{"todeg", 8, F TODEGX, 0, 0, 2192, "Current angle units => degrees"},
	{"torad", 8, F TORADX, 0, 0, 2193, "Current angle units => radians"},
	{"tograd", 8, F TOGRADX, 0, 0, 2194, "Current angle units => gradients"},
	{"dms", 8, F DMSTODECX, 0, 0, 2025, "Degrees, minutes, seconds => degrees"},
	{"todms", 8, F DECTODMSX, 0, 0, 2195, "Degrees => degrees, minutes, seconds"},
	{"ftoc", 8, F FTOCX, 0, 0, 2196, "Fahrenheit => Celcius degrees"},
	{"ctof", 8, F CTOFX, 0, 0, 2197, "Celcius => Fahrenheit degrees"},

	//matrix operators
	{"transp", 9, 0, 0, F TRANSPM, 2198, "Transpose (swap columms and rows, diagonal symmetry)"},
	{"invert", 8, 0, 0, F INVERTM, 2199, "Inverted matrix, 1/A"},
	{"elim", 9, 0, 0, F ELIMM, 2044, "Gaussian elimination"},
	{"solve", 9, 0, 0, F EQUSOLVEM, 2050, "Solve system of linear equations"},
	{"angle", CMDVARARG+2, 0, 0, F ANGLEM, 2200, "Angle between 2 vectors"},
	{"vert", 120, 0, 0, F VERTM, 2201, "Vectors product"},
	{"width", 9, 0, 0, F WIDTHM, 2202, "Number of matrix columns"},
	{"height", 9, 0, 0, F HEIGHTM, 2203, "Number of matrix rows"},
	{"det", 8, 0, 0, F DETM, 2204, "Determinant"},
	{"rank", 8, 0, 0, F RANKM, 2205, "Count of linearly independent rows"},
	{"polynom", CMDVARARG+2, 0, 0, F POLYNOMM, 2052, "Polynom value"},
	{"matrix", CMDVARARG+2, 0, 0, F MATRIXM, 2206, "(r,c); Make zero matrix with R rows and C columns"},
	{"swap", CMDVARARG+2, 0, 0, F SWAPM, 2207, "Exchange variables a and b"},
};
const int funcTab_size = sizeA(funcTab);

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
#ifdef CONSOLE
	puts(lng(id, txt));
#else
	SetWindowTextT(hOut, lng(id, txt));
#endif
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
				if(!fm) cerror(1065, "The function requires one parameter");
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
		FREEM(x);
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
	const Top *o, **po, **po1;
	Tstack *t;
	Tvar *v;
	int i, varNameLen;
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
	if(c=='(' && s[1]!=')') inParenthesis++;
	if(c==')') inParenthesis--;

	//store the first character to c1 for faster comparisons
	si.c1 = (char)tolower(c);
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
			for (po1 = po + 1; po1 < funcTabSorted + sizeA(funcTab); po1++) {
				i = cmpSearchOp(&si, po1);
				if (i < 0) break;
				if (i == 0) po = po1;
			}
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
#ifdef CONSOLE
		if(c=='"' && (!s[1] || s[1]==' ' && !s[2])) { s++; return CMDEND; }
#endif
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
		Complex &v= stackEnd[j];
		if(o->mfunc!=SWAPM) deref(v);
		if(isMatrix(v)) matrix=true;
		if(isImag(v)) imag=true;
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
				parse(formula, &e);
				if(error) break;
				stackEnd=&numStack[numStack.len-1];
				deref(*stackEnd);
				//add item to result
				if(fm==FILTERM) {
					if(!isZero(*stackEnd)) {
						CONCATM(u, y, toVariable(stackEnd[isEach ? -2 : -3])->newx);
						w=y; y=u; u=w;
					}
				}
				else if(fm) {
					if(j) {
						((TbinaryC)fm)(u, y, *stackEnd);
						w=y; y=u; u=w;
					} else {
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
				//prefix operators before parenthesis have higher priority, except minus (for example -(3)^2)
				if(opStack.len>stackBeg && !error){
					const Top *o=opStack[opStack.len-1].op;
					u=o->type;
					if(u>=8 && u<=9 && o!=&opNeg) doOp();
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
void ClearError(int err)
{
	InterlockedCompareExchange((Tinterlock*)&error, 0, (Tinterlock)err);
}
//---------------------------------------------------------------
DWORD WINAPI calcThread(char *param)
{
	char *output, *a;
	const char *e, *input, *s;
	bool b, isPrint, isGrey;
	Tvar *v;
	Complex y;
	Tlen i;
	int baseOld;
	int n;
	Darray<char> buf;

#ifndef CONSOLE
	DWORD time= getTickCount();
	SetDlgItemText(hWin, IDC_TIME, "");
	SetWindowText(hOut, "");
#endif
#ifndef NDEBUG
	if(*param==0){
		delete[] param;
		param= new char[1000];
		strcpy(param, "cos(pi/2");
	}
#endif
	output=0;
	cleanup();
	initLabels(param);

	//set the precision
	prec2= int(digits/dwordDigits[base])+2;
	amin(prec2, 8*32/TintBits);
	Nseed=prec2;
	baseOld=base;
	isGrey=false;

	for(precision= (prec2>60) ? (8*32/TintBits+1) : prec2; ; ){
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
					if(error) errPos=e;
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
						ClearError(1102);
						goto lout;
					}
					ClearError(1101);
				}
				else{
					if(error){
#ifndef CONSOLE
						PostMessage(hWin, WM_INPUTCUR, 0, int(errPos-param));
#endif
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
					int digits2= (precision>=prec2) ? digits : int((precision-2)*dwordDigits[base]+1);
					int len = LENM(y, digits2);
					a= (buf+=len)-1;
					if((unsigned)buf.len>MAX_OUTPUT_SIZE || !len){
						buf.len=n;
						cerror(1062, "Result is too long");
					}
					else{
						WRITEM(a, y, digits2, matrixFormat);
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
					e=input++;
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
		if(error == 1030 && precision < prec2) { //divisor of a big number
			ClearError(1030);
			cleanup();
#ifndef CONSOLE
			SetWindowText(hOut, "");
#endif
		}
		else {
			if(error) break;
			//compare with the previous result
			b= output && !strcmp(output, buf);
			delete[] output;
			output=buf.array;
			buf.array= new char[buf.capacity];
			if(b) break;
#ifndef CONSOLE
			if(precision < prec2) {
				isGrey=true;
				outputColor(0x707070);
			}
			else if(isGrey) {
				isGrey=false;
				outputColor(0);
			}
			writeOutput(output);
#endif
		}
		if(precision>=prec2){
			precision++;
		}
		else{
			precision=prec2;
		}
	}
#ifndef CONSOLE
	if(isGrey) {
		isGrey=false;
		outputColor(0);
	}
#endif

	if(!error || error==1100){
#ifdef CONSOLE
		puts(output);
#else
		//display calculation time
		char t[16];
		sprintf(t, "%u ms", getTickCount()-time);
		SetDlgItemText(hWin, IDC_TIME, t);
		//save to log file
		if(logging && *fnLog && output){
			HANDLE f= createFile(fnLog, OPEN_ALWAYS);
			SetFilePointer(f, 0, 0, FILE_END);
			saveResult(f, "%1\r\n =\r\n%2\r\n--------------------\r\n", /* format string is in saveResult, too */
				param, output, true);
		}
#endif
	}
	else{
		for(i=vars.len-1; i>=0; i--){
			vars[i].modif=false;
		}
	}
	delete[] output;
#ifndef CONSOLE
	delete[] param;
#endif
	return 0;
}
//---------------------------------------------------------------
#ifndef CONSOLE

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
			while(PeekMessageW(&mesg, NULL, 0, 0, PM_REMOVE)){
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
	digits= GetDlgItemInt(hWin, IDC_PRECISION, 0, FALSE);
	fixDigits= GetDlgItemInt(hWin, IDC_FIXDIGITS, 0, FALSE);
	error=0;
	//start a working thread
	DWORD threadId;
	thread= CreateThread(0, 0, (LPTHREAD_START_ROUTINE)calcThread, input, 0, &threadId);
}

void calc()
{
	//copy content of the input edit box
	char *input= getInput();
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

void wrAns()
{
	if(error) return;
	char *buf= AWRITEM(ans, digits, matrixFormat);
	writeOutput(buf);
	delete[] buf;
}

#endif

static int __cdecl cmpOp(const void *a, const void *b)
{
	return _stricmp((*(Top**)a)->name, (*(Top**)b)->name);
}

void initFuncTab()
{
	WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, L"\xb0\0", 1, degSymbol, 2, "°", 0);

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
