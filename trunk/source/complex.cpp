/*
  (C) 2005-2011  Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
*/
#include "hdr.h"
#include "complex.h"
//-------------------------------------------------------------------

char imagChar='i';
extern Numx Kone,Kzero,Kminusone;
Complex onec={&Kone.m,&Kzero.m},minusonec={&Kminusone.m,&Kzero.m};

void errImag()
{
  cerror(1035,"Function does not support imaginary numbers");
}

void _stdcall WRITEC(char *buf, const Complex x, int digits)
{
  WRITEX(buf,x.r,digits);
  if(isZero(x.i)) return;
  buf=strchr(buf,0);
  *buf++=' ';
  if(x.i[-2]==0) *buf++='+';

  if(enableFractions && isFraction(x.i) && x.i[1]!=1){
    if(x.i[-2]!=0) *buf++='-';
    *buf++='(';
    Pint t = ALLOCX(x.i[-4]);
    COPYX(t,x.i);
    ABSX(t);
    WRITEX(buf,t,digits);
    FREEX(t);
    buf=strchr(buf,0);
    *buf++=')';
  }else{
    WRITEX(buf,x.i,digits);
    buf=strchr(buf,0);
    *buf++=' ';
  }
  *buf++= imagChar;
  *buf=0;
}

/*
char* _stdcall READC(Complex x, const char *buf)
{
  char *s= READX(x.r,buf);
  while(*s==' ') s++;
  if(*s=='i'){
    char c=(char)tolower(s[1]);
    if(c<'a' || c>'z'){
      s++;
      COPYX(x.i,x.r);
      ZEROX(x.r);
    }
  }
  return s;
} 
*/       

int _stdcall LENC(const Complex x, int digits)
{
  return LENX(x.r,digits)+LENX(x.i,digits)+5;
}

Complex _fastcall ALLOCC(Tint len)
{
  Complex y;
  y.r=ALLOCX(len);
  y.i=ALLOCX(len);
  return y;
}

Complex _fastcall NEWCOPYC(const Complex a)
{
  Complex y;
  y.r=NEWCOPYX(a.r);
  y.i=NEWCOPYX(a.i);
  return y;
}

void assignC(Complex &dest, const Complex &src)
{
  assign(dest.r, src.r);
  assign(dest.i, src.i);
}

//-------------------------------------------------------------------

void _fastcall MAPC(Complex &x, Tunary0 f)
{
  f(x.r);
  f(x.i);
}

void _fastcall MAPC(Complex &y, const Complex &x, Tunary2 f)
{
  f(y.r, x.r);
  f(y.i, x.i);
}

void _fastcall MAPC(Complex &y, const Complex &a, const Complex &b, Tbinary f)
{
  f(y.r, a.r, b.r);
  f(y.i, a.i, b.i);
}

void _fastcall FREEC(Complex x)
{
  MAPC(x,FREEX);
}
void _stdcall COPYC(Complex dest, const Complex src)
{
  MAPC(dest,src,COPYX);
}
void _fastcall ROUNDC(Complex x)
{
  MAPC(x,ROUNDX);
}
void _fastcall TRUNCC(Complex x)
{
  MAPC(x,TRUNCX);
}
void _fastcall INTC(Complex x)
{
  MAPC(x,INTX);
}
void _fastcall CEILC(Complex x)
{
  MAPC(x,CEILX);
}
void _fastcall FRACC(Complex x)
{
  MAPC(x,FRACX);
}
void _fastcall FRACTOC(Complex x)
{
  MAPC(x,FRACTOX);
}
void _stdcall NOTC(Complex y,const Complex x)
{ 
  MAPC(y,x,NOTX); 
}
void _fastcall NEGC(Complex x)
{ 
  MAPC(x,NEGX); 
}
void _stdcall ANDC(Complex y,const Complex a,const Complex b)
{ 
  MAPC(y,a,b,ANDX); 
}
void _stdcall ORC(Complex y,const Complex a,const Complex b)
{ 
  MAPC(y,a,b,ORX); 
}
void _stdcall XORC(Complex y,const Complex a,const Complex b)
{ 
  MAPC(y,a,b,XORX); 
}
void _stdcall NANDBC(Complex y,const Complex a,const Complex b)
{ 
  MAPC(y,a,b,NANDBX); 
}
void _stdcall NORBC(Complex y,const Complex a,const Complex b)
{ 
  MAPC(y,a,b,NORBX); 
}
void _stdcall IMPBC(Complex y,const Complex a,const Complex b)
{ 
  MAPC(y,a,b,IMPBX); 
}
void _stdcall EQVBC(Complex y,const Complex a,const Complex b)
{ 
  MAPC(y,a,b,EQVBX); 
}
void _fastcall ZEROC(Complex x)
{
  MAPC(x,ZEROX);
}
void _stdcall PLUSC(Complex y,const Complex a,const Complex b)
{
  MAPC(y,a,b,PLUSX);
}
void _stdcall MINUSC(Complex y,const Complex a,const Complex b)
{
  MAPC(y,a,b,MINUSX);
}

