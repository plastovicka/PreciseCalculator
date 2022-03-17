/*
	(C) 2004-2022  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "arit.h"
#include "preccalc.h"

int
angleMode=ANGLE_DEG,
 numFormat=MODE_NORM,
 fixDigits=2,
 enableFractions=1,
 useSeparator1=1,
 useSeparator2=1,
 sepFreq1=3,
 sepFreq2=10,
 separator1=' ',
 separator2=' ',
 base=10,
 baseIn,
 disableRounding=0,
 error=0;
Tint Nln2, Nln10, Npi, Nseed;

struct Tpostfix { char c; int e; } postfixTab[] ={
	{'y', -8},//yocto
	{'z', -7},//zepto
	{'a', -6},//atto
	{'f', -5},//femto
	{'p', -4},//piko
	{'n', -3},//nano
	{'u', -2},//mikro
	{'m', -1},//mili
	{'k', +1},//kilo
	{'M', +2},//mega
	{'G', +3},//giga
	{'T', +4},//tera
	{'P', +5},//peta
	{'X', +6},//exa
	{'Z', +7},//zeta
	{'Y', +8},//yotta
};


//TODO: synchronization for multiple threads

Numx Kzero={1, 0, 0, 0, 0}, Kone={1, -2, 0, 1, 1, 1}, Kminusone={1, -2, 1, 1, 1, 1}, Khalf={1, -2, 0, 0, 1, 2},
Ktwo={1, -2, 0, 1, 2, 1}, Kthree={1, -2, 0, 1, 3, 1}, Kten={1, -2, 0, 1, 10, 1}, Kbase={1, 1, 0, 2, 1};
Pint one=&Kone.m, minusone=&Kminusone.m, half=&Khalf.m, two=&Ktwo.m, three=&Kthree.m, ten=&Kten.m;
Pint lnBase, ln2, ln10, pi, pi2, pi4, seedx;

double dwordDigits[37]={0, 0, // 32*ln(2)/ln(base)
#ifndef ARIT64
32.0000000000000000, 20.1897521142866370, 16.0000000000000000,
13.7816498583485780, 12.3792898315053300, 11.3986299874567100,
10.6666666666666660, 10.0948760571433190, 9.6329598612473983,
9.2500744421724104, 8.9261742608361541, 8.6476209416742318,
8.4047851211901925, 8.1906567939140960, 8.0000000000000000,
7.8288173477832323, 7.6739989301802058, 7.5330852277324230,
7.4041028211122937, 7.2854479583024956, 7.1758023749624140,
7.0740713426401198, 6.9793373435370096, 6.8908249291742889,
6.8078737137076208, 6.7299173714288791, 6.6564671256483026,
6.5870986387339023, 6.5214415068961982, 6.4591707706271952,
6.3999999999999995, 6.3436756214579368, 6.2899722314503235,
6.2386887006011618, 6.1896449157526652
#else
64, 40.379504228573276, 32,
27.5632997166971552, 24.7585796630106616, 22.7972599749134193,
21.3333333333333333, 20.189752114286638, 19.2659197224947965,
18.500148884344823, 17.85234852167231, 17.2952418833484634,
16.8095702423803871, 16.3813135878281913, 16,
15.6576346955664659, 15.3479978603604125, 15.0661704554648471,
14.8082056422245872, 14.5708959166049919, 14.3516047499248281,
14.1481426852802398, 13.9586746870740198, 13.7816498583485776,
13.6157474274152418, 13.4598347428577587, 13.3129342512966053,
13.1741972774678053, 13.0428830137923962, 12.9183415412543904,
12.8, 12.687351242915874, 12.5799444629006469,
12.4773774012023237, 12.3792898315053308
#endif
};

#ifdef DEBUG_CMP
bool CMPDBG(Pint y, Pint z)
{
	if(error) return true;
	Tint _precision = y[-4];
	if(z[-3]>_precision)
	{
		z[-3]=_precision;
		if(z[_precision]<0){ //most significant bit is 1
			Numx Ktmp;
			Pint t=&Ktmp.m;
			t[-4]=t[-3]=t[0]=1;
			t[-2]=z[-2];
			t[-1]=z[-1]-(_precision-1);
			Pint u=ALLOCX(z[-4]);
			PLUSX(u, z, t);
			//logx("u", u); logx("z", z); logx("t", t);
			COPYX(z, u);
			FREEX(u);
		}
	}
	return !CMPX(y, z);
}
#endif

void getln2(Tint len)
{
	if(len>Nln2){
		len++;
		FREEX(ln2);
		FREEX(lnBase);
		lnBase= ALLOCX(len);
		ln2= ALLOCX(len);
		// ln(2)=-ln(1/2)
		LNX(ln2, half);
		NEGX(ln2);
		// ln(2^32)=32*ln2
		MULTI(lnBase, ln2, TintBits);
		Nln2= error ? 0 : len;
	}
}

void getln10(Tint len)
{
	if(len>Nln10){
		len++;
		FREEX(ln10);
		ln10= ALLOCX(len);
		LNX(ln10, ten);
		Nln10= error ? 0 : len;
	}
}

void _stdcall RANDX(Pint y)
{
	Pint t;
	Tint len=min(y[-4], Nseed)+3;
	getpi(len);
	getln2(len);
	if(!seedx || seedx[-4]<len){
		t=ALLOCX(len);
		if(seedx){
			COPYX(t, seedx);
			FREEX(seedx);
		}
		else{
			SETX(t, GetTickCount());
			SCALEX(t, -1);
		}
		seedx=t;
	}
	t=ALLOCX(len);
	MULTX(t, seedx, pi);
	PLUSX(seedx, t, ln2);
	FREEX(t);
	SCALEX(seedx, 2);
	FRACX(seedx);
	COPYX(y, seedx);
}

void _stdcall RANDOMX(Pint y, const Pint x)
{
	RANDX(y);
	MULTI1(y, toDword(x));
	TRUNCX(y);
}

//-------------------------------------------------------------------
__int64 strtoi64(const char *str, char **end)
{
	int sign=0;
	__int64 n=0;

	while(*str==' ' || *str=='\t')  str++;
	if(*str=='-'){
		sign=1;
		str++;
	}
	else if(*str=='+'){
		str++;
	}
	while(*str>='0' && *str<='9'){
		if((unsigned __int64)n > 922337203685477580) overflow();
		n=10*n+(*str-'0');
		str++;
	}
	if(n<0) overflow();
	if(end) *end = (char *)str;
	return sign ? -n : n;
}

//-------------------------------------------------------------------
//x*=base^e
void _stdcall EEX(Pint x, __int64 e, int _base)
{
	Pint y, t;
	ALLOCN(2, x[-4], &y, &t);
	Numx Ktmp;
	Pint tmp=&Ktmp.m;
	tmp[-4]=1;
	SETX(tmp, _base);
	if(e<0){
		POWI(t, tmp, -e);
		DIVX(y, x, t);
	}
	else{
		POWI(t, tmp, e);
		MULTX(y, x, t);
	}
	COPYX(x, y);
	FREEX(y);
}

char* _stdcall READX(Pint x, const char *buf)
{
	char *s= READX1(x, buf);
	while(*s==' ') s++;
	if(isLetter(*s) && !isLetter(s[1]))
	{
		if(*s=='E' || *s=='e'){
			//exponent
			s++;
			EEX(x, strtoi64(s, &s), baseIn);
		}
		else{
			//engineer symbols
			for(Tpostfix *p= postfixTab; p<endA(postfixTab); p++){
				if(p->c == *s){
					s++;
					EEX(x, p->e, 1000);
					break;
				}
			}
		}
	}
	//convert integer to fraction
	if(is32bit(x) && !isZero(x)){
		x[-3]=-2;
		x[1]=1;
	}
	return s;
}

static void roundResult(char *buf, int _digits, bool fixed)
{
	char *s, *k, *dot;
	int i, n;

	if(*buf=='-' || *buf=='+') buf++;
	s=buf;
	dot=0;
	//find the first not zero digit
	for(;; s++){
		if(*s=='0') continue;
		if(*s=='.'){
			dot=s;
			continue;
		}
		break;
	}
	if(dot && fixed){
		n= fixDigits+1-int(s-dot);
		if(n<=0) n=1;
		if(_digits>n) _digits=n;
	}
	//find the last digit
	for(;; s++){
		if(*s==0 || *s==' '){
			k=s;
			if(!dot) return;
			goto ltrunc;
		}
		if(*s=='.'){
			dot=s;
			if(fixed && _digits>fixDigits) _digits=fixDigits;
			continue;
		}
		if(_digits--<=0 && dot) break;
	}
	i=*s-'0';
	if(i>9) i=*s-'A'+10;
	if(i>35) i=*s-'a'+10;
	if(disableRounding) {
		//round only if there are all 9
		if (i == base - 1) {
			for (k = s + 1; *k == *s; k++);
			if (*k != '\0') i=0;
		}
		else i=0;
	}
	k=s;
	if(i>=((base+1)>>1)){
		//increment
		for(;;){
			if(s==buf){
				//insert 1 at the beginning
				i=strleni(buf);
				memmove(buf+1, buf, i+1);
				*s='1';
				k++;
				break;
			}
			s--;
			if(*s=='.') s--;
			if(base>10 ? (*s!='A'+base-11 && *s!='a'+base-11) : (*s!='0'+base-1)){
				if(*s=='9') *s=digitTab[10];
				else (*s)++;
				break;
			}
			*s='0';
		}
	}
ltrunc:
	//truncate trailing zeros
	k--;
	while(k>buf && *k=='0') k--;
	//delete trailing dot
	if(*k!='.') k++;
	//move exponent
	for(s=k; *s && *s!=' '; s++);
	strcpy(k, s);
}

static void prettyResult(char *buf)
{
	char *s, *d, *b1, *b2, *e1, *e2;
	int i, n1, n2, a, f1, f2;

	if(!useSeparator1 && !useSeparator2) return;
	aminmax(sepFreq1, 1, 10000);
	aminmax(sepFreq2, 1, 10000);
	f1=sepFreq1;
	f2=sepFreq2;
	if(base==2 || base==16){
		for(i=1; i<f1; i<<=1);
		f1=i;
		for(i=1; i<f2; i<<=1);
		f2=i;
	}
	b1=buf;
	if(*b1=='-' || *b1=='+') b1++;
	for(s=b1; *s && *s!=' ' && *s!='.'; s++);
	e1=s;
	if(*s=='.') s++;
	b2=s;
	for(; *s && *s!=' '; s++);
	e2=s;
	n1= int(e1-b1);
	n2= int(e2-b2);
	a=0;
	if(useSeparator1) a+=(n1-1)/f1;
	if(useSeparator2) a+=(n2-1)/f2;
	i=strleni(s)+1;
	d=s+a;
	memmove(d, s, i);
	i=0;
	if(useSeparator2){
		i= n2%f2;
		if(!i) i=f2;
		i++;
	}
	for(; s>b2;){
		if(!--i){ *--d=char(separator2); i=f2; }
		*--d = *--s;
	}
	if(b2!=e1) *--d = *--s;
	i= useSeparator1 ? f1+1 : 0;
	for(; s>b1;){
		if(!--i){ *--d=char(separator1); i=f1; }
		*--d = *--s;
	}
}

void TuintToStr(Tuint x, char *buf)
{
#ifdef ARIT64
	_ui64toa(x, buf, base);
#else
	_ultoa(x, buf, base);
#endif
}

void _stdcall WRITEX(char *buf, const Pint x0, int _digits)
{
	__int64 e=0;
	Pint y, t, w, x, a;
	Tint prec;
	char *s;

	if(!buf) return;
	x=x0;
	if(isZero(x)){
		*buf++='0';
		*buf=0;
		if(numFormat==MODE_SCI) strcpy(buf, " E+0");
		return;
	}
	prec = int(_digits/dwordDigits[base])+2;

	if(isFraction(x)){
		if(enableFractions && numFormat!=MODE_FIX){
			bool isInt = x[1]==1;
			if(numFormat==MODE_NORM || !isInt) {
				if(x[-2]) *buf++='-';
				//numerator
				TuintToStr(x[0], buf);
				if(isInt) {
					prettyResult(buf);
				}
				else {
					//denominator
					s=strchr(buf, 0);
					*s++=' ';
					*s++='/';
					*s++=' ';
					TuintToStr(x[1], s);
				}
				if(base>10 && digitTab[10]=='A') {
					//to upper case
					for(s=buf; *s; s++) if(*s>='a' && *s<='z') *s-=('a'-'A');
				}
				return;
			}
		}
		x=ALLOCX(prec+1);
		COPYX(x, x0);
		FRACTOX(x);
	}
	if((numFormat==MODE_NORM || numFormat==MODE_FIX) &&
			x[-1]>=0 && x[-1]<=x[-4] && x[-1]<=prec){
		WRITEX1(buf, x);
	}
	else{
		//guess exponent
		double er = x[-1]*dwordDigits[base] - 5;
		if(er>=Int64Min){
			overflow();
			*buf++='0';
			*buf=0;
		}
		else if(-er>=Int64Min){
			*buf++='0';
			*buf=0;
		}
		else{
			e= (__int64)er;
			a=ALLOCN(2, x[-4], &y, &t);
			Numx Ktmp;
			Pint tmp=&Ktmp.m;
			tmp[-4]=1;
			SETX(tmp, base);
			if(e<=0){
				POWI(t, tmp, -e);
				MULTX(y, x, t);
			}
			else{
				POWI(t, tmp, e);
				DIVX(y, x, t);
			}
			if(e){
				//correct exponent
				while(y[-1]>0 && (y[-1]>1 || Tuint(y[0])>=Tuint(base)) && !error){
					DIVI(t, y, base);
					w=t; t=y; y=w;
					e++;
					if(e==TintMin) overflow();
				}
				while((y[-1]<=0 || numFormat==MODE_ENG && (e%3)) && !error){
					MULTI1(y, base);
					e--;
					if(e==TintMin) overflow();
				}
			}
			WRITEX1(buf, y);
			FREEX(a);
		}
	}
	if(x!=x0) FREEX(x);
	roundResult(buf, _digits, numFormat==MODE_FIX && !e);

	//write exponent
	if(e || numFormat==MODE_SCI){
		s= strchr(buf, 0);
		if(s[-1]=='0' && !strchr(buf, '.')){
			//trim trailing zeros
			if(numFormat==MODE_ENG){
				while(s>buf+3 && s[-1]=='0' && s[-2]=='0' && s[-3]=='0'){
					s-=3;
					*s=0;
					e+=3;
				}
			}
			else{
				while(s>buf+1 && s[-1]=='0'){
					s--;
					*s=0;
					e++;
				}
			}
		}
		if(e || numFormat==MODE_SCI){
			*s++=' ';
			*s++='E';
			if(e>=0) *s++='+';
			_i64toa(e, s, 10);
		}
	}

	prettyResult(buf);
}

int _stdcall LENX(const Pint x, int _digits)
{
	if(isFraction(x)) return 2*_digits+90;
	return 24+2*int(dwordDigits[base]*(x[-4]+1));
}

char * _stdcall AWRITEX(const Pint x, int _digits)
{
	char *buf= new char[LENX(x, _digits)];
	WRITEX(buf, x, _digits);
	return buf;
}
//-------------------------------------------------------------------
//Karatsuba algorithm
// x = x0 + x1*B
// y = y0 + y1*B
// x*y = (B^2+B)*x1*y1 + B*(x1-x0)*(y0-y1) + (B+1)*x0*y0
// complexity O(n^(ln3/ln2)) = O(n^1.585)
void _stdcall MULTX2(Pint z, const Pint x, const Pint y)
{
	Tint n, n1, n2, n01, n02, B, xsgn, ysgn, xexp, yexp;
	Pint x0, y0, t1, t2, t3;

	n01=x[-3]; n02=y[-3];
	const int MULT2LIM = 50;
	if(n01<MULT2LIM || n02<MULT2LIM || 2*n02>3*n01 || 2*n01>3*n02){
		MULTX1(z, x, y);
		return;
	}

	n=z[-4]+1;
	n1=min(n, n01);
	n2=min(n, n02);
	B=n1+n2;
	if(n>B) n=B;
	ALLOCN(5, n, &x0, &y0, &t1, &t2, &t3);

	B=(B+1)/4;

	//sign
	xsgn=x[-2]; ysgn=y[-2];
	x[-2]=0; y[-2]=0;

	//precision
	x0[-3]=B; y0[-3]=B;
	x[-3]=n1-B; y[-3]=n2-B;

	//exponent
	xexp=x[-1]; yexp=y[-1];
	x0[-1]= B-n1;
	y0[-1]= B-n2;
	x[-1]=y[-1]=-B;

	memcpy(x0, x + (n1-B), B*(TintBits/8));
	NORMX(x0);
	memcpy(y0, y + (n2-B), B*(TintBits/8));
	NORMX(y0);

	MINUSX(t3, x, x0);  //t3= x1-x0
	MINUSX(t1, y0, y);  //t1= y0-y1
	MULTX2(t2, t3, t1); //t2= (x1-x0)*(y0-y1)
	MULTX2(t3, x0, y0); //t3= x0*y0
	SCALEX(t2, B);
	PLUSX(t1, t3, t2);  //t1= B*(x1-x0)*(y0-y1) + x0*y0
	SCALEX(t3, B);
	PLUSX(t2, t1, t3);  //t2= B*(x1-x0)*(y0-y1) + (B+1)*x0*y0
	MULTX2(t3, x, y);   //t3= x1*y1
	SCALEX(t3, B);
	PLUSX(t1, t2, t3);  //t1= B*x1*y1 + B*(x1-x0)*(y0-y1) + (B+1)*x0*y0
	SCALEX(t3, B);
	PLUSX(z, t1, t3);   //z= (B^2+B)*x1*y1 + B*(x1-x0)*(y0-y1) + (B+1)*x0*y0

	x[-2]=xsgn; y[-2]=ysgn;
	z[-2]=xsgn^ysgn;
	x[-3]=n01; y[-3]=n02;
	x[-1]=xexp; y[-1]=yexp;
	z[-1]=ADDII(z[-1], ADDII(xexp, yexp));

	FREEX(x0);
}
//-------------------------------------------------------------------
#ifndef NDEBUG
const int MULT3LIM = 30;
#elif defined(ARIT64)
const int MULT3LIM = 730;
#else
const int MULT3LIM = 450;
#endif

//Toom–Cook (Toom-3) algorithm
// x = x0 + x1*B + x2*B^2
// y = y0 + y1*B + y2*B^2
// complexity O(n^(ln5/ln3)) = O(N^1.465)
void _stdcall MULTX(Pint z, const Pint x, const Pint y)
{
	Tint n, n1, n2, n01, n02, B, xsgn, ysgn, xexp, yexp;
	Pint rr[5], pp[5], qq[5], x0, y0, t1, t2, t3, t4;

	n01=x[-3]; n02=y[-3];
	if(n01<MULT3LIM || n02<MULT3LIM || 2*n02>3*n01 || 2*n01>3*n02){
		MULTX2(z, x, y);
		return;
	}

	n=z[-4]+1;
	n1=min(n, n01); n2=min(n, n02);
	B=n1+n2;
	if(n>B) n=B;
	B=(B+2)/6;
	ALLOCN(9, n, &x0, &y0, &t1, &pp[1], &pp[2], &pp[3], &qq[1], &qq[2], &qq[3]);

	//sign
	xsgn=x[-2]; ysgn=y[-2];
	x[-2]= y[-2]=0;

	//precision
	x0[-3]= y0[-3]=B;
	x[-3]=n1-B*2;
	y[-3]=n2-B*2;

	//exponent
	xexp=x[-1]; yexp=y[-1];
	t2=qq[3];
	t2[-1]= x0[-1]= B-n1;
	y0[-1]= B-n2;
	x[-1]= y[-1]= -B*2;

	//splitting and evaluation of the first polynomial
	memcpy(x0, x + (n1-B), B*(TintBits/8));
	NORMX(x0);
	PLUSX(t1, x, x0);
	t2[-3]=B;
	memcpy(t2, x + (n1-B*2), B*(TintBits/8));
	NORMX(t2);
	PLUSX(pp[1], t1, t2);
	MINUSX(pp[2], t1, t2);
	PLUSX(t1, pp[2], x);
	MULTI1(t1, 2);
	MINUSX(pp[3], t1, x0);

	//splitting and evaluation of the second polynomial
	t2[-1]=y0[-1];
	memcpy(y0, y + (n2-B), B*(TintBits/8));
	NORMX(y0);
	PLUSX(t1, y, y0);
	t2[-3]=B;
	memcpy(t2, y + (n2-B*2), B*(TintBits/8));
	NORMX(t2);
	PLUSX(qq[1], t1, t2);
	MINUSX(qq[2], t1, t2);
	PLUSX(t1, qq[2], y);
	MULTI1(t1, 2);
	MINUSX(qq[3], t1, y0);

	//pointwise multiplication and interpolation
	MULTX(t1, pp[3], qq[3]);
	rr[0]=pp[3];
	rr[1]=qq[3];
	MULTX(rr[1], pp[1], qq[1]);
	rr[2]=pp[1];
	rr[4]=qq[1];
	MULTX(rr[0], x0, y0);
	t3=x0;
	MINUSX(t3, t1, rr[1]);
	DIVI1(t3, 3);
	MULTX(rr[2], pp[2], qq[2]);
	t2=pp[2];
	MINUSX(t2, rr[2], rr[0]);
	MINUSX(t1, t2, t3);
	DIVI1(t1, 2);
	MULTX(rr[4], x, y);
	t4=y0;
	MULTI(t4, rr[4], 2);
	PLUSX(t3, t1, t4);
	MINUSX(t4, rr[1], rr[2]);
	DIVI1(t4, 2);
	PLUSX(t1, t2, t4);
	MINUSX(t2, t1, rr[4]);
	MINUSX(t1, t4, t3);

	//recomposition
	SCALEX(t1, B);
	PLUSX(t4, rr[0], t1);
	SCALEX(t2, B*2);
	PLUSX(t1, t4, t2);
	SCALEX(t3, B*3);
	PLUSX(t4, t1, t3);
	SCALEX(rr[4], B*4);
	PLUSX(z, t4, rr[4]);

	x[-2]=xsgn; y[-2]=ysgn;
	z[-2]=xsgn^ysgn;
	x[-3]=n01; y[-3]=n02;
	x[-1]=xexp; y[-1]=yexp;
	z[-1]=ADDII(z[-1], ADDII(xexp, yexp));

#ifdef DEBUG_CMP
	Pint t=t1;
	MULTX2(t, x, y);
	assert(&MULTX && CMPDBG(z, t));
#endif

	FREEX(x0);
}
//-------------------------------------------------------------------
#if 0
void _stdcall SQRX(Pint z, const Pint x)
{
	Tint n, n1, n01, B, xsgn, xexp;
	Pint rr[5], pp[5], x0, t1, t2, t3, t4;

	n01=x[-3];
	if(n01<MULT3LIM){
		MULTX2(z, x, x);
		return;
	}
	n=z[-4]+1;
	n1=min(n, n01);
	B=n1*2;
	if(n>B) n=B;
	B=(n1+1)/3;
	ALLOCN(8, n, &x0, &t1, &t4, &pp[1], &pp[2], &pp[3], &rr[1], &rr[4]);

	xsgn=x[-2];
	x[-2]=0;

	x0[-3]=B;
	x[-3]=n1-B*2;

	xexp=x[-1];
	t4[-1]=x0[-1]= B-n1;
	x[-1]= -B*2;

	memcpy(x0, x + (n1-B), B*(TintBits/8));
	NORMX(x0);
	PLUSX(t1, x, x0);
	t4[-3]=B;
	memcpy(t4, x + (n1-B*2), B*(TintBits/8));
	NORMX(t4);
	PLUSX(pp[1], t1, t4);
	MINUSX(pp[2], t1, t4);
	PLUSX(t1, pp[2], x);
	MULTI1(t1, 2);
	MINUSX(pp[3], t1, x0);

	SQRX(t1, pp[3]);
	rr[0]=pp[3];
	SQRX(rr[1], pp[1]);
	rr[2]=pp[1];
	SQRX(rr[0], x0);
	t3=x0;
	MINUSX(t3, t1, rr[1]);
	DIVI1(t3, 3);
	SQRX(rr[2], pp[2]);
	t2=pp[2];
	MINUSX(t2, rr[2], rr[0]);
	MINUSX(t1, t2, t3);
	DIVI1(t1, 2);
	SQRX(rr[4], x);
	MULTI(t4, rr[4], 2);
	PLUSX(t3, t1, t4);
	MINUSX(t4, rr[1], rr[2]);
	DIVI1(t4, 2);
	PLUSX(t1, t2, t4);
	MINUSX(t2, t1, rr[4]);
	MINUSX(t1, t4, t3);

	SCALEX(t1, B);
	PLUSX(t4, rr[0], t1);
	SCALEX(t2, B*2);
	PLUSX(t1, t4, t2);
	SCALEX(t3, B*3);
	PLUSX(t4, t1, t3);
	SCALEX(rr[4], B*4);
	PLUSX(z, t4, rr[4]);

	x[-2]=xsgn;
	z[-2]=0;
	x[-3]=n01;
	x[-1]=xexp;
	z[-1]=ADDII(z[-1], ADDII(xexp, xexp));

	FREEX(x0);
}
#else
void _stdcall SQRX(Pint z, const Pint x)
{
	MULTX(z, x, x);
}
#endif
//-------------------------------------------------------------------
void _stdcall EXPX(Pint y, const Pint x0)
{
	Tint ex, n, sgn, k, p;
	Pint z, t, u, w, x, x1;

	x=x0;
	SETX(y, 1);
	if(isZero(x)) return; //x^0=1

	p = y[-4] + 2;
	if(p<40) k=30;
	else if(p<200 || isOneOrMinusOne(x)) k=80;
	else if(p<1000) k=150;
	else if(p<3000) k=250;
	else if(p<6000) k=400;
	else k=450;
	if(x[-1]<0){
		k += x[-1]*40;
		if(k<0) k=0;
	}

	ALLOCN(4, p + k/(TintBits/2), &u, &t, &z, &x1);
	ex=0;
	sgn=x[-2];
	if(x[-1]>0 && (!isFraction(x)
		|| Tuint(x[0])/20/y[-4] >= Tuint(x[1]))){
		getln2(y[-4]);
		x[-2]=0;
		if(x[-1]>1 || x[-1]==1 && CMPX(x, lnBase)>0){
			//calculate exponent
			DIVX(t, x, lnBase);
			ex=t[0];
			if(t[-1]>1 || ex<0){
				x[-2]=sgn;
				overflow();
				FREEX(u);
				y[-1]=ExpMax;
				return;
			}
			//x-=ex*ln(base)
			MULTI(t, lnBase, ex);
			MINUSX(x1, x, t);
			x1[-2]=sgn;
			x=x1;
		}
		x0[-2]=sgn;
	}
	//x<=lnBase

	//exp(x)= (exp(x/2))^2
	RSHI(x1, x, k);
	x=x1;

	COPYX(u, x);
	PLUSX(z, one, x);
	n=2;
	do{
		MULTX(t, x, u);
		DIVI(u, t, n);
		PLUSX(t, z, u); //z+=x^n/n!
		w=t; t=z; z=w;
		n++;
	} while((u[-1]>=z[-1]-z[-3] || u[-1]>=0) && !isZero(u) && !error);

	while(--k >= 0 && !error)
	{
		t[-4] = p + k/(TintBits/2);
		SQRX(t, z);
		w=t; t=z; z=w;
	}
	COPYX(y, z);
	//y<=Base
	SCALEX(y, (sgn) ? -ex : ex);
	FREEX(u);
}
//-------------------------------------------------------------------
void _stdcall AGMX(Pint z, const Pint x0, const Pint y0)
{
	Pint a, b, y, t, m, w;
	Tint p;

	p=z[-4]+3;
	m=ALLOCN(4, p, &a, &b, &t, &y);
	COPYX(a, x0);
	COPYX(b, y0);
	int n=0;
	while(!error)
	{
		MINUSX(y, a, b);
		if((y[-1]<a[-1]-z[-4] && y[-1]<0) || isZero(y))
			break; /**/

		// y=(a+b)/2
		PLUSX(t, a, b);
		DIVI(y, t, 2);
		// b=sqrt(a*b)
		MULTX(t, b, a);
		SQRTX(b, t);
		// a=y
		w=a; a=y; y=w;
		n++;
	}
	COPYX(z, a);
	FREEX(m);
}
//-------------------------------------------------------------------
static void _stdcall _LNX(Pint y, const Pint x0, bool useAGM)
{
	Tint ex, p;
	int num2;
	Pint z, t, v, w, x, u=0;

	if(x0[-2] || isZero(x0)){
		cerror(1009, "Logarithm of not positive number");
		return;
	}
#ifdef ARIT64
	const int LNLIM = 17;
#else
	const int LNLIM = 50;
#endif
	p=y[-4];
	if(p<LNLIM) useAGM = false;
	if(useAGM) {
		p+=3;
		ALLOCN(3, p, &x, &t, &v);
	}
	else {
		ALLOCN(4, p, &x, &t, &v, &u);
	}
	COPYX(x, x0);
	ex=num2=0;
	if(isFraction(x)){
		if(Tuint(x[0]) >= Tuint(x[1])){
			do{
				if(Tint(x[1])<0){
					FRACTOX(x);
					break;
				}
				x[1]<<=1;
				num2--;
			} while(Tuint(x[0]) >= Tuint(x[1]));
		}
		else{
			while(Tuint(x[0]<<1) < Tuint(x[1]) && Tint(x[0])>0){
				x[0]<<=1;
				num2++;
			}
		}
	}
	if(!isFraction(x)){
		//separate exponent
		ex=x[-1];
		x[-1]=0;
		//multiply by 2
		for(; Tuint(x[0]) <
#ifdef ARIT64
			Tuint(13043817825332782212) /* 2^63*sqrt2 */
#else
			3037000499u /* 2^31*sqrt2 */
#endif
			&& !x[-1]; num2++){
			MULTI1(x, 2);
		}
	}
	//now is 0.5 <= x < 1.42
	z=y;

	if(useAGM)
	{
		getpi(p);

		//m=precision/2
		Tint m;
		if(CMPX(x, half)==0)
		{
			//ln(1/2)=-ln(2^m)/m
			m=(p*TintBits)/2+6;
			RSHI(v, one, m-2);
			AGMX(t, one, v);
			DIVX(v, pi2, t);
			DIVI(z, v, m);
			NEGX(z);
		}
		else
		{
			//ln(x)=pi/2/(1 agm ((4 rsh m)/x)) - m*ln2
			m=p/2+6;
			ex-=m;
			SETX(t, 4);
			SCALEX(t, -m);
			DIVX(v, t, x);
			COPYX(z, v);
			AGMX(t, one, v);
			DIVX(z, pi2, t);
		}
	}
	else
	{
		//x:=(x-1)/(x+1)
		MINUSX(t, x, one);
		PLUSX(v, x, one);
		DIVX(x, t, v);

		//ln(x)=2*(t+t^3/3+t^5/5+t^7/7+t^9/9+...), t=(x-1)/(x+1)
		SETX(t, 1);
		COPYX(z, x);
		if(!isZero(x)){
			COPYX(u, x);
			SQRX(x, u);
			int n=3;
			do{
				MULTX(t, u, x);
				w=t; t=u; u=w;
				DIVI(t, u, n);
				PLUSX(v, z, t);
				w=v; v=z; z=w;
				n+=2;
			} while((t[-1]>=z[-1]-z[-3] || t[-1]>=0) && !isZero(t) && !error);
			MULTI1(z, 2);
		}
	}

	if(ex && !error){
		getln2(y[-4]);
		if(ex<0){
			MULTI(t, lnBase, -ex);
			MINUSX(v, z, t);
		}
		else{
			MULTI(t, lnBase, ex);
			PLUSX(v, z, t);
		}
		w=v; v=z; z=w;
	}
	if(num2 && !error){
		getln2(y[-4]);
		MULTIN(t, ln2, num2);
		MINUSX(v, z, t);
		w=v; v=z; z=w;
	}
	COPYX(y, z);
	FREEX(x);
}

