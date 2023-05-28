/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#ifndef ARITH
#define ARITH

#ifdef _WIN64
#define ARIT64
#define TintBits 64
#define ExpMax   0x7ffffffffffffff0
#define TuintMax 0xffffffffffffffff
#define TintMin  0x8000000000000000
typedef INT_PTR Tint;
typedef UINT_PTR Tuint;
#else
#define TintBits 32
#define ExpMax   0x7ffffff0
#define TuintMax 0xffffffff
#define TintMin  0x80000000
typedef int Tint;
typedef unsigned Tuint;
#endif

#define Int64Min  0x8000000000000000
typedef Tint *Pint;

struct Numx {
	Tint
	 alen, //allocated data length (in Tint units)
	 len,  //used data length (in Tint units), 0=zero, -1=variable, -2=fraction, -5=range, -12=matrix
	 sgn,  //sign 0=plus,1=minus
	 exp,  //exponent (how many Tints has integer part)
	 m,	  //mantissa, (most significant digits are at the beginning)
	 d,
	 reserved[12];
};

//fractions: alen,-2,sgn,0 or 1,numerator,denominator
//zero can have any sgn, any exp, alen>0

struct Complex {
	Pint r; //real part
	Pint i; //imaginary part
};

/*
 result must be at different memory locations than input parameters (except functions COPYX, MULTI, DIVI, LSHI)
 */
