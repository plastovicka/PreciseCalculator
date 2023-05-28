/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */

#ifndef ARITCH
#define ARITCH

#include "arit.h"

/*
 result must be at different memory locations than input parameters
 */
extern "C"{
	Complex _fastcall ALLOCC(Tint len);
	void _fastcall FREEC(Complex x);
	Complex _fastcall NEWCOPYC(const Complex a);
	void _stdcall COPYC(Complex dest, const Complex src);

	char* _stdcall READC(Complex x, const char *buf);
	void _stdcall WRITEC(char *buf, const Complex x, int digits);
	int _stdcall LENC(const Complex x, int digits);

	void _fastcall SETCN(Complex x, Tint n);
	void _fastcall SETC(Complex x, Tuint n);
	void _fastcall ZEROC(Complex x);
	void _fastcall ONEC(Complex x);

	void _fastcall NEGC(Complex x);
	void _fastcall CONJGC(Complex x);
	void _fastcall REALC(Complex x);
	void _fastcall IMAGC(Complex x);
	void _fastcall ABSC(Complex x);
	void _fastcall SIGNC(Complex x);

	void _fastcall ROUNDC(Complex x);
	void _fastcall TRUNCC(Complex x);
	void _fastcall INTC(Complex x);
	void _fastcall CEILC(Complex x);
	void _fastcall FRACC(Complex x);
	void _fastcall FRACTOC(Complex x);

	void _stdcall ISUFFIXC(Complex y, const Complex x);

	void _stdcall ARGC(Complex y, const Complex x);
	void _stdcall EXPC(Complex y, const Complex x);
	void _stdcall LNC(Complex y, const Complex x);
	void _stdcall LOGNC(Complex y, const Complex n, const Complex x);
	void _stdcall LOG10C(Complex y, const Complex x);
	void _stdcall POWC(Complex y, const Complex a, const Complex b);
	void _stdcall POWCI(Complex y, const Complex x, __int64 n);
	void _stdcall ROOTC(Complex y, const Complex b, const Complex a);
	void _stdcall SQRTC(Complex y, const Complex x);

	int  _stdcall CMPC(const Complex a, const Complex b);
	void _stdcall PLUSC(Complex y, const Complex a, const Complex b);
	void _stdcall MINUSC(Complex y, const Complex a, const Complex b);
	void _stdcall MULTC(Complex y, const Complex a, const Complex b);
	void _stdcall MULTIC(Complex y, const Complex x, Tuint n);
	void _stdcall MULTI1C(Complex x, Tuint n);
	void _stdcall DIVC(Complex y, const Complex a, const Complex b);
	void _stdcall DIVIC(Complex y, const Complex x, Tuint n);
	void _stdcall DIVI1C(Complex x, Tuint n);
	void _stdcall INVERTC(Complex y, const Complex x);
	void _stdcall SQRC(Complex y, const Complex x);

	void _stdcall POLARC(Complex y, const Complex a, const Complex b);
	void _stdcall COMPLEXC(Complex y, const Complex a, const Complex b);

	void _stdcall SINC(Complex y, const Complex x);
	void _stdcall COSC(Complex y, const Complex x);
	void _stdcall TANC(Complex y, const Complex x);
	void _stdcall SECC(Complex y, const Complex x);
	void _stdcall CSCC(Complex y, const Complex x);
	void _stdcall COTGC(Complex y, const Complex x);
	void _stdcall ASINC(Complex y, const Complex x);
	void _stdcall ACOSC(Complex y, const Complex x);
	void _stdcall ATANC(Complex y, const Complex x);
	void _stdcall ASECC(Complex y, const Complex x);
	void _stdcall ACSCC(Complex y, const Complex x);
	void _stdcall ACOTGC(Complex y, const Complex x);

	void _stdcall SINHC(Complex y, const Complex x);
	void _stdcall COSHC(Complex y, const Complex x);
	void _stdcall TANHC(Complex y, const Complex x);
	void _stdcall SECHC(Complex y, const Complex x);
	void _stdcall CSCHC(Complex y, const Complex x);
	void _stdcall COTGHC(Complex y, const Complex x);
	void _stdcall ASINHC(Complex y, const Complex x);
	void _stdcall ACOSHC(Complex y, const Complex x);
	void _stdcall ATANHC(Complex y, const Complex x);
	void _stdcall ASECHC(Complex y, const Complex x);
	void _stdcall ACSCHC(Complex y, const Complex x);
	void _stdcall ACOTGHC(Complex y, const Complex x);

	void _stdcall EQUALC(Complex y, const Complex a, const Complex b);
	void _stdcall NOTEQUALC(Complex y, const Complex a, const Complex b);

	void _stdcall NOTC(Complex y, const Complex x);
	void _stdcall ANDC(Complex y, const Complex a, const Complex b);
	void _stdcall ORC(Complex y, const Complex a, const Complex b);
	void _stdcall XORC(Complex y, const Complex a, const Complex b);
	void _stdcall NANDBC(Complex y, const Complex a, const Complex b);
	void _stdcall NORBC(Complex y, const Complex a, const Complex b);
	void _stdcall IMPBC(Complex y, const Complex a, const Complex b);
	void _stdcall EQVBC(Complex y, const Complex a, const Complex b);
	void _stdcall RSHC(Complex y, const Complex a, const Complex b);
	void _stdcall RSHIC(Complex y, const Complex a, const Complex b);
	void _stdcall LSHC(Complex y, const Complex a, const Complex b);
	void _stdcall LSHIC(Complex y, const Complex a, Tint n);
}

inline bool isZero(const Complex x){
	return x.r[-3]==0 && x.i[-3]==0;
}

inline bool isImag(const Complex x){
	return x.i[-3]!=0;
}

inline bool isVariable(const Complex x){
	return x.r[-3]==-1;
}

inline bool isRange(const Complex x){
	return x.r[-3]==-5;
}

inline bool isInt(const Complex x){
	return isInt(x.r) && x.i[-3]==0;
}

void assignC(Complex &dest, const Complex &src);

typedef void(_fastcall *TunaryC0)(Complex);
typedef void(_stdcall *TunaryC2)(Complex, const Complex);
typedef void(_stdcall *TbinaryCI0)(Complex, Tuint i);
typedef void(_stdcall *TbinaryCI2)(Complex, const Complex, Tuint i);
typedef void(_stdcall *TbinaryC)(Complex, const Complex, const Complex);
typedef void(_stdcall *TnularyC)(Complex);
typedef void(_stdcall *TvarargC)(Complex, unsigned, const Complex*);
typedef void(_stdcall *TarrayargC)(Complex, const Complex*);

extern char imagChar;

#endif