void _stdcall LNX(Pint y, const Pint x)
{
	_LNX(y, x, true);

#ifdef DEBUG_CMP
	Pint z=ALLOCX(y[-4]);
	_LNX(z, x, false);
	assert(&LNX && CMPDBG(y, z));
	FREEX(z);
#endif
}
//-------------------------------------------------------------------
void _stdcall SetPrec(Pint x, Tint _precision)
{
	x[-4] = _precision;
	if(x[-3] > _precision) x[-3]=_precision;
}

void _stdcall INVERSEROOTI(Pint y, Pint x, Tuint n)
{
	Pint t, u, r, a, w;
	Tint p, _precision = 14;

	p=y[-4] + 1;

	a=ALLOCN(3, p, &t, &u, &r);

	if(n==2)
	{
		r[-4]=t[-4]=_precision*2;
		SQRTX2(t, x);
		DIVX(r, t, x);
	}
	else{
		SETX(r, 1);
		DIVI1(r, 2);
	}

	//r:=r+r*(1-x*r^n)/n
	int i=0;
	do
	{
		_precision *= 2;
		if(_precision>p) { _precision=p; i++; }
		SetPrec(r, _precision);
		SetPrec(t, _precision);
		SetPrec(u, _precision);

		if(n==2) SQRX(t, r);
		else if(n!=1) POWI(t, r, n);

		MULTX(u, x, t);
		MINUSX(t, one, u);

		MULTX(u, r, t);
		DIVI1(u, n);
		PLUSX(t, r, u);
		w=t; t=r; r=w;
	} while((_precision<p || (u[-1]>r[-1]-r[-3]) && !isZero(u) && i<50) && !error);
	COPYX(y, r);
	FREEX(a);
}

