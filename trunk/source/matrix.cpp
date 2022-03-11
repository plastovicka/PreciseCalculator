/*
	(C) 2005-2022  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "matrix.h"
#include "darray.h"

int matrixFormat=1;

//-------------------------------------------------------------------
void errMatrix()
{
	cerror(1043, "The function does not support matrices");
}

void errEmptyMatrix()
{
	cerror(1064, "Matrix is empty");
}

bool isVector(const Complex &cx)
{
	Pmatrix x= toMatrix(cx);
	return isMatrix(cx) && (x->rows==1 || x->cols==1);
}

Tint getPrecision(Complex cx)
{
	if(isMatrix(cx)){
		Pmatrix x=toMatrix(cx);
		if(x->len>0) return x->A[0].r[-4];
	}
	return cx.r[-4];
}

void _fastcall FREE_ARRAYM(Pmatrix x)
{
	for(int i=0; i<x->len; i++){
		FREEC(x->A[i]);
	}
	delete[] x->A;
}

void matrixToComplex(Complex &cx)
{
	if(!isMatrix(cx)) return;
	Pmatrix x=toMatrix(cx);
	FREE_ARRAYM(x);
	x->tag=0;
}

//create a new matrix or resize matrix to a->cols, a->rows
void prepareM(Complex cy, int cols, int rows)
{
	int i, len;
	Tint prec;

	len= cols * rows;
	prec= getPrecision(cy);
	Pmatrix y= toMatrix(cy);
	if(isMatrix(cy)){
		if(y->alen < len){
			Complex *A0=y->A;
			y->A= new Complex[y->alen= len];
			memcpy(y->A, A0, sizeof(Complex)*y->len);
			delete[] A0;
		}
		for(i=y->len; i<len; i++){
			y->A[i]= ALLOCC(prec);
		}
		for(i=len; i<y->len; i++){
			FREEC(y->A[i]);
		}
	}
	else{
		y->tag= -12;
		y->alen= 0;
		y->A= NULL;
	}
	if(!y->alen && len){
		y->A= new Complex[y->alen= len];
		for(i=0; i<len; i++){
			y->A[i]= ALLOCC(prec);
		}
	}
	y->rows= rows;
	y->cols= cols;
	y->len= len;
}

void prepareM(Complex& cy, Pmatrix a)
{
	prepareM(cy, a->cols, a->rows);
}

bool noMatrix(Complex &y, TunaryC0 f)
{
	if(!isMatrix(y)){
		f(y);
		return true;
	}
	return false;
}

bool noMatrix(Complex &y, const Complex &a, TunaryC2 f)
{
	if(!isMatrix(a)){
		matrixToComplex(y);
		f(y, a);
		return true;
	}
	return false;
}

bool noMatrix(Complex &y, const Complex &a, const Complex &b, TbinaryC f)
{
	if(!isMatrix(a) && !isMatrix(b)){
		matrixToComplex(y);
		f(y, a, b);
		return true;
	}
	return false;
}

bool noMatrixOrEmpty(Complex &y, const Complex &a, TunaryC2 f)
{
	if(noMatrix(y, a, f)) return true;
	if(toMatrix(a)->len==0) { errEmptyMatrix(); return true; }
	return false;
}

bool sameSizeM(const Complex ca, const Complex cb)
{
	Pmatrix a=toMatrix(ca), b=toMatrix(cb);
	if(!isMatrix(ca) || !isMatrix(cb) || a->cols!=b->cols ||
		a->rows!=b->rows){
		cerror(1044, "Matrices don't have the same size");
		return false;
	}
	return true;
}

bool isSquareM(const Pmatrix x)
{
	if(x->rows!=x->cols){
		cerror(1048, "Matrix is not square");
		return false;
	}
	return true;
}
//-------------------------------------------------------------------
Complex _fastcall NEWCOPYM(const Complex ca)
{
	if(!isMatrix(ca)) return NEWCOPYC(ca);
	Complex result;
	result.r= ALLOCX(ca.r[-4]);
	result.i= NEWCOPYX(ca.i);
	Pmatrix y= toMatrix(result);
	Pmatrix a= toMatrix(ca);
	y->tag= -12;
	y->rows= a->rows;
	y->cols= a->cols;
	y->len= a->len;
	if(a->len) {
		y->A= new Complex[y->alen= a->alen];
		for(int i=0; i<a->len; i++) {
			y->A[i]= NEWCOPYC(a->A[i]);
		}
	}
	else {
		y->A= NULL;
		y->alen= 0;
	}
	return result;
}

void _fastcall FREEM(Complex cx)
{
	if(cx.r && isMatrix(cx)){
		FREE_ARRAYM(toMatrix(cx));
	}
	FREEX(cx.r);
	FREEX(cx.i);
}

void assignM(Complex &dest, const Complex src)
{
	if(!dest.r || getPrecision(dest)<getPrecision(src)){
		FREEM(dest);
		dest=NEWCOPYM(src);
	}
	else{
		COPYM(dest, src);
	}
}


void _stdcall WRITEM(char *buf, const Complex cx, int digits, int cr)
{
	int i, j;
	Complex *p;

	if(!isMatrix(cx)){
		WRITEC(buf, cx, digits);
		return;
	}
	Pmatrix x= toMatrix(cx);
	*buf++='(';
	p=x->A;
	for(i=0; i<x->rows; i++){
		if(i){ *buf++=' '; *buf++='\\'; *buf++= cr ? '\n' : ' '; }
		for(j=0; j<x->cols; j++){
			if(j){ *buf++=','; *buf++=' '; }
			WRITEC(buf, *p++, digits);
			buf=strchr(buf, 0);
		}
	}
	*buf++=')';
	*buf=0;
}

int _stdcall LENM(const Complex cx, int digits)
{
	int i, n;
	Complex *p;

	if(!isMatrix(cx)){
		return LENC(cx, digits);
	}
	n=2;
	Pmatrix x= toMatrix(cx);
	p=x->A;
	for(i=0; i<x->len; i++){
		n+=2+LENC(*p++, digits);
	}
	return n+x->rows;
}

char*_stdcall AWRITEM(const Complex x, int digits, int cr)
{
	char *buf= new char[LENM(x, digits)];
	WRITEM(buf, x, digits, cr);
	return buf;
}

//-------------------------------------------------------------------
void _fastcall EMPTYM(Complex x)
{
	prepareM(x, 0, 0);
}

void _fastcall SETM(Complex x, Tuint n)
{
	matrixToComplex(x);
	SETC(x, n);
}

void _fastcall WIDTHM(Complex x)
{
	unsigned i;
	if(!isMatrix(x)) i=1;
	else i=toMatrix(x)->cols;
	SETM(x, i);
}

void _fastcall HEIGHTM(Complex x)
{
	unsigned i;
	if(!isMatrix(x)) i=1;
	else i=toMatrix(x)->rows;
	SETM(x, i);
}

void _fastcall COUNTM(Complex x)
{
	unsigned i;
	if(!isMatrix(x)) i=1;
	else i=toMatrix(x)->len;
	SETM(x, i);
}


void _fastcall TRANSPM(Complex cx)
{
	int i, j, rows, cols;
	Complex *A;

	if(!isMatrix(cx)) return;
	Pmatrix x=toMatrix(cx);
	rows= x->cols;
	cols= x->rows;
	if(cols!=1 && rows!=1){
		if(x->len==0) return;
		A= new Complex[x->alen];
		for(i=0; i<rows; i++){
			for(j=0; j<cols; j++){
				A[i*cols+j]= x->A[j*rows+i];
			}
		}
		delete[] x->A;
		x->A=A;
	}
	x->rows= rows;
	x->cols= cols;
}

void _stdcall TRANSP2M(Complex y, const Complex cx)
{
	COPYM(y, cx);
	TRANSPM(y);
}

//warning: a and b are destroyed
void _stdcall CONCATROWM(Complex y, Complex a, Complex b)
{
	int cola, colb, rows;
	Pmatrix ma, mb, my;
	Complex *p;

	ma= isMatrix(a) ? toMatrix(a) : 0;
	mb= isMatrix(b) ? toMatrix(b) : 0;
	if(ma){ cola=ma->cols; rows=ma->rows; }
	else{ cola=1; rows=1; }
	if(mb){ colb=mb->cols; rows+=mb->rows; }
	else{ colb=1; rows++; }
	if(cola!=colb){
		if(cola==0) cola=colb;
		else if(colb!=0) {
			cerror(1041, "Matrices have incorrect size");
			return;
		} 
	}
	my= toMatrix(y);
	if(isMatrix(y)){
		FREE_ARRAYM(my);
	}
	else{
		my->tag=-12;
	}
	my->cols=cola;
	my->rows=rows;
	my->len= cola*rows;
	if(ma && ma->alen>=my->len){
		my->alen= ma->alen;
		my->A= ma->A;
		ma->alen=0;
		ma->A=0;
	}
	else{
		my->A= new Complex[my->alen=2*my->len];
		if(ma) memcpy(my->A, ma->A, ma->len*sizeof(Complex));
		else my->A[0]=NEWCOPYC(a);
	}
	p= my->A + (ma ? ma->len : 1);
	if(mb){
		memcpy(p, mb->A, mb->len*sizeof(Complex));
	}
	else{
		*p= NEWCOPYC(b);
	}
	if(ma) ma->len=ma->rows=ma->cols=0;
	if(mb) mb->len=mb->rows=mb->cols=0;
}

//warning: a and b are destroyed
void _stdcall CONCATM(Complex y, Complex a, Complex b)
{
	TRANSPM(a);
	TRANSPM(b);
	CONCATROWM(y, a, b);
	TRANSPM(y);
}

void indexOutOfRange()
{
	cerror(1051, "Index is out of range");
}

void differentSize()
{
	cerror(1044, "Matrices don't have the same size");
}

bool checkRange(const Complex &cx, int *D)
{
	if(!isMatrix(cx)){
		if(D[0]>=0 && (D[0] || D[1]) || D[2]>=0 && (D[2] || D[3])){
			indexOutOfRange();
			return false;
		}
		return true;
	}
	Pmatrix x= toMatrix(cx);
	if(D[0]<0){
		D[0]=0;
		D[1]=x->rows-1;
	}
	if(D[2]<0){
		D[2]=0;
		D[3]=x->cols-1;
		if(isVector(cx) && x->rows==1){
			D[2]=D[0];
			D[3]=D[1];
			D[0]=D[1]=0;
		}
	}
	if(D[0]>=x->rows || D[1]>=x->rows || D[0]>D[1] ||
		D[2]>=x->cols || D[3]>=x->cols || D[2]>D[3]){
		indexOutOfRange();
		return false;
	}
	return true;
}

void incdecRange(Complex &z, Complex &cy, bool inc, const int *D0)
{
	int D[4];
	D[0]=D0[0]; D[1]=D0[1]; D[2]=D0[2]; D[3]=D0[3];
	if(!checkRange(cy, D)) return;
	if(D[0]!=D[1] || D[2]!=D[3]){
		cerror(1042, "Increment or decrement of a matrix");
		return;
	}
	Complex *py;
	if(!isMatrix(cy)){
		py= &cy;
	}
	else{
		Pmatrix y= toMatrix(cy);
		py = y->A + y->cols*D[0] + D[2];
	}
	if(inc) PLUSX(z.r, py->r, one);
	else MINUSX(z.r, py->r, one);
	COPYX(z.i, py->i);
	assignC(*py, z);
}

void assignRange(Complex &cy, const Complex cx, const int *D0)
{
	int D[4];
	D[0]=D0[0]; D[1]=D0[1]; D[2]=D0[2]; D[3]=D0[3];
	if(!checkRange(cy, D)) return;

	if(!isMatrix(cy)){
		if(!isMatrix(cx)){
			assignC(cy, cx);
		}
		else{
			Pmatrix x= toMatrix(cx);
			if(x->len!=1) differentSize();
			else assignC(cy, x->A[0]);
		}
	}
	else{
		int rows = D[1]-D[0]+1;
		int cols = D[3]-D[2]+1;
		Pmatrix y= toMatrix(cy);
		Complex *py = y->A + y->cols*D[0] + D[2];
		if(!isMatrix(cx)){
			if(rows!=1 || cols!=1) differentSize();
			else assignC(*py, cx);
		}
		else{
			Pmatrix x= toMatrix(cx);
			if(x->rows!=rows || x->cols!=cols) differentSize();
			else{
				Complex *px= x->A;
				for(int r=0; r<rows; r++){
					for(int c=0; c<cols; c++){
						assignC(*py++, *px++);
					}
					py+= y->cols - cols;
				}
			}
		}
	}
}

void _stdcall INDEXM(Complex cy, const Complex cx, int *D)
{
	int i, j;

	if(!isMatrix(cx)){
		if(D[0]>=0 && (D[0] || D[1]) || D[2]>=0 && (D[2] || D[3])){
			indexOutOfRange();
		}
		else{
			COPYM(cy, cx);
		}
		return;
	}
	if(!checkRange(cx, D)) return;
	if(!isMatrix(cx)){ COPYM(cy, cx); return; }
	Pmatrix x= toMatrix(cx);
	int cols=D[3]-D[2]+1;
	int rows=D[1]-D[0]+1;
	if(cols==1 && rows==1){
		COPYM(cy, x->A[x->cols*D[0] + D[2]]);
	}
	else{
		prepareM(cy, cols, rows);
		Complex *A= toMatrix(cy)->A;
		for(i=D[0]; i<=D[1]; i++){
			Complex *B= x->A + x->cols*i + D[2];
			for(j=0; j<cols; j++){
				COPYC(*A++, *B++);
			}
		}
	}
}

void _stdcall EQUALM(Complex cy, const Complex ca, const Complex cb)
{
	if(noMatrix(cy, ca, cb, EQUALC)) return;
	if(!sameSizeM(ca, cb)) return;
	matrixToComplex(cy);
	Pmatrix a=toMatrix(ca), b=toMatrix(cb);
	for(int i=0; i<a->len; i++){
		Complex &a1= a->A[i], &b1= b->A[i];
		if(CMPX(a1.r, b1.r) || CMPX(a1.i, b1.i)){
			ZEROC(cy);
			return;
		}
	}
	ONEC(cy);
}

void _stdcall NOTEQUALM(Complex cy, const Complex ca, const Complex cb)
{
	EQUALM(cy, ca, cb);
	SETC(cy, isZero(cy));
}


static void plusminusM(Complex *C, Complex *A, Complex *B, int rows, int cols, int zeroRow, int zeroCol, int cd, int ad, int bd, TbinaryC f)
{
	int i, j;

	cd-=cols;
	ad-=cols;
	bd-=cols;
	for(i=zeroRow; i<rows; i++){
		for(j=zeroCol; j<cols; j++){
			f(*C, *A, *B);
			C++; A++; B++;
		}
		if(zeroCol){
			COPYC(*C, *A);
			C++; A++; B++;
		}
		C+=cd;
		A+=ad;
		B+=bd;
	}
	if(zeroRow){
		for(j=0; j<cols; j++){
			COPYC(*C, *A);
			C++; A++;
		}
	}
}

/*
(A11,A12 \ A21,A22)*(B11,B12 \ B21,B22)=(C11,C12 \ C21,C22)

M1=(A12-A22)*(B21+B22)     M[2] 0,0
M2=(A11+A22)*(B11+B22)     M[3] 0,0
M3=(A11-A21)*(B11+B12)     M[3] 1,1
M4=(A11+A12)*B22           M[4] 0,1
M5=A11*(B12-B22)           M[0] 0,1
M6=A22*(B21-B11)           M[1] 1,0
M7=(A21+A22)*B11           M[2] 1,0
C11=M1+M2-M4+M6
C12=M4+M5
C21=M6+M7
C22=M2-M3+M5-M7

T(2*n)=7*T(n)+18*n^2
*/
void _stdcall MULTMRecurse(int rows, int cols, int len, Complex *aA, int ad, Complex *bA, int bd, Complex *cA, int cd)
{
	int i, j, k, rows2, cols2, len2, rows1, cols1, len1, lenM, lenM1;
	Complex t, u, *S, *buf, *M[5], *A[2][2], *B[2][2];
	int oddRows, oddCols, oddLen;
	Tint prec;
	const int MINMULT=10;

	if(rows<MINMULT || cols<MINMULT || len<MINMULT){
		S=cA;
		prec=S->r[-4];
		t=ALLOCC(prec);
		u=ALLOCC(prec);
		for(i=0; i<rows; i++){
			for(j=0; j<cols; j++){
				ZEROC(*S);
				for(k=0; k<len; k++){
					if(error) break;
					MULTC(t, aA[i*ad+k], bA[k*bd+j]);
					PLUSC(u, *S, t);
					COPYC(*S, u);
				}
				S++;
			}
			S+=cd-cols;
		}
		FREEC(u);
		FREEC(t);
		return;
	}
	if(error) return;
	rows2= (rows+1)>>1;
	oddRows= rows&1;
	rows1= rows2-oddRows;
	cols2= (cols+1)>>1;
	oddCols= cols&1;
	cols1= cols2-oddCols;
	len2= (len+1)>>1;
	oddLen= len&1;
	len1= len2-oddLen;
	A[0][0]=aA;
	A[0][1]=aA+len2;
	A[1][0]=aA+rows2*ad;
	A[1][1]=A[1][0]+len2;
	B[0][0]=bA;
	B[0][1]=bA+cols2;
	B[1][0]=bA+len2*bd;
	B[1][1]=B[1][0]+cols2;

	lenM1=max(max(rows2*cols2, rows2*len2), len2*cols2);
	lenM=lenM1*5;
	M[0]= buf= new Complex[lenM];
	for(i=1; i<5; i++){
		M[i]= M[i-1] + lenM1;
	}
	prec=cA->r[-4];
	for(i=0; i<lenM; i++){
		buf[i]=ALLOCC(prec);
	}
	plusminusM(M[0], A[0][1], A[1][1], rows2, len1, oddRows, 0, len1, ad, ad, MINUSC);
	plusminusM(M[1], B[1][0], B[1][1], len1, cols2, 0, oddCols, cols2, bd, bd, PLUSC);
	MULTMRecurse(rows2, cols2, len1, M[0], len1, M[1], cols2, M[2], cols2);
	plusminusM(M[0], A[0][0], A[1][1], rows2, len2, oddRows, oddLen, len2, ad, ad, PLUSC);
	plusminusM(M[1], B[0][0], B[1][1], len2, cols2, oddLen, oddCols, cols2, bd, bd, PLUSC);
	MULTMRecurse(rows2, cols2, len2, M[0], len2, M[1], cols2, M[3], cols2);
	plusminusM(M[0], A[0][0], A[0][1], rows2, len1, 0, 0, len1, ad, ad, PLUSC);
	MULTMRecurse(rows2, cols1, len1, M[0], len1, B[1][1], bd, M[4], cols1);
	plusminusM(M[0], B[1][0], B[0][0], len1, cols2, 0, 0, cols2, bd, bd, MINUSC);
	MULTMRecurse(rows1, cols2, len1, A[1][1], ad, M[0], cols2, M[1], cols2);
	plusminusM(M[0], M[2], M[3], rows2, cols2, 0, 0, cols2, cols2, cols2, PLUSC);
	plusminusM(M[2], M[0], M[4], rows2, cols2, 0, oddCols, cols2, cols2, cols1, MINUSC);
	plusminusM(cA, M[2], M[1], rows2, cols2, oddRows, 0, cd, cols2, cols2, PLUSC);
	plusminusM(M[2], B[0][1], B[1][1], len2, cols1, oddLen, 0, cols1, bd, bd, MINUSC);
	MULTMRecurse(rows2, cols1, len2, A[0][0], ad, M[2], cols1, M[0], cols1);
	plusminusM(cA+cols2, M[4], M[0], rows2, cols1, 0, 0, cd, cols1, cols1, PLUSC);
	plusminusM(M[4], A[1][0], A[1][1], rows1, len2, 0, oddLen, len2, ad, ad, PLUSC);
	MULTMRecurse(rows1, cols2, len2, M[4], len2, B[0][0], bd, M[2], cols2);
	plusminusM(cA+rows2*cd, M[1], M[2], rows1, cols2, 0, 0, cd, cols2, cols2, PLUSC);
	plusminusM(M[1], M[3], M[0], rows1, cols1, 0, 0, cols1, cols2, cols1, PLUSC);
	plusminusM(M[0], M[1], M[2], rows1, cols1, 0, 0, cols1, cols1, cols2, MINUSC);
	plusminusM(M[2], A[0][0], A[1][0], rows1, len2, 0, 0, len2, ad, ad, MINUSC);
	plusminusM(M[1], B[0][0], B[0][1], len2, cols1, 0, 0, cols1, bd, bd, PLUSC);
	MULTMRecurse(rows1, cols1, len2, M[2], len2, M[1], cols1, M[3], cols1);
	plusminusM(cA+rows2*cd+cols2, M[0], M[3], rows1, cols1, 0, 0, cd, cols1, cols1, MINUSC);

	for(i=0; i<lenM; i++){
		FREEC(buf[i]);
	}
	delete[] buf;
}


