/*
 (C) 2005-2012  Petr Lastovicka
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
*/
#ifndef MATRIXH
#define MATRIXH

#include "complex.h"
              

struct Matrix {
 Tint tag;  //-12
 Complex *A;
 int 
  alen, //allocated length (count of Complex structures)
  len,  //used length
  cols, //number of columns
  rows; //number of rows
};
typedef Matrix *Pmatrix;


extern "C"{
  void _fastcall FREEM(Complex x);  
  void _stdcall COPYM(Complex dest, const Complex src);

  void _stdcall WRITEM(char *buf, const Complex x, int digits, int cr);
  char*_stdcall AWRITEM(const Complex x, int digits, int cr); 
  int _stdcall LENM(const Complex x, int digits);

  void _stdcall CONCATM(Complex y, Complex a, Complex b);
  void _stdcall CONCATROWM(Complex y, Complex a, Complex b);
  void _stdcall INDEXM(Complex cy, const Complex cx, int *D);
  void _fastcall TRANSPM(Complex x);
  void _stdcall TRANSP2M(Complex y, const Complex cx);
  void _stdcall INVERTM(Complex y,const Complex x);
  void _fastcall ELIMM(Complex x);
  void _fastcall EQUSOLVEM(Complex cx);
  void _fastcall DETM(Complex D, Complex cx);
  void _fastcall RANKM(Complex D, Complex cx);
  void _fastcall SETM(Complex x,Tuint n);
  void _fastcall SETDIAGM(Complex x,Tuint n);
  void _fastcall WIDTHM(Complex x);
  void _fastcall HEIGHTM(Complex x);
  void _fastcall COUNTM(Complex x);
  void _fastcall ABSM(Complex x);    
  void _stdcall POLYNOMM(Complex y,const Complex cx,const Complex cp);
  
  void INTEGRALM(Complex y, Complex a, Complex b, Complex p, Complex var, const char *formula);
  void _stdcall SWAPM(Complex y,Complex ca,Complex cb);

  void _stdcall PLUSM(Complex y,const Complex a,const Complex b); 
  void _stdcall MINUSM(Complex y,const Complex a,const Complex b); 
  void _stdcall MULTM(Complex y,const Complex a,const Complex b);
  void _stdcall MULTIM(Complex y,const Complex a,Tuint n);
  void _stdcall MULTI1M(Complex y,Tuint n);
  void _stdcall MULTCM(Complex y,const Complex a,const Complex i);
  void _stdcall DIVM(Complex y,const Complex a,const Complex b); 
  void _stdcall DIVIM(Complex cy,const Complex cx,Tuint n);
  void _stdcall DIVI1M(Complex y,Tuint n); 
  void _stdcall EQUALM(Complex y,const Complex a,const Complex b);
  void _stdcall NOTEQUALM(Complex y,const Complex a,const Complex b);
  void _stdcall POWM(Complex y,const Complex a,const Complex b);
  void _stdcall POWMI(Complex y,const Complex x,__int64 n);
  void _stdcall VERTM(Complex y,const Complex ca,const Complex cb);
  void _stdcall ANGLEM(Complex y,const Complex a,const Complex b);
  void _stdcall MATRIXM(Complex y,const Complex a,const Complex b);

  void _fastcall NEGM(Complex x);
  void _fastcall CONJGM(Complex x);
  void _fastcall REALM(Complex x);
  void _fastcall IMAGM(Complex x);
  void _fastcall ROUNDM(Complex x);
  void _fastcall TRUNCM(Complex x);
  void _fastcall INTM(Complex x);
  void _fastcall CEILM(Complex x);
  void _fastcall FRACM(Complex x);

  void _stdcall NOTM(Complex y,const Complex x);
  void _stdcall ANDM(Complex y,const Complex a,const Complex b);
  void _stdcall ORM(Complex y,const Complex a,const Complex b);
  void _stdcall XORM(Complex y,const Complex a,const Complex b);
  void _stdcall NANDBM(Complex y,const Complex a,const Complex b);
  void _stdcall NORBM(Complex y,const Complex a,const Complex b);
  void _stdcall IMPBM(Complex y,const Complex a,const Complex b);
  void _stdcall EQVBM(Complex y,const Complex a,const Complex b);
  void _stdcall RSHM(Complex y,const Complex a,const Complex b);
  void _stdcall RSHIM(Complex y,const Complex a,const Complex b);
  void _stdcall LSHM(Complex y,const Complex a,const Complex b);
  void _stdcall LSHIM(Complex y,const Complex a,Tint n);
  void _stdcall GCDM(Complex y, const Complex cx);
  void _stdcall LCMM(Complex y, const Complex cx);

  void _stdcall SUM1M(Complex y, const Complex x);
  void _stdcall SUMXM(Complex y, const Complex x);
  void _stdcall SUMYM(Complex y, const Complex x);
  void _stdcall SUM2M(Complex y, const Complex x);
  void _stdcall SUMX2M(Complex y, const Complex x);
  void _stdcall SUMY2M(Complex y, const Complex x);
  int  _stdcall SUMXYM(Complex y, const Complex cx);
  void _stdcall AVE1M(Complex y, const Complex x);
  void _stdcall AVEXM(Complex y, const Complex x);
  void _stdcall AVEYM(Complex y, const Complex x);
  void _stdcall AVE2M(Complex y, const Complex x);
  void _stdcall AVEX2M(Complex y, const Complex x);
  void _stdcall AVEY2M(Complex y, const Complex x);
  void _stdcall VAR0M(Complex y, const Complex x);
  void _stdcall VAR1M(Complex y, const Complex x);
  void _stdcall VARX0M(Complex y, const Complex x);
  void _stdcall VARX1M(Complex y, const Complex x);
  void _stdcall VARY0M(Complex y, const Complex x);
  void _stdcall VARY1M(Complex y, const Complex x);
  void _stdcall STDEV0M(Complex y, const Complex x);
  void _stdcall STDEV1M(Complex y, const Complex x);
  void _stdcall STDEVX0M(Complex y, const Complex x);
  void _stdcall STDEVX1M(Complex y, const Complex x);
  void _stdcall STDEVY0M(Complex y, const Complex x);
  void _stdcall STDEVY1M(Complex y, const Complex x);
  
  void _stdcall LRAM(Complex y, const Complex x);
  void _stdcall LRBM(Complex y, const Complex x);
  void _stdcall LRRM(Complex y, const Complex x);
  void _stdcall LRXM(Complex x, const Complex d, const Complex y);
  void _stdcall LRYM(Complex y, const Complex d, const Complex x);

  void _stdcall MINM(Complex y, const Complex x);
  void _stdcall MAXM(Complex y, const Complex x);
  void _stdcall MEDIANM(Complex y, Complex x);
  void _stdcall MODEM(Complex y, Complex x);
  void _fastcall SORTM(Complex x);
  void _fastcall SORTDM(Complex x);
  void _fastcall REVERSEM(Complex x);
  int _stdcall PRODUCTM(Complex y0, const Complex cx);
  void _stdcall GEOMM(Complex y, const Complex cx);
  void _stdcall HARMONM(Complex y0, const Complex cx);

  void _stdcall MIN3M(Complex y,const Complex a,const Complex b);
  void _stdcall MAX3M(Complex y,const Complex a,const Complex b);
}

Tint getPrecision(Complex cx);
void matrixToComplex(Complex &cx);
void assignRange(Complex &cy,const Complex cx,const int *D);
void incdecRange(Complex &y,Complex &cv,bool inc,const int *D);


inline bool isMatrix(const Complex x){
  return x.r[-3]==-12;
}

#define toMatrix(c) ((Matrix*)((c).r-3))

#endif