void _stdcall SQRTX(Pint y, const Pint x)
{
	Pint t, u;
	Tint exp;

#ifndef NDEBUG
	const int SQRTLIM=50;
#else
	const int SQRTLIM=5000;
#endif
	if(y[-4] < SQRTLIM || x[-3]<=0){
		SQRTX2(y, x);
		return;
	}

	ALLOCN(2, y[-4]+2, &t, &u);
	exp= x[-1]/2;
	if(exp!=0)
	{
		COPYX(u, x);
		SCALEX(u, -2*exp);
	}
	else{
		u=x;
	}
	INVERSEROOTI(t, u, 2);
	MULTX(y, t, u);
	SCALEX(y, exp);

#ifdef DEBUG_CMP
	SQRTX2(t, x);
	assert(&SQRTX && CMPDBG(y, t));
#endif

	FREEX(t);
}

void _stdcall DIVX(Pint y, const Pint a, const Pint b)
{
#ifndef NDEBUG
	const int DIVLIM=50;
#else
	const int DIVLIM=5000;
#endif
	if(y[-4]<DIVLIM || b[-3]<20 || a[-3]<=0){
		DIVX2(y, a, b);
		return;
	}

	Pint t, u, r, w, m, b1;
	Tint p, bp, _precision;

	bp = b[-3];
	p=(Tint)(y[-4] + 1);

	m=ALLOCN(4, p, &t, &u, &r, &b1);
	COPYX(b1, b);
	ABSX(b1);

	Tint len=0;
	for(_precision=p; _precision>10; _precision>>=1) len++;

	r[-4]=p>>len;
	Numx Kone1 = { 1,1,0,1,1 };
	DIVX2(r, &Kone1.m, b1);

	int fin=0;

	//r:=r*(2-b*r)
	do
	{
		_precision = p>>len;
		if(len>0){
			len--;
		}
		else{
			fin++;
		}
		//if(_precision>p){ _precision=p; fin++; }
		SetPrec(r, _precision);
		SetPrec(t, _precision);
		SetPrec(u, _precision);
		SetPrec(b1, _precision);
		b1[-3]=min(_precision, bp);

		MULTX(u, b1, r);
		MINUSX(t, two, u);
		MULTX(u, r, t);
		w=u; u=r; r=w;
	} while(fin<2 && !error);/**/
	MULTX(y, r, a);
	if(b[-2]) NEGX(y);

#ifdef DEBUG_CMP
	DIVX2(t, a, b);
	assert(&DIVX && CMPDBG(y, t));
#endif

	FREEX(m);
}