void _stdcall MULTM(Complex cy, const Complex ca, const Complex cb)
{
	int colsb, rowsa, n;
	Complex *S;

	if(!isMatrix(ca)){
		MULTCM(cy, cb, ca);
		return;
	}
	if(!isMatrix(cb)){
		MULTCM(cy, ca, cb);
		return;
	}
	Pmatrix a=toMatrix(ca), b=toMatrix(cb), y=toMatrix(cy);
	colsb= b->cols;
	rowsa= a->rows;
	if(isVector(ca) && isVector(cb) &&
		(a->cols!=1 || b->rows!=1)){
		//scalar product of vectors
		n=a->len;
		if(n!=b->len){
			cerror(1046, "Vectors have different lengths");
			return;
		}
		matrixToComplex(cy);
		S=&cy;
		colsb=rowsa=1;
	}
	else{
		n= a->cols;
		if(n!=b->rows){
			cerror(1045, "Number of columns of the first matrix is not equal to number of rows of the second matrix");
			return;
		}
		prepareM(cy, colsb, rowsa);
		if(!y->len) return;
		S= y->A;
	}
	MULTMRecurse(rowsa, colsb, n, a->A, a->cols, b->A, colsb, S, colsb);
}

void _fastcall SETDIAGM(Complex cx, Tuint n)
{
	int i;

	if(!isMatrix(cx)){
		SETC(cx, n);
		return;
	}
	Pmatrix x= toMatrix(cx);
	Complex *p= x->A;
	for(i=0; i<x->len; i++){
		ZEROC(*p++);
	}
	p= x->A;
	for(i=min(x->rows, x->cols)-1; i>=0; i--){
		SETX((*p).r, n);
		p+= 1+x->cols;
	}
}