extern "C"{
	Pint _fastcall ALLOCX(Tint len); //allocate number (len is in Tint units), initialize to zero, return pointer to mantissa
	void _fastcall FREEX(Pint x);    //release number, x is pointer to mantissa, x can be NULL
	Pint _cdecl ALLOCN(int n, Tint len, ...); //allocate n numbers, vararg are variables for pointers
	Pint _fastcall NEWCOPYX(const Pint a); //duplicate a
	void _stdcall COPYX(Pint dest, const Pint src); //assign

	char*_stdcall READX(Pint x, const char *buf);  //read real number, base is determined by global variable baseIn, return pointer to the first character after the number
	void _stdcall WRITEX(char *buf, const Pint x, int digits); //print real number, base is determined by global variable base, buffer must be long enough
	int _stdcall LENX(const Pint x, int digits); //guess buffer length for WRITEX
	char*_stdcall AWRITEX(const Pint x, int digits); //allocate buffer and call WRITEX

	void _fastcall SETXN(Pint x, Tint n); //assign signed int
	void _fastcall SETX(Pint x, Tuint n); //assign unsigned int
	void _fastcall ZEROX(Pint x);  //assign 0
	void _fastcall ONEX(Pint x);   //assign 1
	void _fastcall NORMX(Pint x);  //normalize mantissa

	void _fastcall NEGX(Pint x);   //change sign
	void _fastcall ABSX(Pint x);   //positive sign
	void _fastcall SIGNX(Pint x);  //convert sign to -1,0,+1
	void _fastcall ROUNDX(Pint x); //round to nearest integer
	void _fastcall TRUNCX(Pint x); //round towards zero
	void _fastcall INTX(Pint x);   //round to lesser integer
	void _fastcall CEILX(Pint x);  //round to upper integer
	void _fastcall FRACX(Pint x);  //part after decimal point
	void _fastcall SCALEX(Pint x, Tint n);  //add n to exponent
	void _fastcall FRACTOX(Pint x); //convert fraction to real number
	Tint _fastcall ADDII(Tint a, Tint b);

	int  _stdcall CMPX(const Pint a, const Pint b); //compare, return -1,0,+1
	int  _stdcall CMPU(const Pint a, const Pint b); //ignore sign and compare
	void _stdcall PLUSX(Pint y, const Pint a, const Pint b); //add
	void _stdcall MINUSX(Pint y, const Pint a, const Pint b);//subtract
	void _stdcall MULTX(Pint y, const Pint a, const Pint b); //multiply
	void _stdcall MULTI(Pint y, const Pint a, Tuint i);
	void _stdcall MULTIN(Pint y, const Pint a, Tint i);
	void _stdcall MULTI1(Pint y, Tuint i);
	void _stdcall DIVX(Pint y, const Pint a, const Pint b);  //divide
	void _stdcall DIVX2(Pint y, const Pint a, const Pint b); //slow divide
	void _stdcall DIVI(Pint y, const Pint a, Tuint i);
	void _stdcall DIVI1(Pint y, Tuint n);
	void _stdcall MODX(Pint y, const Pint a, const Pint b);  //modulus
	void _stdcall IDIVX(Pint y, const Pint a, const Pint b);
	void _stdcall RSHX(Pint y, const Pint a, const Pint b);
	void _stdcall RSHIX(Pint y, const Pint a, const Pint b);
	void _stdcall RSHI(Pint y, const Pint a, Tint n);  //shift n bits right or -n bits left
	void _stdcall LSHX(Pint y, const Pint a, const Pint b);
	void _stdcall LSHI(Pint y, const Pint a, Tint n);  //shift n bits left or -n bits right

	void _stdcall EXPX(Pint y, const Pint x);          //e^x
	void _stdcall LNX(Pint y, const Pint x);     //natural logarithm
	void _stdcall INVERSEROOTI(Pint y, Pint x, Tuint n);
	void _stdcall AGMX(Pint z, const Pint x0, const Pint y0);
	void _stdcall POWX(Pint y, const Pint a, const Pint b); //a^b
	void _stdcall POWI(Pint y, const Pint x, __int64 n);    //x^n
	void _stdcall ROOTX(Pint y, const Pint b, const Pint a); //a^(1/b)
	void _stdcall SQRTX(Pint y, const Pint x);  //square root - iteration
	void _stdcall SQRTX2(Pint y, const Pint x); //slow square root
	unsigned _stdcall SQRTI(unsigned __int64 x);//square root
	void _stdcall SQRX(Pint y, const Pint x);

	void _stdcall RADTODEGX(Pint y, const Pint x);  //convert radian to degree
	void _stdcall DEGTORADX(Pint y, const Pint x);  //convert degree to radian
	void _stdcall RADTOGRADX(Pint y, const Pint x); //convert radian to grad
	void _stdcall GRADTORADX(Pint y, const Pint x); //convert grad to radian
	void _stdcall DEGTOGRADX(Pint y, const Pint x);
	void _stdcall GRADTODEGX(Pint y, const Pint x);
	void _stdcall DEGX(Pint y, const Pint x);
	void _stdcall RADX(Pint y, const Pint x);
	void _stdcall GRADX(Pint y, const Pint x);
	void _stdcall TODEGX(Pint y, const Pint x);
	void _stdcall TORADX(Pint y, const Pint x);
	void _stdcall TOGRADX(Pint y, const Pint x);
	void _stdcall DMSTODECX(Pint y, const Pint x);
	void _stdcall DECTODMSX(Pint y, const Pint x);
	void _stdcall CTOFX(Pint y, const Pint x);
	void _stdcall FTOCX(Pint y, const Pint x);

	void _stdcall SINX(Pint y, const Pint x);  //trigonometric functions
	void _stdcall COSX(Pint y, const Pint x);
	void _stdcall SECX(Pint y, const Pint x);
	void _stdcall CSCX(Pint y, const Pint x);
	void _stdcall TANX(Pint y, const Pint x);
	void _stdcall COTGX(Pint y, const Pint x);
	void _stdcall ASINX(Pint y, const Pint x); //inverse trigonometric functions
	void _stdcall ACOSX(Pint y, const Pint x);
	void _stdcall ATANX(Pint y, const Pint x);
	void _stdcall ATAN2X(Pint y, const Pint a, const Pint b);
	void _stdcall ACOTGX(Pint y, const Pint x);

	void _stdcall SINHX(Pint y, const Pint x); //hyperbolic functions
	void _stdcall COSHX(Pint y, const Pint x);
	void _stdcall TANHX(Pint y, const Pint x);
	void _stdcall COTGHX(Pint y, const Pint x);

	void _stdcall FACTORIALX(Pint y, Pint x);
	void _stdcall FACTORIALI(Pint y, Tuint n);//n!
	void _stdcall FFACTX(Pint y, Pint x);
	void _stdcall FFACTI(Pint y, Tuint n);//n!!
	void _stdcall COMBINX(Pint y, const Pint a, const Pint b);
	void _stdcall COMBINI(Pint y, Tuint n, Tuint m);     //combination
	void _stdcall PERMUTX(Pint y, const Pint a, const Pint b);
	void _stdcall PERMUTI(Pint y, Tuint n, Tuint m);     //variation
	void _stdcall GCDX(Pint y, const Pint a, const Pint b);
	void _stdcall LCMX(Pint y, const Pint a, const Pint b);
	void _stdcall DIVISORX(Pint y, const Pint x);
	void _stdcall PRIMEX(Pint y, const Pint x);
	void _stdcall ISPRIMEX(Pint y, const Pint x);
	void _stdcall FIBONACCIX(Pint y, const Pint x);

	void _stdcall NOTX(Pint y, const Pint x);
	void _stdcall ANDX(Pint y, const Pint a, const Pint b);
	void _stdcall ORX(Pint y, const Pint a, const Pint b);
	void _stdcall XORX(Pint y, const Pint a, const Pint b);
	void _stdcall NANDX(Pint y, const Pint a, const Pint b);
	void _stdcall NANDBX(Pint y, const Pint a, const Pint b);
	void _stdcall NORX(Pint y, const Pint a, const Pint b);
	void _stdcall NORBX(Pint y, const Pint a, const Pint b);
	void _stdcall IMPX(Pint y, const Pint a, const Pint b);
	void _stdcall IMPBX(Pint y, const Pint a, const Pint b);
	void _stdcall EQVX(Pint y, const Pint a, const Pint b);
	void _stdcall EQVBX(Pint y, const Pint a, const Pint b);

	void _stdcall GCDNX(Pint y0, unsigned num, const Complex *A);
	void _stdcall LCMNX(Pint y0, unsigned num, const Complex *A);

	void _stdcall IFX(Pint y, const Pint *A);
	void _stdcall EQUALX(Pint y, const Pint a, const Pint b);
	void _stdcall NOTEQUALX(Pint y, const Pint a, const Pint b);
	void _stdcall GREATERX(Pint y, const Pint a, const Pint b);
	void _stdcall LESSX(Pint y, const Pint a, const Pint b);
	void _stdcall GREATEREQX(Pint y, const Pint a, const Pint b);
	void _stdcall LESSEQX(Pint y, const Pint a, const Pint b);

	void _stdcall PIX(Pint y);
	void _stdcall RANDX(Pint y);
	void _stdcall RANDOMX(Pint y, const Pint x);
	void _stdcall HYPOTX(Pint y, const Pint a, const Pint b);

	void _stdcall WRITEX1(char *buf, const Pint x); //decimal point must be inside mantissa, does not round last digit
	char*_stdcall READX1(Pint x, const char *buf);  //read number without exponent
	void _stdcall MULTX1(Pint y, const Pint a, const Pint b); //multiply, does not recurse
	void _stdcall PLUSU(Pint y, const Pint a, const Pint b); //addition, sign is ignored
	void _stdcall MINUSU(Pint y, const Pint a, const Pint b);//subtraction, sign is ignored
	void _stdcall ANDU(Pint y, const Pint a, const Pint b);
	void _stdcall ORU(Pint y, const Pint a, const Pint b);
	void _stdcall XORU(Pint y, const Pint a, const Pint b);
	unsigned _stdcall MODI(const Pint a, Tuint b);  //modulus
	void _stdcall VARX(Pint y, Tuint num, const Pint *A, unsigned sample);
	int  _stdcall PI(Pint x); //compute PI constant, return iterations count

	void cerror(int id, char *txt);
	void errImag();
	void errMatrix();
	void overflow();
	void getln2(Tint len);
	void getln10(Tint len);
	void getpi(Tint len);
	void angleResult(Pint x);
	void assign(Pint &dest, const Pint src);

	extern int error, base, baseIn, angleMode, numFormat, fixDigits;
	extern int enableFractions, separator1, separator2, sepFreq1, sepFreq2, useSeparator1, useSeparator2, disableRounding;
	extern double dwordDigits[];
	extern char digitTab[];
	extern Pint lnBase, ln2, ln10, pi, pi2, pi4, one, minusone, half, two, ten, seedx;
	extern Tint precision, Nseed;
}