void _stdcall ROOTX(Pint y, const Pint b, const Pint a)
{
	if(isZero(a)){
		ZEROX(y);
		if(b[-2] || isZero(b)){ cerror(1012, "Negative or zero root of zero"); return; } // 0^(-1/n)
	}
	else if(equInt(b, 2)){
		SQRTX(y, a);
	}
	else if(isOneOrMinusOne(b)){
		if(b[-2]){ //(-1)#a = 1/a
			DIVX(y, one, a);
		}
		else{ //1#a = a
			COPYX(y, a);
		}
	}
	else{
		Tint sgn= a[-2];
		if(sgn){
			if(isReal(b)){ cerror(1013, "Not integer root of negative number"); return; }
			a[-2]=0;
		}
		Pint t=ALLOCX(y[-4]);
		LNX(y, a);
		DIVX(t, y, b);
		EXPX(y, t);
		FREEX(t);
		if(sgn){
			a[-2]=sgn;
			if(isOdd(b)) y[-2]=sgn;
			else cerror(1014, "Even root of negative number");
		}
	}
}

void _stdcall POWX(Pint y, const Pint a, const Pint b)
{
	if(isZero(a)){
		ZEROX(y);
		if(b[-2] || isZero(b)){ cerror(1015, "Not positive power of zero"); return; } // 0^(-n) or 0^0
	}
	else if((b[0]==TintMin && b[-3]==1 && b[-1]==0
	 || b[0]==1 && b[1]==2 && b[-3]==-2) && b[-2]==0){
		SQRTX(y, a);
	}
	else{
		Tint sgn= a[-2];
		if(sgn){
			if(isReal(b)){ cerror(1016, "Not integer power of negative number"); return; }
			a[-2]=0;
		}
		if(is32bit(b)){
			POWI(y, a, to32bit(b));
		}
		else{
			Pint t=ALLOCX(y[-4]);
			LNX(y, a);
			MULTX(t, y, b);
			EXPX(y, t);
			FREEX(t);
		}
		if(sgn){
			a[-2]=sgn;
			if(isOdd(b)) y[-2]=sgn;
		}
	}
}
//-------------------------------------------------------------------
void _stdcall POWI(Pint y, const Pint x, __int64 n)
{
	Pint z, t, u, w, a;

	if(isZero(x)){
		ZEROX(y);
		if(n<=0) cerror(1015, "Not positive power of zero"); // 0^(-n) nebo 0^0
		return;
	}
	bool sgn= n<0;
	if(sgn) n=-n;
	a=ALLOCN(2, y[-4], &t, &u);
	z=y;
	if(n&1) COPYX(z, x);
	else SETX(z, 1);
	COPYX(u, x);

	for(n>>=1; n>0; n>>=1){
		SQRX(t, u);
		w=t; t=u; u=w;
		if(n&1){
			MULTX(t, z, u);
			w=t; t=z; z=w;
		}
	}
	if(sgn){
		//y=1/z
		if(z==y){
			DIVX(t, one, z);
			COPYX(y, t);
		}
		else{
			DIVX(y, one, z);
		}
	}
	else{
		COPYX(y, z);
	}
	FREEX(a);
}
//-------------------------------------------------------------------

void _stdcall LSHI(Pint y, const Pint a, Tint n)
{
#ifdef ARIT64
	int i= n&63;
	if(i){
		MULTI(y, a, ((Tuint)1)<<i);
	}
	else{
		COPYX(y, a);
	}
	SCALEX(y, n>>6);
#else
	int i= n&31;
	if(i){
		MULTI(y, a, 1<<i);
	}
	else{
		COPYX(y, a);
	}
	SCALEX(y, n>>5);
#endif
}

void _stdcall RSHI(Pint y, const Pint a, Tint n)
{
	//MULTI is faster than DIVI
	LSHI(y, a, -n);
}