void _stdcall POWMI(Complex y, const Complex cx, __int64 n)
{
	Complex z, t, u, w;

	if(!isMatrix(cx)){
		matrixToComplex(y);
		POWCI(y, cx, n);
		return;
	}
	Pmatrix x= toMatrix(cx);
	if(!isSquareM(x)) return;
	bool sgn= n<0;
	if(sgn) n=-n;
	t=ALLOCC(getPrecision(y));
	u=ALLOCC(getPrecision(y));
	prepareM(t, x);
	prepareM(u, x);
	z=y;
	prepareM(z, x);
	if(n&1) COPYM(z, cx);
	else SETDIAGM(z, 1);
	COPYM(u, cx);

	for(n>>=1; n>0; n>>=1){
		MULTM(t, u, u);
		w=t; t=u; u=w;
		if(n&1){
			MULTM(t, z, u);
			w=t; t=z; z=w;
		}
	}
	if(sgn){
		if(z.r==y.r){
			INVERTM(t, z);
			COPYM(y, t);
		}
		else{
			INVERTM(y, z);
		}
	}
	else{
		COPYM(y, z);
	}
	if(z.r!=y.r) FREEM(z);
	if(u.r!=y.r) FREEM(u);
	if(t.r!=y.r) FREEM(t);
}