//-------------------------------------------------------------------

void _fastcall SETCN(Complex x,Tint n)
{
  SETXN(x.r,n);
  ZEROX(x.i);
}

void _fastcall SETC(Complex x,Tuint n)
{
  SETX(x.r,n);
  ZEROX(x.i);
}

void _fastcall ONEC(Complex x)
{
  ONEX(x.r);
  ZEROX(x.i);
}

void _stdcall RSHC(Complex y,const Complex a,const Complex b)
{ 
  if(isImag(b)){
    cerror(1017,"The shift operand is not integer");
    return;
  }
  RSHX(y.r, a.r, b.r);
  RSHX(y.i, a.i, b.r);
}

void _stdcall RSHIC(Complex y,const Complex a,const Complex b)
{ 
  RSHC(y,a,b);
  TRUNCC(y);
}

void _stdcall LSHC(Complex y,const Complex a,const Complex b)
{ 
  if(isImag(b)){
    cerror(1017,"The shift operand is not integer");
    return;
  }
  LSHX(y.r, a.r, b.r);
  LSHX(y.i, a.i, b.r);
}

void _stdcall LSHIC(Complex y,const Complex a, Tint n)
{ 
  LSHI(y.r, a.r, n);
  LSHI(y.i, a.i, n);
}

void _fastcall CONJGC(Complex x)
{
  NEGX(x.i);
}

void _fastcall REALC(Complex x)
{
  ZEROX(x.i);
}

void _fastcall IMAGC(Complex x)
{
  COPYX(x.r, x.i);
  ZEROX(x.i);
}

void _stdcall ISUFFIXC(Complex y,const Complex x)
{
  COPYX(y.r, x.i);
  COPYX(y.i, x.r);
  NEGX(y.r);
}

//abs(a+bi)=sqrt(a^2+b^2)
void _fastcall ABSC(Complex x)
{
  if(!isImag(x)){
    ABSX(x.r);
  }else{
    if(isZero(x.r)){
      ABSX(x.i);
      COPYX(x.r, x.i);
    }else{
      Pint t,u;
      t=ALLOCX(x.r[-4]);
      u=ALLOCX(x.i[-4]);
      SQRX(t, x.r);
      SQRX(u, x.i);
      PLUSX(x.i, t,u);
      SQRTX(x.r, x.i);
      FREEX(u);
      FREEX(t);
    }
    ZEROX(x.i);
  }
}

//arg(a+bi)=atan2(b,a)
void _stdcall ARGC(Complex y,const Complex x)
{
  ATAN2X(y.r, x.i, x.r);
  ZEROX(y.i);
}

//polar(r,a)=r*cos a + i*r*sin a
void _stdcall POLARC(Complex y,const Complex a,const Complex b)
{
  if(isImag(a) || isImag(b)){
    errImag();
    return;
  }
  Pint t=ALLOCX(y.r[-4]);
  SINX(t, b.r);
  MULTX(y.i, a.r, t);
  COSX(t, b.r);
  MULTX(y.r, a.r, t);
  FREEX(t);
}