void _stdcall DIVI1(Pint y, Tuint n)
{
	DIVI(y, y, n);
}

void _stdcall COMBINI(Pint y, Tuint n, Tuint m)
{
	if(m>n){ ZEROX(y); return; }
	if(m>n/2) m=n-m;
	ONEX(y);
	for(Tuint i=1; i<=m; i++){
		MULTI1(y, n);
		DIVI1(y, i);
		n--;
	}
}

void _stdcall PERMUTI(Pint y, Tuint n, Tuint m)
{
	if(m>n){ ZEROX(y); return; }
	ONEX(y);
	while(m--){
		MULTI1(y, n);
		n--;
	}
}

void _stdcall LSHX(Pint y, const Pint a, const Pint b)
{
	if(!isInt(b)){
		cerror(1017, "The shift operand is not integer");
		return;
	}
	LSHI(y, a, toInt(b));
}

void _stdcall RSHX(Pint y, const Pint a, const Pint b)
{
	if(!isInt(b)){
		cerror(1017, "The shift operand is not integer");
		return;
	}
	RSHI(y, a, toInt(b));
}

void _stdcall RSHIX(Pint y, const Pint a, const Pint b)
{
	RSHX(y, a, b);
	TRUNCX(y);
}

void _stdcall COMBINX(Pint y, const Pint a, const Pint b)
{
	if(!isDword(a) || !isDword(b)){
		cerror(1018, "Operand of combination is not integer");
		return;
	}
	COMBINI(y, toDword(a), toDword(b));
}

void _stdcall PERMUTX(Pint y, const Pint a, const Pint b)
{
	if(!isDword(a) || !isDword(b)){
		cerror(1019, "Operand of permutation is not integer");
		return;
	}
	PERMUTI(y, toDword(a), toDword(b));
}

void _stdcall FACTORIALX(Pint y, Pint x)
{
	if(!isDword(x)){
		cerror(1020, "Operand of factorial is not integer");
		return;
	}
	FACTORIALI(y, toDword(x));
}

void _stdcall FFACTX(Pint y, Pint x)
{
	if(!isDword(x)){
		cerror(1020, "Operand of factorial is not integer");
		return;
	}
	FFACTI(y, toDword(x));
}
//-------------------------------------------------------------------
void _stdcall GCDX(Pint y, const Pint a, const Pint b)
{
	if(isReal(a) || isReal(b)){
		cerror(1026, "The greatest common divisor of real numbers");
		return;
	}
	Tint p;
	Pint t, u, v, w, m;
	p= max(a[-4], b[-4]);
	p= max(p, y[-4]);
	m=ALLOCN(3, p, &t, &u, &v);
	COPYX(u, a);
	COPYX(v, b);
	while(!isZero(v)){
		MODX(t, u, v);
		w=v; v=t; t=u; u=w;
	}
	COPYX(y, u);
	y[-2]=0;
	FREEX(m);
}

void _stdcall LCMX(Pint y, const Pint a, const Pint b)
{
	if(isReal(a) || isReal(b)){
		cerror(1027, "The least common multiple of real numbers");
		return;
	}
	Pint t, u;
	ALLOCN(2, abs(a[-3])+abs(b[-3]), &t, &u);
	MULTX(t, a, b);
	GCDX(u, a, b);
	DIVX(y, t, u);
	FREEX(t);
}

//fibonacci(x)= (((1+sqrt5)/2)^x-((1-sqrt5)/2)^x)/sqrt5
void _stdcall FIBONACCIX(Pint y, const Pint x)
{
	Pint t, u, v, s;

	ALLOCN(4, y[-4], &t, &u, &v, &s);
	SETX(t, 5);
	SQRTX(s, t);
	PLUSX(t, s, one);
	DIVI1(t, 2);
	POWX(u, t, x);
	MINUSX(t, one, s);
	DIVI1(t, 2);
	POWX(v, t, x);
	MINUSX(t, u, v);
	DIVX(y, t, s);
	ROUNDX(y);
	FREEX(t);
}
//-------------------------------------------------------------------
Tuint primeTab[]={
	2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59,
	61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131,
	137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197,
	199, 211, 223, 227, 229, 233,
	239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317,
	331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419,
	421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503,
	509, 521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607,
	613, 617, 619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701,
	709, 719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811,
	821, 823, 827, 829, 839,
	0
};

Tuint primeDiffTab[49]={
	10, 2, 4, 2, 4, 6, 2, 6, 4, 2, 4, 6, 6, 2, 6, 4, 2, 6,
	4, 6, 8, 4, 2, 4, 2, 4, 8, 6, 4, 6, 2, 4, 6, 2, 6, 6, 4,
	2, 4, 6, 2, 6, 4, 2, 4, 2, 10, 2, 0
};

void _stdcall DIVISORX(Pint y, const Pint x)
{
	Tuint last, d, *p, i, sgn;

	if(isReal(x) || x[-1]>y[-4]){
		cerror(1030, "divisor of real number");
		return;
	}
	if(CMPU(x, one)==0){
		COPYX(y, one);
		return;
	}
	for(p=primeTab;; p++){
		d=*p;
		if(!d) break;
		if(MODI(x, d)==0){ SETX(y, d); return; }
	}
	Numx Ktmp;
	Pint tmp=&Ktmp.m;
	tmp[-4]=1;
	sgn=x[-2];
	x[-2]=0;
	SQRTX(tmp, x);
	last= TuintMax-10;
	if(tmp[-1]==1) last=tmp[0];
	p=primeDiffTab;
	for(d=210*4+1; d<=last && !error; d+=i){
		if(MODI(x, d)==0){ SETX(y, d); x[-2]=sgn; return; }
		i=*p;
		if(!i){
			p=primeDiffTab;
			i=*p;
		}
		p++;
	}
	if(last==TuintMax-10){
		cerror(1031, "Computation is too difficult");
	}
	else{
		//x is prime
		COPYX(y, x);
	}
	x[-2]=sgn;
}

void _stdcall PRIMEX(Pint y0, const Pint x)
{
	Pint t, y, w;

	y=y0;
	COPYX(y, x);
	if(x[-2] || isZero(x)) ONEX(y);
	TRUNCX(y);
	if(y[-1]>y[-4]){
		cerror(1030, "divisor of real number");
		return;
	}
	if(CMPU(y, one)==0){
		SETX(y, 2);
		return;
	}
	t=ALLOCX(y[-4]);
	PLUSX(t, y, MODI(y, 2)==0 ? one : two);
	w=t; t=y; y=w;
	while(!error){
		DIVISORX(t, y);
		if(CMPU(t, y)==0) break;
		PLUSX(t, y, two);
		w=t; t=y; y=w;
	}
	if(y!=y0){ COPYX(y0, y); t=y; }
	FREEX(t);
}

void _stdcall ISPRIMEX(Pint y, const Pint x)
{
	DIVISORX(y, x);
	EQUALX(y, y, x);
}
//-------------------------------------------------------------------
void _stdcall FTOCX(Pint y, const Pint x)
{
	Numx Ktmp;
	Pint tmp=&Ktmp.m;
	tmp[-4]=1;
	SETX(tmp, 32);
	MINUSX(y, x, tmp);
	MULTI1(y, 5);
	DIVI1(y, 9);
}

void _stdcall CTOFX(Pint y, const Pint x)
{
	Pint t=ALLOCX(y[-4]);
	MULTI(t, x, 9);
	DIVI1(t, 5);
	Numx Ktmp;
	Pint tmp=&Ktmp.m;
	tmp[-4]=1;
	SETX(tmp, 32);
	PLUSX(y, t, tmp);
	FREEX(t);
}

void _stdcall DMSTODECX(Pint y, const Pint x)
{
	Pint t, u, v;

	COPYX(y, x);
	TRUNCX(y);
	//y=degrees
	ALLOCN(3, y[-4], &t, &u, &v);
	MINUSX(t, x, y);
	MULTI(u, t, 100);
	//y=degrees, u=minutes+seconds/100
	COPYX(t, u);
	TRUNCX(t);
	//y=degrees, t=minutes
	MINUSX(v, u, t);
	DIVI1(t, 60);
	PLUSX(u, y, t);
	//u=degrees+minutes/60, v=seconds/100
	DIVI1(v, 36);
	PLUSX(y, u, v);
	FREEX(t);
}

void _stdcall DECTODMSX(Pint y, const Pint x)
{
	Pint t, u, v;

	ALLOCN(3, y[-4], &t, &u, &v);
	SETX(t, 9);
	SCALEX(t, -y[-4]+1);
	t[-2]=x[-2];
	PLUSX(u, t, x);
	//u=x+e
	COPYX(y, u);
	TRUNCX(y);
	//y=degrees
	MINUSX(t, u, y);
	MULTI(u, t, 60);
	//y=degrees, u=minutes+seconds/60
	COPYX(t, u);
	TRUNCX(t);
	//y=degrees, t=minutes
	MINUSX(v, u, t);
	DIVI1(t, 100);
	PLUSX(u, y, t);
	//u=degrees+minutes/100, v=seconds/60
	MULTI1(v, 3);
	DIVI1(v, 500);
	PLUSX(y, u, v);
	FREEX(t);
}

void _stdcall RADTODEGX(Pint y, const Pint x)
{
	getpi(y[-4]+1);
	DIVX(y, x, pi);
	MULTI1(y, 180);
}

void _stdcall DEGTORADX(Pint y, const Pint x)
{
	getpi(y[-4]+1);
	MULTX(y, x, pi);
	DIVI1(y, 180);
}

void _stdcall RADTOGRADX(Pint y, const Pint x)
{
	getpi(y[-4]+1);
	DIVX(y, x, pi);
	MULTI1(y, 200);
}

void _stdcall GRADTORADX(Pint y, const Pint x)
{
	getpi(y[-4]+1);
	MULTX(y, x, pi);
	DIVI1(y, 200);
}

void _stdcall GRADTODEGX(Pint y, const Pint x)
{
	DIVI(y, x, 10);
	MULTI1(y, 9);
}

void _stdcall DEGTOGRADX(Pint y, const Pint x)
{
	DIVI(y, x, 9);
	MULTI1(y, 10);
}