void _stdcall POWM(Complex y, const Complex a, const Complex b)
{
	if(noMatrix(y, a, b, POWC)) return;
	if(isMatrix(b) || isImag(b) || !is32bit(b.r)){
		cerror(1047, "Not integer power of matrix");
		return;
	}
	POWMI(y, a, to32bit(b.r));
}

void _fastcall ELIMM(Complex cx)
{
	if(!isMatrix(cx)) return;
	Pmatrix m= toMatrix(cx);

	int n;
	Complex t, y, *p, *u, *v, *w, *z, *zp, *a;
	Tint prec=getPrecision(cx);
	t=ALLOCC(prec);
	y=ALLOCC(prec);
	n= m->cols;
	z= m->A + m->len;
	for(p=a=m->A, zp=p+n; p<zp && p<z; p+=n, zp+=n, a++){ //(p++ is inside cycle)
		//find not zero pivot
		u=p;
		while(isZero(*u)){
			if((u+=n)>=z){ //all zeros => to next column
				a++;
				u=++p;
				if(p==zp) goto lend;
			}
		}
		if(u==p){
			//divide row where is pivot
			for(u++; u<zp; u++){
				DIVC(t, *u, *p);
				COPYC(*u, t);
			}
			ONEC(*p++);
		}
		else{
			//swap both rows and divide row which has pivot
			w=u;
			ONEC(*p++);
			for(v=p, u++; v<zp; v++, u++){
				DIVC(t, *u, *w);
				COPYC(*u, *v);
				COPYC(*v, t);
			}
			ZEROC(*w);
		}
		//zero column where is pivot
		for(w=a; w<z; w+=n){
			if(w+1!=p && !isZero(*w)){ //except pivot
				for(u=w+1, v=p; v<zp; u++, v++){
					MULTC(t, *v, *w);
					MINUSC(y, *u, t);
					COPYC(*u, y);
				}
				ZEROC(*w);
			}
		}
	}
lend:
	FREEC(y);
	FREEC(t);
}

void _fastcall DETRANKM(Complex D, Complex cx, int det)
{
	if(noMatrix(D, cx, COPYC)){
		if(!det && !isZero(cx)) ONEC(D);
		return;
	}
	Pmatrix m= toMatrix(cx);
	if(det && !isSquareM(m)) return;
	SETM(D, 1);

	int n, result;
	Complex t, y, *p, *u, *v, *w, *z, *zp;
	Tint prec=getPrecision(cx);
	t=ALLOCC(prec);
	y=ALLOCC(prec);
	n= m->cols;
	z= m->A + m->len;
	for(result=0, p=m->A, zp=p+n; p<zp && p<z; result++, p+=n, zp+=n){
		//find not zero pivot
		u=p;
		while(isZero(*u)){
			if((u+=n)>=z){
				if(det){
					ZEROC(D); //matrix is not regular
					goto lend;
				}
				u=++p;
				if(p==zp) goto lend;
			}
		}
		if(u!=p){
			//swap rows
			if(det) NEGC(D);
			for(v=p; v<zp; v++, u++){
				COPYC(t, *u);
				COPYC(*u, *v);
				COPYC(*v, t);
			}
		}
		//divide row where is pivot
		if(det){
			MULTC(t, D, *p);
			COPYC(D, t);
		}
		for(v=p+1; v<zp; v++){
			DIVC(t, *v, *p);
			COPYC(*v, t);
		}
		//zero column where is pivot
		for(w=(p++)+n; w<z; w+=n){
			for(u=w+1, v=p; v<zp; u++, v++){
				MULTC(t, *v, *w);
				MINUSC(y, *u, t);
				COPYC(*u, y);
			}
		}
	}
lend:
	FREEC(y);
	FREEC(t);
	if(!det) SETC(D, result);
}

void _fastcall DETM(Complex D, Complex cx)
{
	DETRANKM(D, cx, 1);
}

void _fastcall RANKM(Complex D, Complex cx)
{
	DETRANKM(D, cx, 0);
}

void _stdcall INVERTM(Complex y, Complex cx)
{
	if(noMatrix(y, cx, INVERTC)) return;
	Pmatrix x= toMatrix(cx);
	if(!isSquareM(x)) return;
	int n= x->len;
	TRANSPM(cx);
	Complex t= ALLOCC(getPrecision(y));
	prepareM(t, x);
	SETDIAGM(t, 1);
	CONCATROWM(y, cx, t);
	TRANSPM(y);
	ELIMM(y);
	TRANSPM(y);
	x= toMatrix(y);
	if(!x->len || isZero(x->A[n-1])){
		cerror(1050, "Matrix is not regular");
	}
	x->rows -= x->cols;
	x->len -= n;
	for(int i=0; i<n; i++){
		FREEC(x->A[i]);
	}
	memmove(x->A, x->A + n, x->len*sizeof(Complex));
	TRANSPM(y);
	FREEM(t);
}

void check1x1(Complex cx)
{
	Pmatrix x= toMatrix(cx);
	if(isMatrix(cx) && x->rows==1 && x->cols==1){
		Complex w= x->A[0];
		x->len=0;
		COPYM(cx, w);
		FREEC(w);
	}
}

void _fastcall EQUSOLVEM(Complex cx)
{
	int r, c, n, i;
	Complex *p, *d, *A;

	Pmatrix x= toMatrix(cx);
	if(!isMatrix(cx) || x->cols<2){
		cerror(1054, "Matrix must have more than one column");
		return;
	}
	ELIMM(cx);
	n=x->cols;
	A=x->A;
	for(r=c=0; c<n-1; c++){
		d= &A[c];
		p= d + r*n;
		if(r>=x->rows || isZero(*p)){
			ZEROC(*d);
		}
		else{
			r++;
			COPYC(*d, A[r*n-1]);
		}
	}
	for(; r<x->rows; r++){
		if(!isZero(A[(r+1)*n-1])){
			cerror(1055, "Equations are not solvable");
		}
	}
	n--;
	x->rows= 1;
	x->cols= n;
	for(i=n; i<x->len; i++){
		FREEC(A[i]);
	}
	x->len= n;
	check1x1(cx);
}