//complex(a,b)=a+b i
void _stdcall COMPLEXC(Complex y,const Complex a,const Complex b)
{
  if(isImag(a) || isImag(b)){
    errImag();
    return;
  }
  COPYX(y.r,a.r);
  COPYX(y.i,b.r);
}
//-------------------------------------------------------------------

// (a+bi)*(c+di)= (a*c-b*d) + i*(b*c+a*d)
void _stdcall MULTC(Complex y,const Complex a,const Complex b) 
{
  Pint t,u;
  t=ALLOCX(y.r[-4]);
  u=ALLOCX(y.r[-4]);
  MULTX(t,a.r,b.r);
  MULTX(u,a.i,b.i);
  MINUSX(y.r,t,u);
  MULTX(t,a.r,b.i);
  MULTX(u,a.i,b.r);
  PLUSX(y.i,t,u);
  FREEX(u);
  FREEX(t);
}

void _stdcall SQRC(Complex y,const Complex x)
{
  MULTC(y,x,x);
}

void _stdcall MULTI1C(Complex x,Tuint n)
{
  MULTI1(x.r,n);
  MULTI1(x.i,n);
}

void _stdcall MULTIC(Complex y,const Complex x,Tuint n)
{
  MULTI(y.r, x.r, n);
  MULTI(y.i, x.i, n);
}

void _stdcall DIVI1C(Complex x,Tuint n)
{
  DIVI1(x.r,n);
  DIVI1(x.i,n);
}

void _stdcall DIVIC(Complex y,const Complex x,Tuint n)
{
  DIVI(y.r, x.r, n);
  DIVI(y.i, x.i, n);
}

// (a+bi)/(c+di)=((a*c+b*d)+i*(b*c-a*d))/(c^2+d^2)
void _stdcall DIVC(Complex y,const Complex a,const Complex b)
{
  Pint t,u,v;

  if(!isImag(b)){
    DIVX(y.r, a.r, b.r);
    DIVX(y.i, a.i, b.r);
  }else if(isZero(b.r)){
    DIVX(y.r, a.i, b.i);
    DIVX(y.i, a.r, b.i);
    NEGX(y.i);
  }else{
    t=ALLOCX(y.r[-4]);
    u=ALLOCX(y.r[-4]);
    v=ALLOCX(y.r[-4]);
    SQRX(t,b.r);
    SQRX(u,b.i);
    PLUSX(v,t,u);
    MULTX(t,a.r,b.r);
    MULTX(y.r,a.i,b.i);
    PLUSX(u,t,y.r);
    DIVX(y.r,u,v);
    MULTX(t,a.i,b.r);
    MULTX(y.i,a.r,b.i);
    MINUSX(u,t,y.i);
    DIVX(y.i,u,v);
    FREEX(v);
    FREEX(u);
    FREEX(t);
  }
}

// 1/(a+bi)=a/(a^2+b^2) - i*b/(a^2+b^2)
void _stdcall INVERTC(Complex y,const Complex x)
{
  SQRX(y.r, x.r);
  SQRX(y.i, x.i);
  Pint t=ALLOCX(y.r[-4]);
  PLUSX(t, y.r, y.i);
  DIVX(y.r, x.r, t);
  DIVX(y.i, x.i, t);
  FREEX(t);
  NEGX(y.i);
}


//sign(x)=x/abs(x)
void _fastcall SIGNC(Complex x)
{
  Complex t=ALLOCC(x.r[-4]);
  Complex u=ALLOCC(x.r[-4]);
  COPYC(t,x);
  COPYC(u,x);
  ABSC(u);
  DIVC(x,t,u);
  FREEC(u);
  FREEC(t);
}

//-------------------------------------------------------------------