void _stdcall DEGX(Pint y, const Pint x)
{
	switch(angleMode){
		default:
			COPYX(y, x);
			break;
		case ANGLE_GRAD:
			DEGTOGRADX(y, x);
			break;
		case ANGLE_RAD:
			DEGTORADX(y, x);
			break;
	}
}

void _stdcall RADX(Pint y, const Pint x)
{
	switch(angleMode){
		default:
			COPYX(y, x);
			break;
		case ANGLE_DEG:
			RADTODEGX(y, x);
			break;
		case ANGLE_GRAD:
			RADTOGRADX(y, x);
			break;
	}
}

void _stdcall GRADX(Pint y, const Pint x)
{
	switch(angleMode){
		default:
			COPYX(y, x);
			break;
		case ANGLE_DEG:
			GRADTODEGX(y, x);
			break;
		case ANGLE_RAD:
			GRADTORADX(y, x);
			break;
	}
}

void _stdcall TODEGX(Pint y, const Pint x)
{
	switch(angleMode){
		default:
			COPYX(y, x);
			break;
		case ANGLE_GRAD:
			GRADTODEGX(y, x);
			break;
		case ANGLE_RAD:
			RADTODEGX(y, x);
			break;
	}
}

void _stdcall TORADX(Pint y, const Pint x)
{
	switch(angleMode){
		default:
			COPYX(y, x);
			break;
		case ANGLE_DEG:
			DEGTORADX(y, x);
			break;
		case ANGLE_GRAD:
			GRADTORADX(y, x);
			break;
	}
}

void _stdcall TOGRADX(Pint y, const Pint x)
{
	switch(angleMode){
		default:
			COPYX(y, x);
			break;
		case ANGLE_DEG:
			DEGTOGRADX(y, x);
			break;
		case ANGLE_RAD:
			RADTOGRADX(y, x);
			break;
	}
}

void angleResult(Pint x)
{
	if(angleMode==ANGLE_RAD) return;
	Pint t=ALLOCX(x[-4]);
	switch(angleMode){
		case ANGLE_DEG:
			RADTODEGX(t, x);
			break;
		case ANGLE_GRAD:
			RADTOGRADX(t, x);
			break;
	}
	COPYX(x, t);
	FREEX(t);
}

//a div b= trunc(a/b)
void _stdcall IDIVX(Pint y, const Pint a, const Pint b)
{
	DIVX(y, a, b);
	TRUNCX(y);
}

//a mod b= a-trunc(a/b)*b
void _stdcall MODX(Pint y, const Pint a, const Pint b)
{
	if(isZero(b)){
		cerror(1010, "Division by zero");
		return;
	}
	if(isZero(a)){
		ZEROX(y);
		return;
	}
	Tint _precision = a[-1]-b[-1];
	if(_precision>=a[-4]){
		cerror(1063, "The first operand of MOD function is longer than precision");
		return;
	}
	if(_precision<0){
		COPYX(y, a);
		return;
	}
	Pint t= ALLOCX(_precision+2);
	IDIVX(t, a, b);
	Pint u= ALLOCX(y[-4]);
	MULTX(u, t, b);
	MINUSX(y, a, u);
	FREEX(u);
	FREEX(t);
}

//-------------------------------------------------------------------

//adjust x, so that |x|<=PI/2
//return true if odd number of PI was subtracted
bool _stdcall ARGPI(Pint y0, const Pint x)
{
	Pint t, u, y, w;
	bool odd=false;

	y=y0;
	switch(angleMode){
		case ANGLE_RAD:
			COPYX(y, x);
			break;
		case ANGLE_DEG:
			DEGTORADX(y, x);
			break;
		case ANGLE_GRAD:
			GRADTORADX(y, x);
			break;
	}
	getpi(y[-4]);
	t= ALLOCX(y[-4]);
	while(CMPU(y, pi2)>0 && !error){
		IDIVX(t, y, pi);
		if(!isZero(t)){
			if(isOdd(t)) odd=!odd;
			u= ALLOCX(y[-4]);
			MULTX(u, t, pi);
			if(u[-1]>u[-4]) cerror(1060, "Trigonometric function operand is too big");
			MINUSX(t, y, u);
			w=y; y=t; t=w;
			FREEX(u);
			if(CMPU(y, pi2)<=0) break;
		}
		Tint sgn = y[-2];
		if(sgn) PLUSX(t, y, pi);
		else MINUSX(t, y, pi);
		w=y; y=t; t=w;
		odd=!odd;
		if(y[-2]!=sgn) break;
	}
	if(y!=y0){
		COPYX(y0, y);
		t=y;
	}
	FREEX(t);
	return odd;
}

void _stdcall SINCOS(Pint y0, Pint x, Tint n, bool hyp)
{
	Pint x2, t, u, w, y, m;

	if(isZero(x)) return; //sin(0)=0, cos(0)=1
	y=y0;
	m=ALLOCN(3, y[-4], &t, &u, &x2);
	SQRX(x2, x);
	COPYX(u, y);
	do{
		MULTX(t, u, x2);
		DIVI(u, t, n++);
		DIVI(t, u, n++);
		if(hyp) PLUSX(u, y, t);
		else MINUSX(u, y, t); //z-=x^n/n!
		MULTX(y, t, x2);
		DIVI(t, y, n++);
		DIVI(y, t, n++);
		PLUSX(t, u, y); //z+=x^n/n!
		w=u; u=y; y=t; t=w;
	} while((u[-1]>=y[-1]-y[-3] || u[-1]>=0) && !isZero(u) && !error);
	COPYX(y0, y);
	FREEX(m);
}

//sin(x)= x - x^3/3! + x^5/5! - ...
void _stdcall SINX0(Pint y, const Pint x)
{
	COPYX(y, x);
	SINCOS(y, x, 2, false);
}

//cos(x)= 1 - x^2/2! + x^4/4! - ...
void _stdcall COSX0(Pint y, const Pint x)
{
	Pint t, u;
	Tint k, p;

	p = y[-4] + 2;
	if(p<40) k=8;
	else if(p<200) k=p/7;
	else if(p<1000) k=p/11;
	else if(p<3000) k=p/13;
	else if(p<6000) k=p/15;
	else k=450;

	ALLOCN(2, p + k/(TintBits/2), &t, &u);

	//cos(x)= 2*(cos(x/2))^2 - 1
	RSHI(t, x, k);
	ONEX(u);
	SINCOS(u, t, 1, false);
	while(--k >= 0 && !error){
		t[-4] = p + k/(TintBits/2);
		SQRX(t, u);
		MULTI1(t, 2);
		MINUSX(k ? u : y, t, one);
	}
	FREEX(t);
}

void _stdcall SINX(Pint y, const Pint x0)
{
	Pint x, t;
	bool ng;

	if(angleMode==ANGLE_DEG && x0[-3]==-2 && x0[1]==1)
	{
		switch(x0[0])
		{
			case 30: //sin(deg 30)=1/2
				COPYX(y, half);
				y[-2]=x0[-2];
				return;
			case 45: //sin(deg 45)=sqrt(2)/2
				SQRTX(y, two);
				DIVI1(y, 2);
				y[-2]=x0[-2];
				return;
			case 60: //sin(deg 60)=sqrt(3)/2
				SQRTX(y, three);
				DIVI1(y, 2);
				y[-2]=x0[-2];
				return;
			case 90: //sin(deg 90)=1
				ONEX(y);
				y[-2]=x0[-2];
				return;
		}
	}

	x=ALLOCN(2, y[-4], &x, &t);
	ng=ARGPI(x, x0);
	if(x[-2]){
		x[-2]=0;
		ng=!ng;  //sin is odd function
	}
	if(isZero(x)){
		ZEROX(y);
	}
	else if(x[-1]>=0){
		//sin(x)=cos(PI/2-x)
		MINUSX(t, pi2, x);
		COSX0(y, t);
	}
	else{
		//x is near zero
		SINX0(y, x);
	}
	if(ng) NEGX(y);
	FREEX(x);
}

void _stdcall COSX(Pint y, const Pint x0)
{
	Pint x, t;
	bool ng;

	if(angleMode==ANGLE_DEG && x0[-3]==-2 && x0[1]==1)
	{
		switch(x0[0])
		{
			case 30: //cos(deg 30)=sqrt(3)/2
				SQRTX(y, three);
				DIVI1(y, 2);
				return;
			case 45: //cos(deg 45)=sqrt(2)/2
				SQRTX(y, two);
				DIVI1(y, 2);
				return;
			case 60: //cos(deg 60)=1/2
				COPYX(y, half);
				return;
			case 90: //cos(deg 90)=0
				ZEROX(y);
				return;
		}
	}

	x=ALLOCN(2, y[-4], &x, &t);
	ng=ARGPI(x, x0);
	x[-2]=0; //cos is even function
	MINUSX(t, pi2, x);
	if(t[-1]<0){ //x is near PI/2
		//cos(x)=sin(PI/2-x)
		SINX0(y, t);
	}
	else{
		COSX0(y, x);
	}
	if(ng) NEGX(y);
	FREEX(x);
}

//sec(x)=1/cos(x)
void _stdcall SECX(Pint y, const Pint x)
{
	Pint t=ALLOCX(y[-4]);
	COSX(t, x);
	DIVX(y, one, t);
	FREEX(t);
}

//csc(x)=1/sin(x)
void _stdcall CSCX(Pint y, const Pint x)
{
	Pint t=ALLOCX(y[-4]);
	SINX(t, x);
	DIVX(y, one, t);
	FREEX(t);
}

static void _stdcall TANCOTG(Pint y, const Pint x, bool co)
{
	Pint s, c, w;
	int r=0;

	if(isInt(x)){
		switch(angleMode){
			case ANGLE_DEG:
				r=90;
				break;
			case ANGLE_GRAD:
				r=100;
				break;
		}
		if(r){
			Tint i=toInt(x);
			if(i%r==0){
				//tan(k*180°)=0
				ZEROX(y);
				if(((i/r)&1) ^ co) cerror(1034, "Infinite result"); //tan(90°)
				return;
			}
		}
	}
	ALLOCN(2, y[-4], &s, &c);
	SINX(s, x);
	c=ALLOCX(y[-4]);
	COSX(c, x);
	if(co){ w=s; s=c; c=w; }
	DIVX(y, s, c);
	FREEX(s);
}