void _stdcall DIVM(Complex y, const Complex a, const Complex b)
{
	if(noMatrix(y, a, b, DIVC)) return;
	Complex t= ALLOCC(getPrecision(y));
	INVERTM(t, b);
	MULTM(y, a, t);
	FREEM(t);
}


void _fastcall ABSM(Complex x)
{
	if(noMatrix(x, ABSC)) return;
	Complex y= ALLOCC(getPrecision(x));
	SUM2M(y, x);
	matrixToComplex(x);
	SQRTC(x, y);
	FREEC(y);
}

void _stdcall MATRIXM(Complex y, const Complex ca, const Complex cb)
{
	if(isInt(ca) && isInt(cb)){
		Tint r = toInt(ca.r);
		Tint c = toInt(cb.r);
		if(r>0 && c>0 && r<100000 && c<100000 || r==0 && c==0){
			if(Int32x32To64(r, c)<=100000){
				prepareM(y, (int)c, (int)r);
				return;
			}
		}
	}
	cerror(1059, "Function matrix must have parameters number of rows, number of columns");
}


void _stdcall ANGLEM(Complex y, const Complex ca, const Complex cb)
{
	matrixToComplex(y);
	if(!isMatrix(ca) && !isMatrix(cb)){
		if(isImag(ca) || isImag(cb)) errImag();
		ATAN2X(y.r, ca.r, cb.r);
		ZEROX(y.i);
		return;
	}
	Pmatrix a=toMatrix(ca), b=toMatrix(cb);
	if(!isVector(ca) || !isVector(cb)){
		errMatrix();
		return;
	}
	if(a->len!=b->len){
		cerror(1046, "Vectors have different lengths");
		return;
	}
	Complex t, u;
	Tint prec= getPrecision(y);
	t=ALLOCC(prec);
	u=ALLOCC(prec);
	SUM2M(t, ca);
	SUM2M(u, cb);
	MULTC(y, t, u);
	SQRTC(t, y);
	MULTM(y, ca, cb);
	DIVC(u, y, t);
	ACOSC(y, u);
	FREEC(u);
	FREEC(t);
}

//vector product
void _stdcall VERTM(Complex y, const Complex ca, const Complex cb)
{
	Pmatrix a=toMatrix(ca), b=toMatrix(cb);
	if(!isVector(ca) || !isVector(cb) || a->len!=3 || b->len!=3){
		cerror(1049, "Arguments have to be vectors of length 3");
		return;
	}
	prepareM(y, a);
	Complex *Y, *A, *B, t, u;
	Y= toMatrix(y)->A;
	A= a->A;
	B= b->A;
	t=ALLOCC(getPrecision(y));
	u=ALLOCC(getPrecision(y));
	int i, j, k;
	for(k=0; k<3; k++){
		i=(k+1)%3;
		j=(k+2)%3;
		MULTC(t, A[i], B[j]);
		MULTC(u, A[j], B[i]);
		MINUSC(Y[k], t, u);
	}
	FREEC(u);
	FREEC(t);
}


void _stdcall POLYNOMM(Complex y, const Complex cx, const Complex cp)
{
	int i, j, n=1;
	Complex t, *m, *d, *s;

	if(!isVector(cp)){
		cerror(1056, "Parameter is not a vector");
		return;
	}
	Pmatrix x= toMatrix(cx);
	if(isMatrix(cx)){
		if(!isSquareM(x)) return;
		n=x->rows;
		if(n==0) { errEmptyMatrix(); return; }
	}
	Pmatrix p= toMatrix(cp);
	SETM(y, 0);
	Tint prec=getPrecision(y);
	t=ALLOCC(prec);
	for(m=&p->A[p->len-1]; m>=p->A; m--){
		MULTM(t, y, cx);
		if(n>1){
			s= toMatrix(t)->A;
			prepareM(y, x);
			d= toMatrix(y)->A;
			for(i=0; i<n; i++){
				for(j=0; j<n; j++){
					if(i==j){
						PLUSC(*d, *s, *m);
					}
					else{
						COPYC(*d, *s);
					}
					s++;
					d++;
				}
			}
		}
		else{
			PLUSC(y, t, *m);
		}
	}
	FREEM(t);
}
//-------------------------------------------------------------------

void _fastcall MAPM(Complex &cx, TunaryC0 f)
{
	if(noMatrix(cx, f)) return;
	Pmatrix x= toMatrix(cx);
	for(int i=0; i<x->len; i++){
		f(x->A[i]);
	}
}

void _fastcall MAPM(Complex &cy, const Complex &cx, TunaryC2 f)
{
	if(noMatrix(cy, cx, f)) return;
	Pmatrix x= toMatrix(cx), y=toMatrix(cy);
	prepareM(cy, x);
	for(int i=0; i<x->len; i++){
		f(y->A[i], x->A[i]);
	}
}

void _stdcall MAPM(Complex cx, Tuint n, TbinaryCI0 f)
{
	if(!isMatrix(cx)){
		f(cx, n);
	}
	else{
		Pmatrix x= toMatrix(cx);
		for(int i=0; i<x->len; i++){
			f(x->A[i], n);
		}
	}
}

void _stdcall MAPM(Complex cy, const Complex cx, Tuint n, TbinaryCI2 f)
{
	if(!isMatrix(cx)){
		matrixToComplex(cy);
		f(cy, cx, n);
	}
	else{
		Pmatrix x= toMatrix(cx);
		prepareM(cy, x);
		Complex *y= toMatrix(cy)->A;
		for(int i=0; i<x->len; i++){
			f(y[i], x->A[i], n);
		}
	}
}

void _stdcall MAP1M(Complex &cy, const Complex &ca, const Complex &b, TbinaryC f)
{
	if(noMatrix(cy, ca, b, f)) return;
	if(isMatrix(b)){ errMatrix(); return; }
	Pmatrix a=toMatrix(ca), y=toMatrix(cy);
	prepareM(cy, a);
	for(int i=0; i<y->len; i++){
		f(y->A[i], a->A[i], b);
	}
}

void _stdcall MAPM(Complex &cy, const Complex &ca, const Complex &cb, TbinaryC f)
{
	if(noMatrix(cy, ca, cb, f)) return;
	if(!sameSizeM(ca, cb)) return;
	Pmatrix a=toMatrix(ca), b=toMatrix(cb), y=toMatrix(cy);
	prepareM(cy, a);
	for(int i=0; i<y->len; i++){
		f(y->A[i], a->A[i], b->A[i]);
	}
}

void _stdcall COPYM(Complex dest, const Complex src)
{
	if(src.r==dest.r) return;
	MAPM(dest, src, COPYC);
}
void _stdcall PLUSM(Complex y, const Complex a, const Complex b)
{
	MAPM(y, a, b, PLUSC);
}
void _stdcall MINUSM(Complex y, const Complex a, const Complex b)
{
	MAPM(y, a, b, MINUSC);
}
void _stdcall MULTCM(Complex y, const Complex a, const Complex i)
{
	MAP1M(y, a, i, MULTC);
}
void _stdcall MULTIM(Complex y, const Complex x, Tuint n)
{
	MAPM(y, x, n, MULTIC);
}
void _stdcall DIVIM(Complex y, const Complex x, Tuint n)
{
	MAPM(y, x, n, DIVIC);
}
void _stdcall MULTI1M(Complex x, Tuint n)
{
	MAPM(x, n, MULTI1C);
}
void _stdcall DIVI1M(Complex x, Tuint n)
{
	MAPM(x, n, DIVI1C);
}
void _stdcall LSHIM(Complex y, const Complex a, int n)
{
	MAPM(y, a, n, (TbinaryCI2)LSHIC);
}