//sin(a+bi)=sin a*cosh b + i*cos a*sinh b
void _stdcall SINC(Complex y,const Complex x)
{
  Pint t,u;
  t=ALLOCX(y.r[-4]);
  u=ALLOCX(y.r[-4]);
  SINX(t, x.r);
  if(!isZero(t)) COSHX(u, x.i);
  MULTX(y.r, t,u);
  COSX(t, x.r);
  if(!isZero(t)) SINHX(u, x.i);
  MULTX(y.i, t,u);
  FREEX(u);
  FREEX(t);
}

//cos(a+bi)=cos a*cosh b - i*sin a*sinh b
void _stdcall COSC(Complex y,const Complex x)
{
  Pint t,u;
  t=ALLOCX(y.r[-4]);
  u=ALLOCX(y.r[-4]);
  COSX(t, x.r);
  if(!isZero(t)) COSHX(u, x.i);
  MULTX(y.r, t,u);
  SINX(t, x.r);
  if(!isZero(t)) SINHX(u, x.i);
  MULTX(y.i, t,u);
  NEGX(y.i);
  FREEX(u);
  FREEX(t);
}

//sinh(a+bi)=sinh a*cos b + i*cosh a*sin b
void _stdcall SINHC(Complex y,const Complex x)
{
  Pint t,u;
  t=ALLOCX(y.r[-4]);
  u=ALLOCX(y.r[-4]);
  SINHX(t, x.r);
  if(!isZero(t)) COSX(u, x.i);
  MULTX(y.r, t,u);
  COSHX(t, x.r);
  if(!isZero(t)) SINX(u, x.i);
  MULTX(y.i, t,u);
  FREEX(u);
  FREEX(t);
}

//cosh(a+bi)=cosh a*cos b + i*sinh a*sin b
void _stdcall COSHC(Complex y,const Complex x)
{
  Pint t,u;
  t=ALLOCX(y.r[-4]);
  u=ALLOCX(y.r[-4]);
  COSHX(t, x.r);
  if(!isZero(t)) COSX(u, x.i);
  MULTX(y.r, t,u);
  SINHX(t, x.r);
  if(!isZero(t)) SINX(u, x.i);
  MULTX(y.i, t,u);
  FREEX(u);
  FREEX(t);
}

static void _stdcall TANCOTGC(Complex y,const Complex x,bool co)
{
  Pint a,b,t,u;

  a=ALLOCX(x.i[-4]);
  MULTI(a, x.r, 2);
  COSX(y.r,a);
  b=ALLOCX(x.r[-4]);
  MULTI(b, x.i, 2);
  COSHX(y.i, b);
  u=ALLOCX(y.r[-4]);
  if(co) NEGX(y.i);
  PLUSX(u, y.r, y.i);
  t=ALLOCX(y.r[-4]);
  SINX(t,a);
  DIVX(y.r, t,u);
  if(co) NEGX(y.r);
  SINHX(t,b);
  DIVX(y.i, t,u);
  FREEX(t);
  FREEX(u);
  FREEX(b);
  FREEX(a);
}

//tan(a+bi)=(sin(2a)+i*sinh(2b))/(cos(2a)+cosh(2b))
void _stdcall TANC(Complex y,const Complex x)
{
  TANCOTGC(y,x,false);
}

//cotg(a+bi)=(-sin(2a)+i*sinh(2b))/(cos(2a)-cosh(2b))
void _stdcall COTGC(Complex y,const Complex x)  
{
  TANCOTGC(y,x,true);
}

static void _stdcall TANHCOTGHC(Complex y,const Complex x,bool co)
{
  Pint a,b,t,u;

  a=ALLOCX(x.i[-4]);
  MULTI(a, x.r, 2);
  COSHX(y.r,a);
  b=ALLOCX(x.r[-4]);
  MULTI(b, x.i, 2);
  COSX(y.i, b);
  u=ALLOCX(y.r[-4]);
  if(co) NEGX(y.i);
  PLUSX(u, y.r, y.i);
  t=ALLOCX(y.r[-4]);
  SINHX(t,a);
  DIVX(y.r, t,u);
  SINX(t,b);
  DIVX(y.i, t,u);
  if(co) NEGX(y.i);
  FREEX(t);
  FREEX(u);
  FREEX(b);
  FREEX(a);
}