//tan(x)=sin(x)/cos(x)
void _stdcall TANX(Pint y, const Pint x)
{
	TANCOTG(y, x, false);
}

//cotg(x)=cos(x)/sin(x)
void _stdcall COTGX(Pint y, const Pint x)
{
	TANCOTG(y, x, true);
}

//-------------------------------------------------------------------

//|x|<=1
void _stdcall ATNX0(Pint y, const Pint x)
{
	Pint x1, x2, t, u, v;
	bool big;
	Tint n, sgn;

	if(isZero(x)){
		//arctan(0)=0
		ZEROX(y);
		return;
	}
	sgn=x[-2];
	if(isOneOrMinusOne(x)){
		//arctan(1)=pi/4
		getpi(y[-4]);
		COPYX(y, pi4);
		y[-2]=sgn;
		return;
	}
	x[-2]=0;
	ALLOCN(4, y[-4], &t, &u, &v, &x2);
	if(isFraction(x)) big= Tuint(x[0])/(float)Tuint(x[1]) > 0.414214;
	else big= (x[-1]==0) && Tuint(x[0]) >
#ifdef ARIT64
		7640891576956012809; //(sqrt(2)-1)*2^64
#else
		1779033704; //(sqrt(2)-1)*2^32
#endif
	if(big){
		//arctan(x)= PI/4- arctan((1-x)^2/(1-x^2)), |x|<1
		MINUSX(t, one, x);
		SQRX(u, t);
		SQRX(t, x);
		MINUSX(v, one, t);
		DIVX(t, u, v);
		x1=t;
	}
	else{
		x1=x;
	}
	COPYX(y, x1);
	COPYX(u, x1);
	SQRX(x2, x1);
	n=3;
	do{
		MULTX(v, u, x2);
		DIVI(u, v, n);
		MINUSX(t, y, u);
		n+=2;
		MULTX(u, v, x2);
		DIVI(v, u, n);
		PLUSX(y, t, v);
		n+=2;
	} while((u[-1]>=y[-1]-y[-3] || u[-1]>=0) && !isZero(u) && !error);
	if(big){
		getpi(y[-4]);
		MINUSX(t, pi4, y);
		COPYX(y, t);
	}
	FREEX(t);
	if(sgn){
		y[-2]=x[-2]=sgn;
	}
}

//arcsin(x)= 2*arctan(x/(1+sqrt(1-x^2)))
void _stdcall ASINX1(Pint y, const Pint x)
{
	if(CMPU(x, one)>0){
		cerror(1021, "Operand of arcsin is not in <-1,+1>");
		return;
	}
	if(isOneOrMinusOne(x)){
		//arcsin(1)=pi/2
		getpi(y[-4]);
		COPYX(y, pi2);
		y[-2]=x[-2];
		return;
	}
	if((x[0]==TintMin && x[-3]==1 && x[-1]==0
		|| x[0]==1 && x[1]==2 && x[-3]==-2)){
		//arcsin(1/2)=pi/6
		getpi(y[-4]);
		COPYX(y, pi2);
		DIVI1(y, 3);
		y[-2]=x[-2];
		return;
	}
	Pint t=ALLOCX(y[-4]);
	SQRX(t, x);
	MINUSX(y, one, t);
	SQRTX(t, y);
	PLUSX(y, t, one);
	DIVX(t, x, y);
	ATNX0(y, t);
	FREEX(t);
	MULTI1(y, 2);
}

void _stdcall ASINX(Pint y, const Pint x)
{
	ASINX1(y, x);
	angleResult(y);
}

//arccos(x)=pi/2-arcsin(x)
void _stdcall ACOSX(Pint y, const Pint x)
{
	if(CMPU(x, one)>0){
		cerror(1022, "Operand of arccos is not in <-1,+1>");
		return;
	}
	getpi(y[-4]);
	if(isOneOrMinusOne(x)){
		if(x[-2]){
			//arccos(-1)=pi
			COPYX(y, pi);
		}
		else{
			//arccos(1)=0
			ZEROX(y);
		}
	}
	else{
		Pint t=ALLOCX(y[-4]);
		ASINX1(t, x);
		MINUSX(y, pi2, t);
		FREEX(t);
	}
	angleResult(y);
}

//arctan(x)= x - x^3/3 + x^5/5 - ...
void _stdcall ATANX(Pint y, const Pint x)
{
	if(CMPU(x, one)>0){
		getpi(y[-4]);
		Pint t=ALLOCX(y[-4]);
		DIVX(y, one, x);
		ATNX0(t, y);
		if(x[-2]){
			PLUSX(y, pi2, t);
			NEGX(y);
		}
		else{
			MINUSX(y, pi2, t);
		}
		FREEX(t);
	}
	else{
		ATNX0(y, x);
	}
	angleResult(y);
}

//arccotg(x)=arctan(1/x)
void _stdcall ACOTGX(Pint y, const Pint x)
{
	Pint t=ALLOCX(y[-4]);
	if(CMPU(x, one)>0){
		DIVX(t, one, x);
		ATNX0(y, t);
	}
	else{
		ATNX0(t, x);
		getpi(y[-4]);
		if(x[-2]){
			PLUSX(y, pi2, t);
			NEGX(y);
		}
		else{
			MINUSX(y, pi2, t);
		}
	}
	FREEX(t);
	angleResult(y);
}

void _stdcall ATAN2X(Pint y, const Pint a, const Pint b)
{
	Pint t, p;

	if(isZero(a) && isZero(b)){
		cerror(1032, "Angle of point [0,0]");
		return;
	}
	getpi(y[-4]);

	if(isZero(a)){
		if(b[-2]) COPYX(y, pi);
		else ZEROX(y);
	}
	else if(isZero(b)){
		COPYX(y, pi2);
		y[-2]=a[-2];
	}
	else{
		t=ALLOCX(y[-4]);
		p=0;
		if(b[-2]) p=pi;
		if(CMPU(a, b)>0){
			p=pi2;
			DIVX(y, b, a);
		}
		else{
			DIVX(y, a, b);
		}
		if(a[-2]) y[-2]=b[-2];
		ATNX0(t, y);
		if(!p){
			COPYX(y, t);
		}
		else if(p==pi){
			PLUSX(y, p, t);
		}
		else{
			MINUSX(y, p, t);
		}
		if(a[-2]) NEGX(y);
		FREEX(t);
	}
	angleResult(y);
}

//-------------------------------------------------------------------
//sinh(x)= (exp(x)-exp(-x))/2 = x + x^3/3! + x^5/5! + ...
void _stdcall SINHX(Pint y, const Pint x)
{
	COPYX(y, x);
	SINCOS(y, x, 2, true);
}

//cosh(x)= (exp(x)+exp(-x))/2 = 1 + x^2/2! + x^4/4! + ...
void _stdcall COSHX(Pint y, const Pint x)
{
	ONEX(y);
	SINCOS(y, x, 1, true);
}

static void _stdcall TANHCOTGH(Pint y, const Pint x, bool co)
{
	Pint s, c, w;

	ALLOCN(2, y[-4], &s, &c);
	SINHX(s, x);
	c=ALLOCX(y[-4]);
	COSHX(c, x);
	if(co){ w=s; s=c; c=w; }
	DIVX(y, s, c);
	FREEX(s);
}

//tanh(x)=sinh(x)/cosh(x)
void _stdcall TANHX(Pint y, const Pint x)
{
	TANHCOTGH(y, x, false);
}

//cotgh(x)=cosh(x)/sinh(x)
void _stdcall COTGHX(Pint y, const Pint x)
{
	TANHCOTGH(y, x, true);
}

//hypot(a,b)=sqrt(a^2+b^2)
void _stdcall HYPOTX(Pint y, const Pint a, const Pint b)
{
	Pint t, u;

	ALLOCN(2, y[-4], &t, &u);
	SQRX(t, a);
	SQRX(y, b);
	PLUSX(u, t, y);
	SQRTX(y, u);
	FREEX(t);
}
//-------------------------------------------------------------------
void _stdcall NOTX(Pint y, const Pint x)
{
	if(x[-2] && x[-3]){
		MINUSU(y, x, one);
		y[-2]=0;
	}
	else{
		PLUSU(y, x, one);
		y[-2]=1;
	}
}

void _stdcall bitop(int op, Pint y, const Pint a0, const Pint b0)
{
	Pint a=a0, b=b0;
	Numx Ktmp;
	Pint tmp=&Ktmp.m;
	tmp[-4]=1;
	SETX(tmp, 1);
	SCALEX(tmp, max(a[-1], b[-1]));
	if(tmp[-1]==TintMin) overflow();

	if(a[-2]){
		a=ALLOCX(y[-4]);
		MINUSU(a, tmp, a0);
	}
	if(b[-2]){
		b=ALLOCX(y[-4]);
		MINUSU(b, tmp, b0);
	}
	switch(op){
		case 0:
			ANDU(y, a, b);
			y[-2]= a0[-2] & b0[-2];
			break;
		case 1:
			ORU(y, a, b);
			y[-2]=1;
			break;
		default:
			XORU(y, a, b);
			y[-2]= a0[-2] ^ b0[-2];
			break;
	}
	if(y[-2]){
		Pint t=ALLOCX(y[-4]);
		MINUSU(t, tmp, y);
		t[-2]=1;
		COPYX(y, t);
		FREEX(t);
	}
	if(a!=a0) FREEX(a);
	if(b!=b0) FREEX(b);
}

void _stdcall ANDX(Pint y, const Pint a, const Pint b)
{
	if((a[-2]|b[-2])==0){
		ANDU(y, a, b);
	}
	else{
		ZEROX(y);
		if(isZero(a) || isZero(b)){
		}
		else if(a[-1]-b[-1]>=y[-4]){
			if(b[-2]) COPYX(y, a);
		}
		else if(b[-1]-a[-1]>=y[-4]){
			if(a[-2]) COPYX(y, b);
		}
		else{
			bitop(0, y, a, b);
		}
	}
}