void _fastcall NEGM(Complex x)
{
	MAPM(x, NEGC);
}
void _fastcall CONJGM(Complex x)
{
	MAPM(x, CONJGC);
}
void _fastcall REALM(Complex x)
{
	MAPM(x, REALC);
}
void _fastcall IMAGM(Complex x)
{
	MAPM(x, IMAGC);
}
void _fastcall ROUNDM(Complex x)
{
	MAPM(x, ROUNDC);
}
void _fastcall TRUNCM(Complex x)
{
	MAPM(x, TRUNCC);
}
void _fastcall INTM(Complex x)
{
	MAPM(x, INTC);
}
void _fastcall CEILM(Complex x)
{
	MAPM(x, CEILC);
}
void _fastcall FRACM(Complex x)
{
	MAPM(x, FRACC);
}
void _stdcall NOTM(Complex y, const Complex x)
{
	MAPM(y, x, NOTC);
}
void _stdcall ANDM(Complex y, const Complex a, const Complex b)
{
	MAPM(y, a, b, ANDC);
}
void _stdcall ORM(Complex y, const Complex a, const Complex b)
{
	MAPM(y, a, b, ORC);
}
void _stdcall NANDBM(Complex y, const Complex a, const Complex b)
{
	MAPM(y, a, b, NANDBC);
}
void _stdcall NORBM(Complex y, const Complex a, const Complex b)
{
	MAPM(y, a, b, NORBC);
}
void _stdcall XORM(Complex y, const Complex a, const Complex b)
{
	MAPM(y, a, b, XORC);
}
void _stdcall IMPBM(Complex y, const Complex a, const Complex b)
{
	MAPM(y, a, b, IMPBC);
}
void _stdcall EQVBM(Complex y, const Complex a, const Complex b)
{
	MAPM(y, a, b, EQVBC);
}
void _stdcall RSHM(Complex y, const Complex a, const Complex b)
{
	MAP1M(y, a, b, RSHC);
}
void _stdcall RSHIM(Complex y, const Complex a, const Complex b)
{
	MAP1M(y, a, b, RSHIC);
}
void _stdcall LSHM(Complex y, const Complex a, const Complex b)
{
	MAP1M(y, a, b, LSHC);
}
//-------------------------------------------------------------------

void _stdcall MINMAXM(Complex y, const Complex cx, int desc)
{
	if(noMatrixOrEmpty(y, cx, COPYC)) return;
	Pmatrix x= toMatrix(cx);
	Complex *A= x->A;
	int num= x->len;
	Complex z= A[--num];
	while(num--){
		if(CMPC(z, A[num])*desc > 0)  z=A[num];
	}
	COPYM(y, z);
}

void _stdcall MINM(Complex y, const Complex cx)
{
	MINMAXM(y, cx, 1);
}

void _stdcall MAXM(Complex y, const Complex cx)
{
	MINMAXM(y, cx, -1);
}

void _stdcall MIN3M(Complex y, const Complex a, const Complex b)
{
	if(isMatrix(a) || isMatrix(b)){
		errMatrix();
		return;
	}
	COPYC(y, CMPC(a, b)<0 ? a : b);
}

void _stdcall MAX3M(Complex y, const Complex a, const Complex b)
{
	if(isMatrix(a) || isMatrix(b)){
		errMatrix();
		return;
	}
	COPYC(y, CMPC(a, b)>0 ? a : b);
}

void checkLR(const Complex &x)
{
	if(!isMatrix(x) || toMatrix(x)->cols!=2){
		cerror(1053, "Matrix doesn't have 2 columns");
		return;
	}
}

int _stdcall SUMM(Complex y0, const Complex cx, int start, int step)
{
	int i, count=1;
	Complex t, y, w;

	if(step==2) checkLR(cx);
	if(noMatrixOrEmpty(y0, cx, COPYC)) return 1;
	Pmatrix x= toMatrix(cx);
	Complex *A= x->A;
	int num= x->len;
	matrixToComplex(y0);
	y=y0;
	t=ALLOCC(y.r[-4]);
	COPYC(y, A[start]);
	for(i=start+step; i<num; i+=step){
		PLUSC(t, y, A[i]);
		w=t; t=y; y=w;
		count++;
	}
	if(y.r!=y0.r){ COPYC(y0, y); t=y; }
	FREEC(t);
	return count;
}

void _stdcall SUM1M(Complex y, const Complex x)
{
	SUMM(y, x, 0, 1);
}

void _stdcall SUMXM(Complex y, const Complex x)
{
	SUMM(y, x, 0, 2);
}

void _stdcall SUMYM(Complex y, const Complex x)
{
	SUMM(y, x, 1, 2);
}

int _stdcall SUMMULM(Complex y0, const Complex cx, int start, int step, int diff=0)
{
	int i, count=0;
	Complex t, y, u, w;

	if(step==2) checkLR(cx);
	if(noMatrixOrEmpty(y0, cx, SQRC)) return 1;
	Pmatrix x= toMatrix(cx);
	Complex *A= x->A;
	int num= x->len;
	matrixToComplex(y0);
	y=y0;
	ZEROC(y);
	t=ALLOCC(y.r[-4]);
	u=ALLOCC(y.r[-4]);
	for(i=start; i<num; i+=step){
		MULTC(u, A[i], A[i+diff]);
		PLUSC(t, y, u);
		w=t; t=y; y=w;
		count++;
	}
	if(y.r!=y0.r){ COPYC(y0, y); t=y; }
	FREEC(u);
	FREEC(t);
	return count;
}

void _stdcall SUM2M(Complex y, const Complex x)
{
	SUMMULM(y, x, 0, 1);
}

void _stdcall SUMX2M(Complex y, const Complex x)
{
	SUMMULM(y, x, 0, 2);
}

void _stdcall SUMY2M(Complex y, const Complex x)
{
	SUMMULM(y, x, 1, 2);
}

int _stdcall SUMXYM(Complex y, const Complex x)
{
	return SUMMULM(y, x, 0, 2, 1);
}

//ave=sum/n
void _stdcall AVEM(Complex y, const Complex cx, int start, int step)
{
	DIVI1C(y, SUMM(y, cx, start, step));
}

void _stdcall AVE1M(Complex y, const Complex x)
{
	AVEM(y, x, 0, 1);
}

void _stdcall AVEXM(Complex y, const Complex x)
{
	AVEM(y, x, 0, 2);
}

void _stdcall AVEYM(Complex y, const Complex x)
{
	AVEM(y, x, 1, 2);
}

void _stdcall AVEMULM(Complex y, const Complex cx, int start, int step, int diff=0)
{
	DIVI1C(y, SUMMULM(y, cx, start, step, diff));
}

void _stdcall AVE2M(Complex y, const Complex x)
{
	AVEMULM(y, x, 0, 1);
}

void _stdcall AVEX2M(Complex y, const Complex x)
{
	AVEMULM(y, x, 0, 2);
}

void _stdcall AVEY2M(Complex y, const Complex x)
{
	AVEMULM(y, x, 1, 2);
}

//var=(sumq-sum^2/n)/(n-sample)
void _stdcall VARM(Complex y, const Complex cx, unsigned sample, int start, int step, bool sqrt=false)
{
	Complex t, u;

	if(step==2) checkLR(cx);
	matrixToComplex(y);
	if(!isMatrix(cx)){
		ZEROC(y);
		return;
	}
	t=ALLOCC(y.r[-4]);
	u=ALLOCC(y.r[-4]);
	int num= SUMM(u, cx, start, step);
	SQRC(t, u);
	DIVIC(u, t, num);
	SUMMULM(t, cx, start, step);
	MINUSC(y, t, u);
	DIVI1C(y, num-sample);
	if(sqrt){
		SQRTC(u, y);
		COPYC(y, u);
	}
	FREEC(u);
	FREEC(t);
}