//tanh(a+bi)=(sinh(2a)+i*sin(2b))/(cosh(2a)+cos(2b))
void _stdcall TANHC(Complex y,const Complex x)
{
  TANHCOTGHC(y,x,false);
}

//cotgh(a+bi)=(sinh(2a)-i*sin(2b))/(cosh(2a)-cos(2b))
void _stdcall COTGHC(Complex y,const Complex x)
{
  TANHCOTGHC(y,x,true);
}

//-------------------------------------------------------------------
// f: 0=asin, 1=acos, 2=asinh, 3=acosh
static void _stdcall ASINCOSC(Complex y,const Complex x, int f)
{
  Complex t,u,z;

  t=ALLOCC(y.r[-4]);
  u=t;
  SQRC(t,x);
  if(f<2) MINUSC(y,onec,t);      //1-x^2
  else if(f==2) PLUSC(y,t,onec); //1+x^2
  else MINUSC(y,t,onec);         //x^2-1
  SQRTC(t,y);
  if(f==1){ // *i
    u.r=t.i;
    u.i=t.r;
    NEGX(u.r);
  }
  if(f==3){
    if(!isImag(x) ? CMPX(x.r,minusone)<0 : 
      (isZero(x.r) ? x.i[-2] : x.r[-2])){
      NEGC(t);
    }
  }
  z=x;
  if(f==0){ // i*x
    z=ALLOCC(y.r[-4]);
    COPYX(z.r, x.i);
    COPYX(z.i, x.r);
    NEGX(z.r);
  }
  PLUSC(y,z,u);
  LNC(t,y);
  if(f<2){ // *-i
    COPYX(y.r, t.i);
    COPYX(y.i, t.r);
    NEGX(y.i);
  }else{
    COPYC(y,t);
  }
  if(f==0) FREEC(z);
  FREEC(t);
}

//arcsin(x)= -i*ln(i*x+sqrt(1-x^2))
void _stdcall ASINC(Complex y,const Complex x)
{
  if(!isImag(x) && CMPU(x.r,one)<=0){
    ASINX(y.r,x.r);
    ZEROX(y.i);
    return;
  }
  ASINCOSC(y,x,0);
}

//arccos(x)= -i*ln(x+i*sqrt(1-x^2))
void _stdcall ACOSC(Complex y,const Complex x)
{
  if(!isImag(x) && CMPU(x.r,one)<=0){
    ACOSX(y.r,x.r);
    ZEROX(y.i);
    return;
  }
  ASINCOSC(y,x,1);
}

//argsinh(x)= ln(x+sqrt(x^2+1))
void _stdcall ASINHC(Complex y,const Complex x)
{
  ASINCOSC(y,x,2);
}

//argcosh(x)= ln(x+sqrt(x+1)*sqrt(x-1))
void _stdcall ACOSHC(Complex y,const Complex x) 
{
  if(isZero(x)){
    ZEROX(y.r);
    COPYX(y.i,pi2);
    angleResult(y.i);
  }else if(!isImag(x) && CMPU(x.r,one)<=0){
    ZEROX(y.r);
    ACOSX(y.i,x.r);
  }else{
    ASINCOSC(y,x,3);
  }
}