void _stdcall ORX(Pint y, const Pint a, const Pint b)
{
	if((a[-2]|b[-2])==0){
		ORU(y, a, b);
	}
	else if(isZero(a)){
		COPYX(y, b);
	}
	else if(isZero(b)){
		COPYX(y, a);
	}
	else if(a[-1]-b[-1]>=y[-4]){
		if(b[-2]) COPYX(y, b);
		else COPYX(y, a);
	}
	else if(b[-1]-a[-1]>=y[-4]){
		if(a[-2]) COPYX(y, a);
		else COPYX(y, b);
	}
	else{
		bitop(1, y, a, b);
	}
}

void _stdcall XORX(Pint y, const Pint a, const Pint b)
{
	if((a[-2]|b[-2])==0){
		XORU(y, a, b);
	}
	else if(isZero(a)){
		COPYX(y, b);
	}
	else if(isZero(b)){
		COPYX(y, a);
	}
	else{
		bitop(2, y, a, b);
	}
}

//p bitnand q = not(p and q)
void _stdcall NANDBX(Pint y, const Pint a, const Pint b)
{
	Pint t=ALLOCX(y[-4]);
	ANDX(t, a, b);
	NOTX(y, t);
	FREEX(t);
}

//p bitnor q == not(p or q)
void _stdcall NORBX(Pint y, const Pint a, const Pint b)
{
	Pint t=ALLOCX(y[-4]);
	ORX(t, a, b);
	NOTX(y, t);
	FREEX(t);
}

//p bitimp q == not p or q
void _stdcall IMPBX(Pint y, const Pint a, const Pint b)
{
	Pint t=ALLOCX(y[-4]);
	NOTX(t, a);
	ORX(y, t, b);
	FREEX(t);
}

//p biteqv q == not (p xor q)
void _stdcall EQVBX(Pint y, const Pint a, const Pint b)
{
	Pint t=ALLOCX(y[-4]);
	XORX(t, a, b);
	NOTX(y, t);
	FREEX(t);
}

//-------------------------------------------------------------------

//p nand q = (p==0 or q==0)
void _stdcall NANDX(Pint y, const Pint a, const Pint b)
{
	SETX(y, isZero(a) || isZero(b));
}

//p nor q == (p==0 and q==0)
void _stdcall NORX(Pint y, const Pint a, const Pint b)
{
	SETX(y, isZero(a) && isZero(b));
}

//p imp q == (p==0 or q!=0)
void _stdcall IMPX(Pint y, const Pint a, const Pint b)
{
	SETX(y, isZero(a) || !isZero(b));
}

//p eqv q == (p==0)==(q==0)
void _stdcall EQVX(Pint y, const Pint a, const Pint b)
{
	SETX(y, isZero(a) == isZero(b));
}

//-------------------------------------------------------------------

void _stdcall IFX(Pint y, const Pint *A)
{
	COPYX(y, A[isZero(A[0]) ? 2 : 1]);
}

void _stdcall EQUALX(Pint y, const Pint a, const Pint b)
{
	SETX(y, CMPX(a, b)==0);
}

void _stdcall NOTEQUALX(Pint y, const Pint a, const Pint b)
{
	SETX(y, CMPX(a, b)!=0);
}

void _stdcall GREATERX(Pint y, const Pint a, const Pint b)
{
	SETX(y, CMPX(a, b)>0);
}

void _stdcall LESSX(Pint y, const Pint a, const Pint b)
{
	SETX(y, CMPX(a, b)<0);
}

void _stdcall GREATEREQX(Pint y, const Pint a, const Pint b)
{
	SETX(y, CMPX(a, b)>=0);
}

void _stdcall LESSEQX(Pint y, const Pint a, const Pint b)
{
	SETX(y, CMPX(a, b)<=0);
}

//-------------------------------------------------------------------
/*
//Chudnovsky algorithm
// 1/(12*sumfor(k,0,N,(-1)^k*(6*k)!*(13591409+545140134*k)/((3*k)!*k!^3*640320^(3*k+3/2)

void _stdcall PI2(Pint y)
{
Pint r,s,t,u,v,w,a;
Tuint k,i;

Numx Kc;
Pint c=&Kc.m;
c[-4]=1;
SETX(c,545140134);

a=ALLOCN(5,y[-4],&r,&s,&t,&u,&v);
DIVI(u,one,640320);
SETX(t,640320);
SQRTX(v,t);
DIVX(t,u,v);
SETX(u,13591409);
MULTI(s,t,13591409);

//s=sum
//t=(6*k)!/((3*k)!*k!^3*640320^(3*k+3/2)
//u=13591409+545140134*k
k=1;
do{
if(t[-1]<-1){
t[-4]=y[-4]+t[-1]+1;
if(t[-3]>0) t[-3]=t[-4];
}
PLUSX(v,u,c);
w=u; u=v; v=w;
for(i=6*k-1; i>6*k-6; i--) MULTI(t,t,i);
if(k<21846) DIVI(t,t,(3*k-1)*(3*k-2)/2);
else if(k&1){ DIVI(t,t,(3*k-1)/2); DIVI(t,t,3*k-2); }
else { DIVI(t,t,3*k-1); DIVI(t,t,3*k/2-1); }
if(k<1626) DIVI(t,t,k*k*k);
else if(k<65536){ DIVI(t,t,k*k); DIVI(t,t,k); }
else{ DIVI(t,t,k); DIVI(t,t,k); DIVI(t,t,k); }
DIVI(t,t,640320); DIVI(t,t,640320); DIVI(t,t,640320);
MULTX1(v,t,u);
if(k&1) MINUSX(r,s,v);
else PLUSX(r,s,v);
w=r; r=s; s=w;
k++;
}while((-v[-1]<=y[-4]) && !isZero(v) && !error);
MULTI(s,s,12);
DIVX(y,one,s);
FREEX(a);
}
*/
/*
//Ramanujan algorithm
// 9801/(2*sqrt2*sumfor(k,0,30,(4*k)!*(1103+26390*k)/(k!^4*396^(4*k))))

void _stdcall PI3(Pint y)
{
Pint r,s,t,u,v,w,a;
Tuint k,i,u1,u2;

Numx Kc;
Pint c=&Kc.m;
c[-4]=2;
SETX(c,26390);

a=ALLOCN(5,y[-4],&r,&s,&t,&u,&v);
SETX(t,1);
u1=1103;
SETX(s,1103);

//s=sum
//t=(4*k)!/(k!^4*396^(4*k))
//u=1103+26390*k
k=1;
do{
if(t[-1]<-1){
t[-4]=y[-4]+t[-1]+1;
if(t[-3]>t[-4]) t[-3]=t[-4];
}
if(u1>0){
u2=u1;
u1+=26390;
if(u1<u2){
SETX(v,u2);
PLUSX(u,v,c);
u1=0;
}
}else{
PLUSX(v,u,c);
w=u; u=v; v=w;
}
i=4*k;
#ifdef ARIT64
if(k<416129) MULTI1(t,4*(i-1)*(i-2)*(i-3));
else if(k<=0x100000000/4){ MULTI1(t,4*(i-1)); MULTI1(t,(i-2)*(i-3)); }
else{ MULTI1(t,4); for(i--; i>4*k-4; i--) MULTI1(t,i); }

if(k<2642246) DIVI1(t,k*k*k);
else if(k<0x100000000){ DIVI1(t,k); DIVI1(t,k*k); }
else{ DIVI1(t,k); DIVI1(t,k); DIVI1(t,k); }

DIVI1(t,24591257856);
#else
if(k<257) MULTI1(t,4*(i-1)*(i-2)*(i-3));
else if(k<=0x10000/4){ MULTI1(t,4*(i-1)); MULTI1(t,(i-2)*(i-3)); }
else{ MULTI1(t,4); for(i--; i>4*k-4; i--) MULTI1(t,i); }

if(k<1626) DIVI1(t,k*k*k);
else if(k<0x10000){ DIVI1(t,k); DIVI1(t,k*k); }
else{ DIVI1(t,k); DIVI1(t,k); DIVI1(t,k); }

DIVI1(t,396); DIVI1(t,62099136);
#endif
if(u1>0) MULTI(v,t,u1);
else MULTX1(v,t,u);
PLUSX(r,s,v);
w=r; r=s; s=w;
k++;
}while((-v[-1]<=y[-4]) && !isZero(v) && !error);
MULTI1(s,2);
SETX(c,2);
SQRTX(r,c);
MULTX(v,r,s);
SETX(c,9801);
DIVX(y,c,v);
FREEX(a);
}
*/

//Brent–Salamin algorithm (1975)
int _stdcall PI(Pint a0)
{
	Pint a, b, z, t, y, m, w;
	Tint n;

	m=ALLOCN(5, a0[-4], &a, &b, &z, &t, &y);
	// n=0; a=1; b=sqrt(1/2); z=1/4
	n=0;
	ONEX(a);
	SQRTX(b, half);
	DIVI(z, half, 2);

	do{
		// y=(a+b)/2
		PLUSX(t, a, b);
		DIVI(y, t, 2);
		// b=sqrt(b*a)
		MULTX(t, b, a);
		SQRTX(b, t);
		// a=y
		w=a; a=y; y=w;
		// t=(a-y)^2*(2^n)
		MINUSX(t, a, y);
		SQRX(y, t);
		LSHI(t, y, n);
		n++;
		// z-=t
		MINUSX(y, z, t);
		w=z; z=y; y=w;
	} while(-t[-1]<=a0[-4] && !isZero(t) && !error);

	// a0= (a+b)^2/(4*z)
	PLUSX(y, a, b);
	SQRX(t, y);
	MULTI1(z, 4);
	DIVX(a0, t, z);
	FREEX(m);
	return (int)n;
}

void getpi(Tint len)
{
	if(len>Npi){
		len++;
		FREEX(pi);
		FREEX(pi2);
		FREEX(pi4);
		pi4= ALLOCX(len);
		pi2= ALLOCX(len);
		pi= ALLOCX(len);
		PI(pi);
		DIVI(pi2, pi, 2);
		DIVI(pi4, pi, 4);
		Npi= error ? 0 : len;
	}
}

void _stdcall PIX(Pint y)
{
	getpi(y[-4]);
	COPYX(y, pi);
}