void _stdcall VAR0M(Complex y, const Complex x)
{
	VARM(y, x, 0, 0, 1);
}

void _stdcall VAR1M(Complex y, const Complex x)
{
	VARM(y, x, 1, 0, 1);
}

void _stdcall VARX0M(Complex y, const Complex x)
{
	VARM(y, x, 0, 0, 2);
}

void _stdcall VARX1M(Complex y, const Complex x)
{
	VARM(y, x, 1, 0, 2);
}

void _stdcall VARY0M(Complex y, const Complex x)
{
	VARM(y, x, 0, 1, 2);
}

void _stdcall VARY1M(Complex y, const Complex x)
{
	VARM(y, x, 1, 1, 2);
}

void _stdcall STDEV0M(Complex y, const Complex x)
{
	VARM(y, x, 0, 0, 1, true);
}

void _stdcall STDEV1M(Complex y, const Complex x)
{
	VARM(y, x, 1, 0, 1, true);
}

void _stdcall STDEVX0M(Complex y, const Complex x)
{
	VARM(y, x, 0, 0, 2, true);
}

void _stdcall STDEVX1M(Complex y, const Complex x)
{
	VARM(y, x, 1, 0, 2, true);
}

void _stdcall STDEVY0M(Complex y, const Complex x)
{
	VARM(y, x, 0, 1, 2, true);
}

void _stdcall STDEVY1M(Complex y, const Complex x)
{
	VARM(y, x, 1, 1, 2, true);
}


//b=(n*sumxy-sumx*sumy)/(n*sumxq-sumx^2)
//a=(sumy-b*sumx)/n
void _stdcall LRABM(Complex a, Complex b, const Complex x)
{
	Complex t, u, v, sx, sy;
	int n;

	Tint prec=getPrecision(b);
	t=ALLOCC(prec);
	u=ALLOCC(prec);
	v=ALLOCC(prec);
	sx=ALLOCC(prec);
	sy=ALLOCC(prec);
	n=SUMXYM(b, x);
	MULTI1C(b, n);
	SUMXM(sx, x);
	SUMYM(sy, x);
	MULTC(t, sx, sy);
	MINUSC(u, b, t);
	SUMX2M(t, x);
	MULTI1C(t, n);
	SQRC(b, sx);
	MINUSC(v, t, b);
	DIVC(b, u, v);
	if(a.r){
		matrixToComplex(a);
		MULTC(u, b, sx);
		MINUSC(a, sy, u);
		DIVI1C(a, n);
	}
	FREEC(sy);
	FREEC(sx);
	FREEC(v);
	FREEC(u);
	FREEC(t);
}

void _stdcall LRAM(Complex y, const Complex x)
{
	Complex b=ALLOCC(getPrecision(y));
	LRABM(y, b, x);
	FREEC(b);
}

void _stdcall LRBM(Complex y, const Complex x)
{
	Complex a;
	a.r=0;
	LRABM(a, y, x);
}

//x=(y-a)/b
void _stdcall LRXM(Complex x, const Complex d, const Complex y)
{
	Complex a, b;

	matrixToComplex(x);
	Tint prec=getPrecision(y);
	a=ALLOCC(prec);
	b=ALLOCC(prec);
	LRABM(a, b, d);
	MINUSC(x, y, a);
	DIVC(a, x, b);
	COPYC(x, a);
	FREEC(b);
	FREEC(a);
}

//y=a+b*x
void _stdcall LRYM(Complex y, const Complex d, const Complex x)
{
	Complex a, b;

	matrixToComplex(y);
	Tint prec=getPrecision(y);
	a=ALLOCC(prec);
	b=ALLOCC(prec);
	LRABM(a, b, d);
	MULTC(y, b, x);
	PLUSC(b, a, y);
	COPYC(y, b);
	FREEC(b);
	FREEC(a);
}

//r=(n*sumxy-sumx*sumy)/sqrt((n*sumxq-sumx^2)*(n*sumyq-sumy^2))
void _stdcall LRRM(Complex y, const Complex x)
{
	Complex t, u, sx, sy;
	int n;

	Tint prec=getPrecision(y);
	t=ALLOCC(prec);
	u=ALLOCC(prec);
	sx=ALLOCC(prec);
	sy=ALLOCC(prec);
	n=SUMXYM(y, x);
	MULTI1C(y, n);
	SUMXM(sx, x);
	SUMYM(sy, x);
	MULTC(t, sx, sy);
	MINUSC(u, y, t);
	SUMX2M(t, x);
	MULTI1C(t, n);
	SQRC(y, sx);
	MINUSC(sx, t, y);
	SUMY2M(t, x);
	MULTI1C(t, n);
	SQRC(y, sy);
	MINUSC(sy, t, y);
	MULTC(y, sx, sy);
	SQRTC(t, y);
	DIVC(y, u, t);
	FREEC(sy);
	FREEC(sx);
	FREEC(u);
	FREEC(t);
}

void _stdcall HARMONM(Complex y0, const Complex cx)
{
	int i;
	Complex t, u, y, w;

	if(noMatrixOrEmpty(y0, cx, COPYC)) return;
	Pmatrix x= toMatrix(cx);
	Complex *A= x->A;
	int num= x->len;
	matrixToComplex(y0);
	Tint prec=getPrecision(y0);
	y=y0;
	t=ALLOCC(prec);
	u=ALLOCC(prec);
	ZEROC(y);
	for(i=0; i<num; i++){
		INVERTC(u, A[i]);
		PLUSC(t, y, u);
		w=t; t=y; y=w;
	}
	SETC(u, num);
	DIVC(t, u, y);
	if(y.r!=y0.r){
		t=y;
	}
	else{
		COPYC(y0, t);
	}
	FREEC(u);
	FREEC(t);
}

int _stdcall PRODUCTM(Complex y0, const Complex cx)
{
	int i, count=2;
	Complex t, y, w;

	if(noMatrixOrEmpty(y0, cx, COPYC)) return 1;
	Pmatrix x= toMatrix(cx);
	Complex *A= x->A;
	int num= x->len;
	matrixToComplex(y0);
	y=y0;
	t=ALLOCC(y.r[-4]);
	MULTC(y, A[0], A[1]);
	for(i=2; i<num; i++){
		MULTC(t, y, A[i]);
		w=t; t=y; y=w;
		count++;
	}
	if(y.r!=y0.r){ COPYC(y0, y); t=y; }
	FREEC(t);
	return count;
}


//geom=product^(1/n)
void _stdcall GEOMM(Complex y, const Complex cx)
{
	Complex t, n;
	n=ALLOCC(2);
	t=ALLOCC(getPrecision(y));
	SETC(n, PRODUCTM(t, cx));
	ROOTC(y, n, t);
	FREEC(t);
	FREEC(n);
}


void _stdcall REPEATOPX(Tbinary f, Complex y, const Complex cx, int errId, char *errStr)
{
	int i;
	Pint t, w, y0;

	if(noMatrixOrEmpty(y, cx, COPYC)) return;
	Pmatrix x= toMatrix(cx);
	Complex *A= x->A;
	int num= x->len;
	for(i=0; i<num; i++){
		if(isImag(A[i])){
			cerror(errId, errStr);
			return;
		}
	}
	y0=y.r;
	t=ALLOCX(y.r[-4]);
	ZEROX(y.i);
	f(y.r, A[0].r, A[1].r);
	for(i=2; i<num; i++){
		f(t, y.r, A[i].r);
		w=t; t=y.r; y.r=w;
	}
	if(y.r!=y0){
		COPYX(y0, y.r);
		t=y.r;
	}
	FREEX(t);
}