//-------------------------------------------------------------------
// f: 0=atan, 1=acotg, 2=atanh, 3=acotgh
static void _stdcall ATANCOTC(Complex y,const Complex x, int f)
{
  Complex t,u,z;

  z=x;
  if(f<2){ // *i
    if(isZero(x.r) && !CMPU(x.i,one)){
      cerror(1040,"arctan, arccot from 1i or -1i");
    }
    NEGX(z.r=NEWCOPYX(x.i));
    z.i=x.r;
  }else{
    if(!isImag(x) && !CMPU(x.r,one)){
      cerror(1039,"argtanh,argcoth from 1 or -1");
    }
  }
  t=ALLOCC(y.r[-4]);
  if(f&1) MINUSC(t,z,onec); 
  else MINUSC(t,onec,z);
  u=ALLOCC(y.r[-4]);
  PLUSC(u,onec,z);
  DIVC(y,u,t);
  LNC(t,y);
  if(f<2){
    DIVI(y.r,t.i,2);
    DIVI(y.i,t.r,2);
    if(f==0){
      NEGX(y.i);
    }else{
      if(!isZero(x)) NEGX(y.r);
    }
    if(isZero(x.r) && (f==0 ? CMPX(x.i,minusone)<0 : 
      x.i[-2] && !isZero(x.i))){
      NEGX(y.r);
    }
  }else{
    DIVI(y.r,t.r,2);
    DIVI(y.i,t.i,2);
    if(isZero(x.i) && (f==2 ? CMPX(x.r,one)>0 : 
      CMPX(x.r,one)<0 && !x.r[-2] && !isZero(x.r))){
      NEGX(y.i);
    }
  }
  FREEC(u);
  FREEC(t);
  if(f<2) FREEX(z.r);
}

//arctan(x)=-i/2*ln((1+i*x)/(1-i*x))
//for imag x<-1i, the real part of the result is negated
void _stdcall ATANC(Complex y,const Complex x)
{
  ATANCOTC(y,x,0);
}

//arccotg(x)=i/2*ln((1+i*x)/(i*x-1))
//for imag 0<x<1i, the real part of the result is negated
void _stdcall ACOTGC(Complex y,const Complex x)
{
  ATANCOTC(y,x,1);
}

//argtanh(x)=ln((1+x)/(1-x))/2
//for real x>1, the result is conjugated
void _stdcall ATANHC(Complex y,const Complex x)
{
  if(isZero(x)){
    ZEROC(y);
  }else{
    ATANCOTC(y,x,2);
  }
}

//argcotgh(x)=ln((1+x)/(x-1))/2
//for real 0<x<1, the result is conjugated
void _stdcall ACOTGHC(Complex y,const Complex x)
{
  ATANCOTC(y,x,3);
}

//-------------------------------------------------------------------
void _stdcall SECCSCC(Complex y, const Complex x, TunaryC2 f)
{
  Complex t=ALLOCC(y.r[-4]);
  f(t,x);
  INVERTC(y,t);
  FREEC(t);
}

//sec x=1/cos x
void _stdcall SECC(Complex y,const Complex x)
{
  SECCSCC(y,x,COSC);
}

//csc x=1/sin x
void _stdcall CSCC(Complex y,const Complex x)
{
  SECCSCC(y,x,SINC);
}

//sech x=1/cosh x
void _stdcall SECHC(Complex y,const Complex x)
{
  SECCSCC(y,x,COSHC);
}

//csch x=1/sinh x
void _stdcall CSCHC(Complex y,const Complex x)
{
  SECCSCC(y,x,SINHC);
}

void _stdcall ASECCSCC(Complex y, const Complex x, TunaryC2 f)
{
  Complex t=ALLOCC(y.r[-4]);
  INVERTC(t,x);
  f(y,t);
  FREEC(t);
}

//asec x=acos(1/x)
void _stdcall ASECC(Complex y,const Complex x)
{
  ASECCSCC(y,x,ACOSC);
}

//acsc x=asin(1/x)
void _stdcall ACSCC(Complex y,const Complex x)
{
  ASECCSCC(y,x,ASINC);
}

//asech x=acosh(1/x)
void _stdcall ASECHC(Complex y,const Complex x)
{
  ASECCSCC(y,x,ACOSHC);
}

//acsch x=asinh(1/x)
void _stdcall ACSCHC(Complex y,const Complex x)
{
  ASECCSCC(y,x,ASINHC);
}