inline bool isZero(const Pint x){
	return x[-3]==0;
}

inline bool isOneOrMinusOne(const Pint x){
	return x[0]==1 && (x[-3]==1 && x[-1]==1 || x[-3]==-2 && x[1]==1);
}

inline bool equInt(const Pint x, Tuint n)
{
	return Tuint(x[0])==n && (x[-3]==1 && x[-1]==1 || x[-3]==-2 && x[1]==1)
		 && x[-2]==0;
}

inline bool isFraction(const Pint x){
	return x[-3]==-2;
}

//is signed integer which can be converted to Tint
inline bool isInt(const Pint x){
	return (x[-3]==1 && x[-1]==1 ||
		x[-3]==-2 && x[1]==1) && x[0]>=0 || x[-3]==0;
}

inline Tint toInt(const Pint x){
	return x[-3] ? (x[-2] ? -x[0] : x[0]) : 0;
}

inline bool isInt4(const Pint x){
	return (x[-3]==1 && x[-1]==1 ||
		x[-3]==-2 && x[1]==1) && Tuint(x[0])<=0x7fffffff || x[-3]==0;
}

inline int toInt4(const Pint x){
	return x[-3] ? (x[-2] ? -(int)x[0] : (int)x[0]) : 0;
}


//is positive integer which can be converted to Tuint
inline bool isDword(const Pint x){
	return (x[-3]==1 && x[-1]==1 || x[-3]==-2 && x[1]==1)
		&& x[-2]==0 || x[-3]==0;
}