void _stdcall GCDM(Complex y, const Complex cx)
{
	REPEATOPX(GCDX, y, cx, 1026, "The greatest common divisor of complex numbers");
}

void _stdcall LCMM(Complex y, const Complex cx)
{
	REPEATOPX(LCMX, y, cx, 1027, "The least common multiple of complex numbers");
}

int __cdecl SORTCMP(const void *elem1, const void *elem2)
{
	return CMPC(*(const Complex*)elem1, *(const Complex*)elem2);
}

int __cdecl SORTDCMP(const void *elem1, const void *elem2)
{
	return CMPC(*(const Complex*)elem2, *(const Complex*)elem1);
}

void _fastcall SORTM(Complex cx)
{
	if(!isMatrix(cx)) return;
	Pmatrix x= toMatrix(cx);
	qsort(x->A, x->len, sizeof(Complex), SORTCMP);
}

void _fastcall SORTDM(Complex cx)
{
	if(!isMatrix(cx)) return;
	Pmatrix x= toMatrix(cx);
	qsort(x->A, x->len, sizeof(Complex), SORTDCMP);
}

void _fastcall REVERSEM(Complex cx)
{
	if(!isMatrix(cx)) return;
	Pmatrix x= toMatrix(cx);
	Complex w, *b, *e;
	if(x->len)
		for(b=x->A, e=&x->A[x->len-1]; b<e; b++, e--){
			w=*b; *b=*e; *e=w;
		}
}

//warning: cx will be sorted
void _stdcall MEDIANM(Complex y, Complex cx)
{
	if(noMatrixOrEmpty(y, cx, COPYC)) return;
	Pmatrix x= toMatrix(cx);
	int num= x->len;
	Complex *A= x->A;
	matrixToComplex(y);
	SORTM(cx);
	if(num&1){
		COPYC(y, A[num>>1]);
	}
	else{
		PLUSC(y, A[num>>1], A[(num>>1)-1]);
		DIVI1C(y, 2);
	}
}

//warning: cx will be sorted
void _stdcall MODEM(Complex y, Complex cx)
{
	int i, j, m, im=0;

	if(noMatrixOrEmpty(y, cx, COPYC)) return;
	Pmatrix x= toMatrix(cx);
	Complex *A= x->A;
	Complex first= A[0];
	SORTM(cx);
	m=1;
	j=1;
	for(i=1; i<x->len; i++){
		if(CMPC(A[i-1], A[i])==0){
			j++;
		}
		else{
			if(j>m){
				m=j;
				im=i-1;
			}
			j=1;
		}
	}
	if(j>m){
		m=j;
		im=i-1;
	}
	COPYM(y, m>1 ? A[im] : first);
}

//-------------------------------------------------------------------

extern void parse(const char *input, const char **e);
extern void deref(Complex &x);
extern Complex *deref1(Complex &x);
extern void _stdcall ASSIGNM(Complex y, const Complex a, const Complex x);
extern Darray<Complex> numStack;


/*
(F[0] + 4*F[1] + F[2])/2
(F[0] + 4*f(a+h/2) + 2*F[1] + 4*f(a+h*3/2) + F[2])/4
*/
void integralRecurse(Complex y, Complex *F0, Complex a, Complex h0, Complex var, const char *formula, int depth, int bits)
{
	Complex t, u, *v, h, oldV, F[5];
	const char *e;
	int i;

	depth++;
	F[1].r=F[1].i=F[3].r=F[3].i=0;
	Tint prec=getPrecision(y);
	t=ALLOCC(prec);
	u=ALLOCC(prec);
	h=ALLOCC(prec);
	DIVIM(h, h0, 2);
	PLUSM(t, a, h);
	PLUSM(u, t, h0);
	v=deref1(var);
	oldV=*v;
	//function values
	for(i=1; i<=3; i+=2){
		*v= (i==1) ? t : u;
		if(error) goto lfree;
		parse(formula, &e);
		if(error)
			goto lfree;
		F[i]=*numStack--;
		deref(F[i]);
	}
	F[0]=F0[0];
	F[2]=F0[1];
	F[4]=F0[2];
	//sum5
	PLUSM(t, F[0], F[4]);
	DIVI1M(t, 2);
	PLUSM(u, t, F[2]);
	DIVI1M(u, 2);
	PLUSM(t, u, F[1]);
	PLUSM(y, t, F[3]);
	//sum3
	PLUSM(t, F[0], F[4]);
	DIVI1M(t, 4);
	PLUSM(u, t, F[2]);
	MULTI1M(u, 2);
	//difference
	MINUSM(t, u, y);
	LSHIM(u, y, -bits);
	ABSM(u);
	ABSM(t);
	//recurse
	if(CMPC(t, u)>0 && !error && depth<50){ ///
		PLUSM(t, a, h0);
		integralRecurse(u, F+2, t, h, var, formula, depth, bits);
		integralRecurse(t, F, a, h, var, formula, depth, bits);
		PLUSM(y, u, t);
		DIVI1M(y, 2);
	}
lfree:
	*v=oldV;
	FREEM(F[3]);
	FREEM(F[1]);
	FREEM(h);
	FREEM(u);
	FREEM(t);
}

//integral= (b-a)/3 * integralRecurse
void integral(Complex y, Complex a, Complex b, Complex var, const char *formula, int bits)
{
	Complex h, F[3];
	int i;
	Tint prec, oldyPrec, oldPrec;
	const char *e;

	oldPrec=precision;
	oldyPrec=y.r[-4];
	precision=y.r[-4]=(bits+92*(TintBits/32))/TintBits;
	memset(F, 0, sizeof(F));
	prec=getPrecision(y);
	h=ALLOCC(prec);
	PLUSM(h, a, b);
	DIVI1M(h, 2);
	//function values
	for(i=0; i<3; i++){
		ASSIGNM(y, var, (i==0 ? a : (i==1 ? h : b)));
		parse(formula, &e);
		if(error) goto lfree;
		F[i]=*numStack--;
		deref(F[i]);
	}
	MINUSM(h, b, a);
	DIVI1M(h, 2);
	integralRecurse(y, F, a, h, var, formula, 0, bits);
	MULTI1M(h, 2);
	if(isMatrix(h)) ABSM(h);
	MULTM(F[0], y, h);
	DIVIM(y, F[0], 3);
lfree:
	FREEM(h);
	FREEM(F[2]);
	FREEM(F[1]);
	FREEM(F[0]);
	precision=oldPrec;
	y.r[-4]=oldyPrec;
}

void INTEGRALM(Complex y, Complex a, Complex b, Complex p, Complex var, const char *formula)
{
	int bits;
	Tuint timeOrPrec;
	DWORD t0, t1, t2;

	if(isMatrix(p) || isImag(p) || !isDword(p.r)){
		cerror(1058, "The fourth argument has to be integer");
		return;
	}
	timeOrPrec= toDword(p.r);
	if(timeOrPrec<100){
		integral(y, a, b, var, formula, int(timeOrPrec*3.322));
	}
	else{
		t0=t2=GetTickCount();
		for(bits=16; bits<96; bits+=8){
			t1=t2;
			integral(y, a, b, var, formula, bits);
			t2=GetTickCount();
			if((t2-t0)+(t2-t1)*4 >(timeOrPrec/2) || error) break;
		}
	}
}