//-------------------------------------------------------------------
//exp(a+bi)=exp a*(cos b+i*sin b)
void _stdcall EXPC(Complex y,const Complex x)
{
  Pint t,u;
  t=ALLOCX(y.r[-4]);
  u=ALLOCX(y.r[-4]);
  EXPX(t, x.r);
  COSX(u, x.i);
  MULTX(y.r, t,u);
  SINX(u, x.i);
  MULTX(y.i, t,u);
  FREEX(u);
  FREEX(t);
}

void lnSetArg(Complex y, const Complex &x)
{
  if(x.r[-2]){
    switch(angleMode){
    case ANGLE_RAD:
      getpi(y.r[-4]);
      COPYX(y.i,pi);
      break;
    case ANGLE_DEG:
      SETX(y.i,180);
      break;
    case ANGLE_GRAD:
      SETX(y.i,200);
      break;
    }
  }else{
    ZEROX(y.i);
  }
}

//ln z=ln abs z + i*arg z
void _stdcall LNC(Complex y,const Complex x)     
{
  if(isZero(x)){ cerror(1036,"Logarithm of zero"); return; }
  if(!isImag(x)){
    if(!CMPU(x.r, one)){
      //ln 1 = 0
      ZEROX(y.r);
      lnSetArg(y,x);
      return;
    }
    if(!CMPU(x.r, two)){
      getln2(y.r[-4]);
      COPYX(y.r, ln2);
      lnSetArg(y,x);
      return;
    }
    if(!CMPU(x.r, ten)){
      getln10(y.r[-4]);
      COPYX(y.r, ln10);
      lnSetArg(y,x);
      return;
    }
  }
  else if(isZero(x.r)){
    if(!CMPU(x.i,one)){
      //ln 1i = 90°i
      switch(angleMode){
      case ANGLE_RAD:
        getpi(y.r[-4]);
        COPYX(y.i,pi2);
        break;
      case ANGLE_DEG:
        SETX(y.i,90);
        break;
      case ANGLE_GRAD:
        SETX(y.i,100);
        break;
      }
      y.i[-2]=x.i[-2];
      ZEROX(y.r);
      return;
    }
  }
  Complex t=ALLOCC(y.r[-4]);
  COPYC(t,x);
  ABSC(t);
  LNX(y.r, t.r);
  ARGC(t, x);
  COPYX(y.i,t.r);
  FREEC(t);
}

//log x=ln x/ln 10
void _stdcall LOG10C(Complex y,const Complex x)  
{
  Complex t=ALLOCC(y.r[-4]);
  LNC(t,x);
  getln10(y.r[-4]);
  DIVX(y.r, t.r, ln10);
  DIVX(y.i, t.i, ln10);
  FREEC(t);
}

//logn(n,x)=ln x/ln n
void _stdcall LOGNC(Complex y,const Complex n, const Complex x)  
{
  Complex t,u;
  t=ALLOCC(y.r[-4]);
  LNC(t,x);
  u=ALLOCC(y.r[-4]);
  LNC(u,n);
  DIVC(y,t,u);
  FREEC(u);
  FREEC(t);
}

//sqrt z= sqrt((abs z+real z)/2) + i*sqrt((abs z-real z)/2)*sign imag z
void _stdcall SQRTC(Complex y,const Complex x)   
{
  if(!isImag(x)){
    if(x.r[-2]){
      COPYX(y.r, x.r);
      NEGX(y.r);
      SQRTX(y.i, y.r);
      ZEROX(y.r);
    }else{
      SQRTX(y.r, x.r);
      ZEROX(y.i);
    }
  }else{ 
    if(isZero(x.r)){
      //sqrt(bi)=sqrt(b/2) + i*sqrt(b/2)
      DIVI(y.i, x.i, 2);
      ABSX(y.i);
      SQRTX(y.r, y.i);
      COPYX(y.i, y.r);
    }else{
      COPYC(y,x);
      ABSC(y);
      Pint t=ALLOCX(y.r[-4]);
      MINUSX(t, y.r, x.r);
      DIVI1(t,2);
      SQRTX(y.i, t);
      PLUSX(t, y.r, x.r);
      DIVI1(t,2);
      SQRTX(y.r, t);
      FREEX(t);
    }
    y.i[-2]=x.i[-2];
  }
}