inline Tuint toDword(const Pint x){
	return x[-3] ? x[0] : 0;
}

inline unsigned toDword4(const Pint x){
	return x[-3] ? (unsigned)x[0] : 0;
}


#ifdef ARIT64
inline bool isDword4(const Pint x){
	return isDword(x) && Tuint(x[0])<=0xffffffff;
}

inline bool is32bit(const Pint x){
	return (x[-3]==1 && x[-1]==1 ||
		x[-3]==-2 && x[1]==1) && x[0]>=0 || x[-3]==0;
}

inline __int64 to32bit(const Pint x){
	return x[-3] ? (x[-2] ? -x[0] : x[0]) : 0;
}
#else
inline bool isDword4(const Pint x){ 
	return (x[-3]==1 && x[-1]==1 || x[-3]==-2 && x[1]==1) 
		&& x[-2]==0 || x[-3]==0; 
}

inline bool is32bit(const Pint x)
{
	return x[-3]==1 && x[-1]==1 || x[-3]==-2 && x[1]==1 
		|| x[-3]==0; 
}

inline __int64 to32bit(const Pint x)
{
	return (x[-3]==0) ? 0 : (x[-2] ? -(__int64)(Tuint)x[0] : (Tuint)x[0]);
}
#endif

inline bool isReal(const Pint x){
	return x[-1]<x[-3] && x[-3]!=0 || x[-3]==-2 && x[1]!=1;
}

inline bool isOdd(const Pint x){
	return x[-1]==x[-3] && x[-3]>0 && (x[x[-3]-1]&1)
		|| x[-3]==-2 && (x[0]&1);
}

void assignM(Complex &dest, const Complex src);
void assign(Complex &dest, const Complex src);

enum{ ANGLE_DEG, ANGLE_RAD, ANGLE_GRAD };
enum{ MODE_SCI, MODE_NORM, MODE_ENG, MODE_FIX };

typedef void(_fastcall *Tunary0)(Pint);
typedef void(_stdcall *Tunary2)(Pint, const Pint);
typedef void(_stdcall *Tbinary)(Pint, const Pint, const Pint);
typedef void(_stdcall *Tnulary)(Pint);
typedef void(_stdcall *Tvararg)(Pint, unsigned, const Complex*);
typedef void(_stdcall *Tarrayarg)(Pint, const Complex*);

#ifndef NDEBUG
extern void showx(Pint x);
extern void logx(char *msg, Pint x);
void logs(char *fmt, ...);
#else 
inline void showx(Pint){};
#endif

template <class T> inline void aminmax(T &x, int l, int h){
	if(int(x)<l) x=l;
	if(int(x)>h) x=h;
}

#endif