void _stdcall POWC(Complex y,const Complex a,const Complex b)
{
  if(isZero(a)){
    ZEROC(y);
    if(isImag(b) || b.r[-2] || isZero(b.r)){ 
      cerror(1037,"Complex or negative power of zero"); 
      return; 
    }
  }else if((b.r[0]==TintMin && b.r[-3]==1 && b.r[-1]==0  
    || b.r[0]==1 && b.r[1]==2 && b.r[-3]==-2
    ) && b.r[-2]==0 && !isImag(b)){
    SQRTC(y,a);
  }else if(!isImag(a) && !isImag(b) && a.r[-2]==0){
    //real power of positive number
    POWX(y.r, a.r, b.r);
    ZEROX(y.i);
  }else if(!isImag(b) && is32bit(b.r)){
    POWCI(y,a, to32bit(b.r));
  }else{
    Complex t=ALLOCC(y.r[-4]);
    LNC(y,a);
    MULTC(t,y,b);
    EXPC(y,t);
    FREEC(t);
  }
}

void _stdcall ROOTC(Complex y,const Complex b,const Complex a)
{
  if(isZero(a)){
    ZEROC(y);
    if(isImag(b) || b.r[-2] || isZero(b.r)){ 
      cerror(1038,"Complex or negative root of zero"); 
      return; 
    } 
  }else if(equInt(b.r,2) && !isImag(b)){
    SQRTC(y,a);
  }else if(!isImag(a) && !isImag(b) && (a.r[-2]==0 ||
    !isReal(b.r) && isOdd(b.r))){
    ROOTX(y.r, b.r, a.r);
    ZEROX(y.i);
  }else{
    Complex t=ALLOCC(y.r[-4]);
    LNC(y,a);
    DIVC(t,y,b);
    EXPC(y,t);
    FREEC(t);
  }
}

//-------------------------------------------------------------------
void _stdcall POWCI(Complex y, const Complex x, __int64 n)
{
  Complex z,t,u,w;

  if(!isImag(x)){
    POWI(y.r, x.r, n);
    ZEROX(y.i);
    return;
  }
  bool sgn= n<0;
  if(sgn) n=-n;
  t=ALLOCC(y.r[-4]);
  u=ALLOCC(y.r[-4]);
  z=y;
  if(n&1) COPYC(z,x);
  else SETC(z,1);
  COPYC(u,x);
  
  for(n>>=1; n>0; n>>=1){
    SQRC(t,u);
    w=t; t=u; u=w;
    if(n&1){ 
      MULTC(t,z,u);
      w=t; t=z; z=w;
    }
  }
  if(sgn){
    if(z.r==y.r){
      INVERTC(t,z);
      COPYC(y,t);
    }else{
      INVERTC(y,z);
    }
  }else{
    COPYC(y,z);
  }
  if(z.r!=y.r) FREEC(z);
  if(u.r!=y.r) FREEC(u);
  if(t.r!=y.r) FREEC(t);
}
//-------------------------------------------------------------------

void _stdcall EQUALC(Complex y, const Complex a, const Complex b)
{
  SETC(y,CMPX(a.r,b.r)==0 && CMPX(a.i,b.i)==0);
}

void _stdcall NOTEQUALC(Complex y, const Complex a, const Complex b)
{
  SETC(y,CMPX(a.r,b.r)!=0 || CMPX(a.i,b.i)!=0);
}

int  _stdcall CMPC(const Complex a,const Complex b)
{
  int result= CMPX(a.r,b.r);
  if(!result) result= CMPX(a.i,b.i);
  return result;
}

//-------------------------------------------------------------------
