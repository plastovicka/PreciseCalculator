/*
	(C) Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "matrix.h"
#include "darray.h"
#include "jit.h"

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

Tint getPrecision(const Complex &cx)
{
	if(isMatrix(cx)){
		Pmatrix x=toMatrix(cx);
		if(x->len>0){
			MatrixItem &m= x->A[0];
			if(!m.denominator) return m.r[-4];
		}
	}
	return cx.r[-4];
}

//materialize a full Complex value from a matrix item
ComplexItem::ComplexItem(const MatrixItem &m)
{
	if(m.denominator){
		Pint y= c.r = &x.m;
		y[-4]= 2; //length
		if(m.numerator==0){
			ZEROX(y);
			y[-2] = 0; //CMPX requires initialized sign
		}
		else{
			y[0]= m.numerator;
			y[1]= m.denominator<0 ? -m.denominator : m.denominator;
			y[-1]= (Tuint)y[0] >= (Tuint)y[1]; //exponent
			y[-2]= m.denominator<0; //sign
			y[-3]= -2; //fraction
		}
	}
	else{
		c.r=m.r;
	}
	c.i= m.i;
}

void ComplexItem::set(const ComplexItem &s)
{
	if(s.c.r==&s.x.m){
		c.r = &x.m;
		COPYX(c.r, s.c.r);
	}
	else c.r= s.c.r;
	c.i = s.c.i;
}

void ComplexItem::free()
{
	if(c.r!=&x.m) FREEX(c.r);
	if(c.i) FREEX(c.i);
}

static void newItem(MatrixItem &m, Complex &c)
{
	if(isFraction(c.r) && c.r[1]>0){
		m.numerator= c.r[0];
		m.denominator= c.r[-2] ? -c.r[1] : c.r[1];
	}
	else if(isZero(c.r)){
		m.numerator = 0;
		m.denominator = 1;
	}
	else{
		m.denominator= 0;
		m.r= NEWCOPYX(c.r);
	}
	m.i = isImag(c) ? NEWCOPYX(c.i) : 0;
}

static void newItem(MatrixItem &dest, const MatrixItem &src)
{
	if(src.denominator){
		dest.numerator= src.numerator;
		dest.denominator= src.denominator;
	}
	else{
		dest.denominator= 0;
		dest.r= NEWCOPYX(src.r);
	}
	dest.i= !isZero_safe(src.i) ? NEWCOPYX(src.i) : 0;
}

static void assignItem(MatrixItem &m, const Complex &c)
{
	if(isFraction(c.r) && c.r[1]>0) {
		if(!m.denominator) FREEX(m.r);
		m.numerator = c.r[0];
		m.denominator = c.r[-2] ? -c.r[1] : c.r[1];
	}
	else if(isZero(c.r)) {
		if(!m.denominator) FREEX(m.r);
		m.numerator = 0;
		m.denominator = 1;
	}
	else if(m.denominator) {
		m.denominator = 0;
		m.r = NEWCOPYX(c.r);
	}
	else assign(m.r, c.r);

	if(isImag(c)) assign(m.i, c.i);
	else ZEROX_safe(m.i);
}

static void copyItem(MatrixItem &dest, const Complex &src)
{
	if(isFraction(src.r) && src.r[1]>0) {
		if(!dest.denominator) FREEX(dest.r);
		dest.numerator = src.r[0];
		dest.denominator = src.r[-2] ? -src.r[1] : src.r[1];
	}
	else if(isZero(src.r)) {
		if(!dest.denominator) FREEX(dest.r);
		dest.numerator = 0;
		dest.denominator = 1;
	}
	else if(dest.denominator) {
		dest.denominator = 0;
		dest.r = NEWCOPYX(src.r);
	}
	else COPYX(dest.r, src.r);

	if(isImag(src)) {
		if(!dest.i) dest.i = NEWCOPYX(src.i);
		else COPYX(dest.i, src.i);
	}
	else ZEROX_safe(dest.i);
}

static void copyItem(MatrixItem &dest, const MatrixItem &src)
{
	if(src.denominator) {
		if(!dest.denominator) FREEX(dest.r);
		dest.numerator = src.numerator;
		dest.denominator = src.denominator;
	}
	else if(dest.denominator) {
		dest.denominator = 0;
		dest.r = NEWCOPYX(src.r);
	}
	else COPYX(dest.r, src.r);

	if(!isZero_safe(src.i)) {
		if(!dest.i) dest.i = NEWCOPYX(src.i);
		else COPYX(dest.i, src.i);
	}
	else ZEROX_safe(dest.i);
}

static void copyItem(MatrixItem &dest, const MatrixItem &src, Tint prec)
{
	if(src.denominator) {
		if(!dest.denominator) FREEX(dest.r);
		dest.numerator = src.numerator;
		dest.denominator = src.denominator;
	}
	else {
		if(dest.denominator) {
			dest.denominator = 0;
			dest.r = ALLOCX(prec);
		}
		COPYX(dest.r, src.r);
	}

	if(!isZero_safe(src.i)) {
		if(!dest.i) dest.i = ALLOCX(prec);
		COPYX(dest.i, src.i);
	}
	else ZEROX_safe(dest.i);
}

void MatrixItem::set(Tuint n)
{
	if(!denominator) FREEX(r);
	denominator = 1;
	numerator = n;
	ZEROX_safe(i);
}

inline void MatrixItem::free()
{
	if(!denominator) FREEX(r);
	FREEX(i);
}

void _fastcall FREE_ARRAYM(Pmatrix x)
{
	for(int i=0; i<x->len; i++){
		x->A[i].free();
	}
	delete[] x->A;
}

void _fastcall FREE_ARRAYM(Complex &x)
{
	if(isMatrix(x)) FREE_ARRAYM(toMatrix(x));
}

void matrixToComplex(Complex &cx)
{
	if(!isMatrix(cx)) return;
	Pmatrix x=toMatrix(cx);
	FREE_ARRAYM(x);
	x->tag=0; //zero
	cx.r[-2]=0;
}

inline void MatrixItem::init()
{
	numerator = 0;
	denominator = 1;
	i = 0;
}

//create a new matrix or resize matrix to a->cols, a->rows
void prepareM(Complex &cy, int cols, int rows)
{
	int i, len;

	len= cols * rows;
	Pmatrix y= toMatrix(cy);
	if(isMatrix(cy)){
		if(y->alen < len){
			MatrixItem *A0=y->A;
			y->A= new MatrixItem[y->alen= len];
			memcpy(y->A, A0, sizeof(MatrixItem)*y->len);
			delete[] A0;
		}
		for(i=y->len; i<len; i++){
			y->A[i].init();
		}
		for(i=len; i<y->len; i++){
			y->A[i].free();
		}
	}
	else{
		y->tag= -12;
		y->alen= 0;
		y->A= NULL;
	}
	if(!y->alen && len){
		y->A= new MatrixItem[y->alen= len];
		for(i=0; i<len; i++){
			y->A[i].init();
		}
	}
	y->rows= rows;
	y->cols= cols;
	y->len= len;
}

void prepareM(Complex &cy, Pmatrix a)
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

bool sameSizeM(const Complex &ca, const Complex &cb)
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
Complex _fastcall NEWCOPYM(const Complex &ca)
{
	if(!isMatrix(ca)) return NEWCOPYC(ca);
	Complex result= ALLOCC(ca.r[-4]);
	Pmatrix y= toMatrix(result);
	Pmatrix a= toMatrix(ca);
	y->tag= -12;
	y->rows= a->rows;
	y->cols= a->cols;
	y->len= a->len;
	if(a->len) {
		y->A= new MatrixItem[y->alen= a->alen];
		for(int i=0; i<a->len; i++) {
			newItem(y->A[i], a->A[i]);
		}
	}
	else {
		y->A= NULL;
		y->alen= 0;
	}
	return result;
}

void _fastcall FREEM(Complex &cx)
{
	if(cx.r && isMatrix(cx)){
		FREE_ARRAYM(toMatrix(cx));
	}
	FREEX(cx.r);
	FREEX(cx.i);
}

void assignM(Complex &dest, const Complex &src)
{
	if(!dest.r || getPrecision(dest)<getPrecision(src)){
		FREEM(dest);
		dest=NEWCOPYM(src);
	}
	else{
		COPYM(dest, src);
	}
}

const int MatrixDisplayLen = 500; //limit for variables window

void _stdcall WRITEM(char *buf, const Complex &cx, int digits, int cr)
{
	int i, j, n;
	MatrixItem *p;

	if(!isMatrix(cx)){
		WRITEC(buf, cx, digits);
		return;
	}
	Pmatrix x= toMatrix(cx);
	*buf++='(';
	p=x->A;
	n = 0;
	for(i=0; i<x->rows; i++){
		if(i){ *buf++=' '; *buf++='\\'; *buf++= cr>0 ? '\n' : ' '; }
		for(j=0; j<x->cols; j++){
			if(j){ *buf++=','; *buf++=' '; }
			if(cr<0 && ++n>MatrixDisplayLen) {
				*buf++ = '.';
				*buf++ = '.';
				*buf++ = '.';
				*buf=0;
				return;
			}
			ComplexItem t(*p++);
			WRITEC(buf, t, digits);
			buf=strchr(buf, 0);
		}
	}
	*buf++=')';
	*buf=0;
}

int _stdcall LENM(const Complex &cx, int digits, int cr)
{
	int i, n;
	MatrixItem *p;

	if(!isMatrix(cx)){
		return LENC(cx, digits);
	}
	Pmatrix x= toMatrix(cx);
	n=2+x->rows;
	p=x->A;
	for(i=0; i<x->len; i++){
		ComplexItem c(*p++);
		n+=2+LENC(c, digits);
		if(n<0) return 0;
		if(cr<0 && i>=MatrixDisplayLen) break;
	}
	return n;
}

char*_stdcall AWRITEM(const Complex &x, int digits, int cr)
{
	int len=LENM(x, digits, cr);
	if(len<=0) {
		char *buf= new char[1];
		*buf=0;
		return buf;
	}
	else {
		char *buf= new char[len];
		WRITEM(buf, x, digits, cr);
		return buf;
	}
}

//-------------------------------------------------------------------
void _fastcall EMPTYM(Complex &x)
{
	prepareM(x, 0, 0);
}

void _fastcall SETM(Complex &x, Tuint n)
{
	matrixToComplex(x);
	SETC(x, n);
}

void _fastcall WIDTHM(Complex &x)
{
	unsigned i;
	if(!isMatrix(x)) i=1;
	else i=toMatrix(x)->cols;
	SETM(x, i);
}

void _fastcall HEIGHTM(Complex &x)
{
	unsigned i;
	if(!isMatrix(x)) i=1;
	else i=toMatrix(x)->rows;
	SETM(x, i);
}

void _fastcall COUNTM(Complex &x)
{
	unsigned i;
	if(!isMatrix(x)) i=1;
	else i=toMatrix(x)->len;
	SETM(x, i);
}


void _fastcall TRANSPM(Complex &cx)
{
	int i, j, rows, cols;
	MatrixItem *A;

	if(!isMatrix(cx)) return;
	Pmatrix x=toMatrix(cx);
	rows= x->cols;
	cols= x->rows;
	if(cols!=1 && rows!=1){
		if(x->len==0) return;
		A= new MatrixItem[x->alen];
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

void _stdcall TRANSP2M(Complex &y, const Complex &cx)
{
	COPYM(y, cx);
	TRANSPM(y);
}

//warning: a and b are destroyed
void _stdcall CONCATROWM(Complex &y, Complex &a, Complex &b)
{
	int cola, colb, rows;
	Pmatrix ma, mb, my;
	MatrixItem *p;

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
	if (cola * rows > MatrixMaxLen) {
		cerror(1066, "Matrix has too large dimensions");
		return;
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
		my->A= new MatrixItem[my->alen=2*my->len];
		if(ma) memcpy(my->A, ma->A, ma->len*sizeof(MatrixItem));
		else newItem(my->A[0], a);
	}
	p= my->A + (ma ? ma->len : 1);
	if(mb) memcpy(p, mb->A, mb->len*sizeof(MatrixItem));
	else newItem(*p, b);

	if(ma) ma->len=ma->rows=ma->cols=0;
	if(mb) mb->len=mb->rows=mb->cols=0;
}

//warning: a and b are destroyed
void _stdcall CONCATM(Complex &y, Complex &a, Complex &b)
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
	if(!isMatrix(cy)){
		if(inc) PLUSX(z.r, cy.r, one);
		else MINUSX(z.r, cy.r, one);
		COPYtoImag(z, cy.i);
		assignC(cy, z);
	}
	else{
		Pmatrix y= toMatrix(cy);
		MatrixItem *pm= y->A + y->cols*D[0] + D[2];
		ComplexItem py(*pm);
		if(inc) PLUSX(z.r, py.c.r, one);
		else MINUSX(z.r, py.c.r, one);
		COPYtoImag(z, py.c.i);
		assignItem(*pm, z);
	}
}

void assignRange(Complex &cy, const Complex &cx, const int *D0)
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
			else{
				ComplexItem c(x->A[0]);
				assignC(cy, c);
			}
		}
	}
	else{
		int rows = D[1]-D[0]+1;
		int cols = D[3]-D[2]+1;
		Pmatrix y= toMatrix(cy);
		MatrixItem *py = y->A + y->cols*D[0] + D[2];
		if(!isMatrix(cx)){
			if(rows!=1 || cols!=1) differentSize();
			else assignItem(*py, cx);
		}
		else{
			Pmatrix x= toMatrix(cx);
			if(x->rows!=rows || x->cols!=cols) differentSize();
			else{
				MatrixItem *px= x->A;
				for(int r=0; r<rows; r++){
					for(int c=0; c<cols; c++){
						ComplexItem cxi(*px++);
						assignItem(*py++, cxi);
					}
					py+= y->cols - cols;
				}
			}
		}
	}
}

void _stdcall INDEXM(Complex &cy, const Complex &cx, int *D)
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
		ComplexItem c(x->A[x->cols*D[0] + D[2]]);
		COPYM(cy, c);
	}
	else{
		prepareM(cy, cols, rows);
		MatrixItem *A= toMatrix(cy)->A;
		for(i=D[0]; i<=D[1]; i++){
			MatrixItem *B= x->A + x->cols*i + D[2];
			for(j=0; j<cols; j++){
				copyItem(*A++, *B++);
			}
		}
	}
}

void _stdcall EQUALM(Complex &cy, const Complex &ca, const Complex &cb)
{
	if(noMatrix(cy, ca, cb, EQUALC)) return;
	if(!sameSizeM(ca, cb)) return;
	matrixToComplex(cy);
	Pmatrix a=toMatrix(ca), b=toMatrix(cb);
	for(int i=0; i<a->len; i++){
		ComplexItem a1(a->A[i]), b1(b->A[i]);
		if(CMPC(a1, b1)){
			ZEROC(cy);
			return;
		}
	}
	ONEC(cy);
}

void _stdcall NOTEQUALM(Complex &cy, const Complex &ca, const Complex &cb)
{
	EQUALM(cy, ca, cb);
	SETC(cy, isZero(cy));
}


static void plusminusM(MatrixItem *C, MatrixItem *A, MatrixItem *B, int rows, int cols, int zeroRow, int zeroCol, int cd, int ad, int bd, TbinaryC f, Complex& t)
{
	int i, j;

	cd-=cols;
	ad-=cols;
	bd-=cols;
	for(i=zeroRow; i<rows; i++){
		for(j=zeroCol; j<cols; j++){
			ComplexItem a(*A), b(*B);
			f(t, a, b);
			copyItem(*C, t);
			C++; A++; B++;
		}
		if(zeroCol){
			copyItem(*C, *A);
			C++; A++; B++;
		}
		C+=cd;
		A+=ad;
		B+=bd;
	}
	if(zeroRow){
		for(j=0; j<cols; j++){
			copyItem(*C, *A);
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

time: T(2*n)=7*T(n)+18*n^2
*/
static void _stdcall MULTMRecurse(int rows, int cols, int len, MatrixItem *aA, int ad, MatrixItem *bA, int bd, MatrixItem *cA, int cd, Tint prec)
{
	int i, j, k, rows2, cols2, len2, rows1, cols1, len1, lenM, lenM1;
	Complex t, u, v;
	MatrixItem *S, *buf, *M[5], *A[2][2], *B[2][2];
	int oddRows, oddCols, oddLen;
	const int MINMULT=10;

	if(rows<MINMULT || cols<MINMULT || len<MINMULT){
		S=cA;
		t=ALLOCR(prec);
		u=ALLOCR(prec);
		v=ALLOCR(prec);
		for(i=0; i<rows; i++){
			for(j=0; j<cols; j++){
				ZEROC(v);
				for(k=0; k<len; k++){
					if(error) break;
					ComplexItem ai(aA[i*ad+k]), bi(bA[k*bd+j]);
					MULTC(t, ai, bi);
					PLUSC(u, v, t);
					std::swap(v, u);
				}
				copyItem(*S++, v);
			}
			S+=cd-cols;
		}
		FREEC(v);
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
	M[0]= buf= new MatrixItem[lenM];
	for(i=1; i<5; i++){
		M[i]= M[i-1] + lenM1;
	}
	for(i=0; i<lenM; i++){
		buf[i].init();
	}
	t=ALLOCR(prec);
	
	plusminusM(M[0], A[0][1], A[1][1], rows2, len1, oddRows, 0, len1, ad, ad, MINUSC, t);
	plusminusM(M[1], B[1][0], B[1][1], len1, cols2, 0, oddCols, cols2, bd, bd, PLUSC, t);
	MULTMRecurse(rows2, cols2, len1, M[0], len1, M[1], cols2, M[2], cols2, prec);
	plusminusM(M[0], A[0][0], A[1][1], rows2, len2, oddRows, oddLen, len2, ad, ad, PLUSC, t);
	plusminusM(M[1], B[0][0], B[1][1], len2, cols2, oddLen, oddCols, cols2, bd, bd, PLUSC, t);
	MULTMRecurse(rows2, cols2, len2, M[0], len2, M[1], cols2, M[3], cols2, prec);
	plusminusM(M[0], A[0][0], A[0][1], rows2, len1, 0, 0, len1, ad, ad, PLUSC, t);
	MULTMRecurse(rows2, cols1, len1, M[0], len1, B[1][1], bd, M[4], cols1, prec);
	plusminusM(M[0], B[1][0], B[0][0], len1, cols2, 0, 0, cols2, bd, bd, MINUSC, t);
	MULTMRecurse(rows1, cols2, len1, A[1][1], ad, M[0], cols2, M[1], cols2, prec);
	plusminusM(M[0], M[2], M[3], rows2, cols2, 0, 0, cols2, cols2, cols2, PLUSC, t);
	plusminusM(M[2], M[0], M[4], rows2, cols2, 0, oddCols, cols2, cols2, cols1, MINUSC, t);
	plusminusM(cA, M[2], M[1], rows2, cols2, oddRows, 0, cd, cols2, cols2, PLUSC, t);
	plusminusM(M[2], B[0][1], B[1][1], len2, cols1, oddLen, 0, cols1, bd, bd, MINUSC, t);
	MULTMRecurse(rows2, cols1, len2, A[0][0], ad, M[2], cols1, M[0], cols1, prec);
	plusminusM(cA+cols2, M[4], M[0], rows2, cols1, 0, 0, cd, cols1, cols1, PLUSC, t);
	plusminusM(M[4], A[1][0], A[1][1], rows1, len2, 0, oddLen, len2, ad, ad, PLUSC, t);
	MULTMRecurse(rows1, cols2, len2, M[4], len2, B[0][0], bd, M[2], cols2, prec);
	plusminusM(cA+rows2*cd, M[1], M[2], rows1, cols2, 0, 0, cd, cols2, cols2, PLUSC, t);
	plusminusM(M[1], M[3], M[0], rows1, cols1, 0, 0, cols1, cols2, cols1, PLUSC, t);
	plusminusM(M[0], M[1], M[2], rows1, cols1, 0, 0, cols1, cols1, cols2, MINUSC, t);
	plusminusM(M[2], A[0][0], A[1][0], rows1, len2, 0, 0, len2, ad, ad, MINUSC, t);
	plusminusM(M[1], B[0][0], B[0][1], len2, cols1, 0, 0, cols1, bd, bd, PLUSC, t);
	MULTMRecurse(rows1, cols1, len2, M[2], len2, M[1], cols1, M[3], cols1, prec);
	plusminusM(cA+rows2*cd+cols2, M[0], M[3], rows1, cols1, 0, 0, cd, cols1, cols1, MINUSC, t);
	
	FREEC(t);
	for(i=0; i<lenM; i++){
		buf[i].free();
	}
	delete[] buf;
}

void _stdcall MULTM(Complex &cy, const Complex &ca, const Complex &cb)
{
	int colsb, rowsa, n;
	bool scalar;
	MatrixItem *S, m;

	if(!isMatrix(ca)){
		MULTCM(cy, cb, ca);
		return;
	}
	if(!isMatrix(cb)){
		MULTCM(cy, ca, cb);
		return;
	}
	Pmatrix a=toMatrix(ca), b=toMatrix(cb);
	colsb= b->cols;
	rowsa= a->rows;
	scalar= isVector(ca) && isVector(cb) && (a->cols!=1 || b->rows!=1);
	if(scalar){
		//scalar product of vectors
		n=a->len;
		if(n!=b->len){
			cerror(1046, "Vectors have different lengths");
			return;
		}
		matrixToComplex(cy);
		m.init();
		S=&m;
		colsb=rowsa=1;
	}
	else{
		n= a->cols;
		if(n!=b->rows){
			cerror(1045, "Number of columns of the first matrix is not equal to number of rows of the second matrix");
			return;
		}
		prepareM(cy, colsb, rowsa);
		if(!toMatrix(cy)->len) return;
		S= toMatrix(cy)->A;
	}
	MULTMRecurse(rowsa, colsb, n, a->A, a->cols, b->A, colsb, S, colsb, cy.r[-4]);

	if(scalar){
		ComplexItem c(m);
		COPYC(cy, c);
		m.free();
	}
}

void _fastcall SETDIAGM(Complex &cx, Tuint n)
{
	int i;

	if(!isMatrix(cx)){
		SETC(cx, n);
		return;
	}
	Pmatrix x= toMatrix(cx);
	for(i=0; i<x->len; i++){
		MatrixItem &m = x->A[i];
		m.free();
		m.init();
	}
	MatrixItem *p= x->A;
	for(i=min(x->rows, x->cols)-1; i>=0; i--){
		p->numerator = n;
		p+= 1+x->cols;
	}
}

void _stdcall POWMI(Complex &y, const Complex &cx, __int64 n)
{
	Complex z, t, u;

	if(!isMatrix(cx)){
		matrixToComplex(y);
		POWCI(y, cx, n);
		return;
	}
	Pmatrix x= toMatrix(cx);
	if(!isSquareM(x)) return;
	bool sgn= n<0;
	if(sgn) n=-n;
	t=ALLOCR(getPrecision(y));
	u=ALLOCR(getPrecision(y));
	prepareM(t, x);
	prepareM(u, x);
	z=y;
	prepareM(z, x);
	if(n&1) COPYM(z, cx);
	else SETDIAGM(z, 1);
	COPYM(u, cx);

	for(n>>=1; n>0; n>>=1){
		MULTM(t, u, u);
		std::swap(t, u);
		if(n&1){
			MULTM(t, z, u);
			std::swap(t, z);
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

void _stdcall POWM(Complex &y, const Complex &a, const Complex &b)
{
	if(noMatrix(y, a, b, POWC)) return;
	if(isMatrix(b) || isImag(b) || !is32bit(b.r)){
		cerror(1047, "Not integer power of matrix");
		return;
	}
	POWMI(y, a, to32bit(b.r));
}

void _fastcall ELIMM(Complex &cx)
{
	if(!isMatrix(cx)) return;
	Pmatrix m= toMatrix(cx);

	int n;
	Complex t, y;
	MatrixItem *p, *u, *v, *w, *z, *zp, *a;
	Tint prec=getPrecision(cx);
	t=ALLOCR(prec);
	y=ALLOCR(prec);
	n= m->cols;
	z= m->A + m->len;
	for(p=a=m->A, zp=p+n; p<zp && p<z; p+=n, zp+=n, a++){ //(p++ is inside cycle)
		//find not zero pivot
		u=p;
		while(u->isZero()){
			if((u+=n)>=z){ //all zeros => to next column
				a++;
				u=++p;
				if(p==zp) goto lend;
			}
		}
		if(u==p){
			//divide row where is pivot
			for(u++; u<zp; u++){
				ComplexItem cu(*u), cp(*p);
				DIVC(t, cu, cp);
				copyItem(*u, t);
			}
			p++->set(1);
		}
		else{
			//swap both rows and divide row which has pivot
			w=u;
			p++->set(1);
			for(v=p, u++; v<zp; v++, u++){
				ComplexItem cu(*u), cw(*w);
				DIVC(t, cu, cw);
				copyItem(*u, *v);
				copyItem(*v, t);
			}
			w->set(0);
		}
		//zero column where is pivot
		for(w=a; w<z; w+=n){
			if(w+1!=p && !w->isZero()){ //except pivot
				for(u=w+1, v=p; v<zp; u++, v++){
					ComplexItem cv(*v), cw(*w), cu(*u);
					MULTC(t, cv, cw);
					MINUSC(y, cu, t);
					copyItem(*u, y);
				}
				w->set(0);
			}
		}
	}
lend:
	FREEC(y);
	FREEC(t);
}

static void DETRANKM(Complex &D, Complex &cx, int det)
{
	if(noMatrix(D, cx, COPYC)){
		if(!det && !isZero(cx)) ONEC(D);
		return;
	}
	Pmatrix m= toMatrix(cx);
	if(det && !isSquareM(m)) return;
	SETM(D, 1);

	int n, result;
	Complex t, y;
	MatrixItem *p, *u, *v, *w, *z, *zp;
	Tint prec=getPrecision(cx);
	t=ALLOCR(prec);
	y=ALLOCR(prec);
	n= m->cols;
	z= m->A + m->len;
	for(result=0, p=m->A, zp=p+n; p<zp && p<z; result++, p+=n, zp+=n){
		//find not zero pivot
		u=p;
		while(u->isZero()){
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
				ComplexItem tu(*u);
				COPYC(t, tu);
				copyItem(*u, *v);
				copyItem(*v, t);
			}
		}
		//divide row where is pivot
		ComplexItem cp(*p);
		if(det){
			MULTC(t, D, cp);
			COPYC(D, t);
		}
		for(v=p+1; v<zp; v++){
			ComplexItem cv(*v);
			DIVC(t, cv, cp);
			copyItem(*v, t);
		}
		//zero column where is pivot
		for(w=(p++)+n; w<z; w+=n){
			for(u=w+1, v=p; v<zp; u++, v++){
				ComplexItem cv(*v), cw(*w), cu(*u);
				MULTC(t, cv, cw);
				MINUSC(y, cu, t);
				copyItem(*u, y);
			}
		}
	}
lend:
	FREEC(y);
	FREEC(t);
	if(!det) SETC(D, result);
}

void _stdcall DETM(Complex &D, Complex &cx)
{
	DETRANKM(D, cx, 1);
}

void _stdcall RANKM(Complex &D, Complex &cx)
{
	DETRANKM(D, cx, 0);
}

void _stdcall INVERTM(Complex &y, Complex &cx)
{
	if(noMatrix(y, cx, INVERTC)) return;
	Pmatrix x= toMatrix(cx);
	if(!isSquareM(x)) return;
	int n= x->len;
	TRANSPM(cx);
	Complex t= ALLOCR(getPrecision(y));
	prepareM(t, x);
	SETDIAGM(t, 1);
	CONCATROWM(y, cx, t);
	TRANSPM(y);
	ELIMM(y);
	TRANSPM(y);
	x= toMatrix(y);
	if(!x->len || x->A[n-1].isZero()){
		cerror(1050, "Matrix is not regular");
	}
	x->rows -= x->cols;
	x->len -= n;
	for(int i=0; i<n; i++){
		x->A[i].free();
	}
	memmove(x->A, x->A + n, x->len*sizeof(MatrixItem));
	TRANSPM(y);
	FREEM(t);
}

void check1x1(Complex &cx)
{
	Pmatrix x= toMatrix(cx);
	if(isMatrix(cx) && x->len==1){
		ComplexItem w(x->A[0]);
		x->len=0;
		COPYM(cx, w);
		w.free();
	}
}

void _fastcall EQUSOLVEM(Complex &cx)
{
	int r, c, n, i;
	MatrixItem *p, *d, *A;

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
		if(r>=x->rows || p->isZero()){
			d->set(0);
		}
		else{
			r++;
			copyItem(*d, A[r*n-1]);
		}
	}
	for(; r<x->rows; r++){
		if(!A[(r+1)*n-1].isZero()){
			cerror(1055, "Equations are not solvable");
		}
	}
	n--;
	x->rows= 1;
	x->cols= n;
	for(i=n; i<x->len; i++){
		A[i].free();
	}
	x->len= n;
	check1x1(cx);
}

void _stdcall DIVM(Complex &y, const Complex &a, Complex &b)
{
	if(noMatrix(y, a, b, DIVC)) return;
	Complex t= ALLOCR(getPrecision(y));
	INVERTM(t, b);
	MULTM(y, a, t);
	FREEM(t);
}


void _fastcall ABSM(Complex &x)
{
	if(noMatrix(x, ABSC)) return;
	Complex y= ALLOCR(getPrecision(x));
	SUM2M(y, x);
	matrixToComplex(x);
	SQRTC(x, y);
	FREEC(y);
}

void _stdcall MATRIXM(Complex &y, const Complex &ca, const Complex &cb)
{
	if(isInt(ca) && isInt(cb)){
		Tint r = toInt(ca.r);
		Tint c = toInt(cb.r);
		if(r>0 && c>0 || r==0 && c==0){
			if(r<=MatrixMaxLen && c<=MatrixMaxLen && Int32x32To64(r, c)<=MatrixMaxLen){
				prepareM(y, (int)c, (int)r);
				return;
			}
			cerror(1066, "Matrix has too large dimensions");
		}
	}
	cerror(1059, "Function matrix must have parameters number of rows, number of columns");
}


void _stdcall ANGLEM(Complex &y, const Complex &ca, const Complex &cb)
{
	matrixToComplex(y);
	if(!isMatrix(ca) && !isMatrix(cb)){
		if(isImag(ca) || isImag(cb)) errImag();
		ATAN2X(y.r, ca.r, cb.r);
		ZEROX_safe(y.i);
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
	t=ALLOCR(prec);
	u=ALLOCR(prec);
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
void _stdcall VERTM(Complex &y, const Complex &ca, const Complex &cb)
{
	Pmatrix a=toMatrix(ca), b=toMatrix(cb);
	if(!isVector(ca) || !isVector(cb) || a->len!=3 || b->len!=3){
		cerror(1049, "Arguments have to be vectors of length 3");
		return;
	}
	prepareM(y, a);
	MatrixItem *Y, *A, *B;
	Complex t, u, v;
	Y= toMatrix(y)->A;
	A= a->A;
	B= b->A;
	Tint p = getPrecision(y);
	t=ALLOCR(p);
	u=ALLOCR(p);
	v=ALLOCR(p);
	int i, j, k;
	for(k=0; k<3; k++){
		i=(k+1)%3;
		j=(k+2)%3;
		ComplexItem ai(A[i]), bj(B[j]), aj(A[j]), bi(B[i]);
		MULTC(t, ai, bj);
		MULTC(u, aj, bi);
		MINUSC(v, t, u);
		copyItem(Y[k], v);
	}
	FREEC(v);
	FREEC(u);
	FREEC(t);
}


void _stdcall POLYNOMM(Complex &y, const Complex &cx, const Complex &cp)
{
	int i, j, n=1;
	Complex t, u;
	MatrixItem *m, *d, *s;

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
	t=ALLOCR(prec);
	u=ALLOCR(prec);
	for(m=&p->A[p->len-1]; m>=p->A; m--){
		ComplexItem cm(*m);
		MULTM(t, y, cx);
		if(n>1){
			s= toMatrix(t)->A;
			prepareM(y, x);
			d= toMatrix(y)->A;
			for(i=0; i<n; i++){
				for(j=0; j<n; j++){
					ComplexItem cs(*s);
					if(i==j){
						PLUSC(u, cs, cm);
						copyItem(*d, u);
					}
					else{
						copyItem(*d, cs);
					}
					s++;
					d++;
				}
			}
		}
		else{
			PLUSC(y, t, cm);
		}
	}
	FREEC(u);
	FREEM(t);
}
//-------------------------------------------------------------------

void _fastcall MAPM(Complex &cx, TunaryC0 f)
{
	if(noMatrix(cx, f)) return;
	Pmatrix x= toMatrix(cx);
	Complex t= ALLOCR(getPrecision(cx));
	for(int i=0; i<x->len; i++){
		ComplexItem xi(x->A[i]);
		COPYC(t, xi);
		f(t);
		copyItem(x->A[i], t);
	}
	FREEC(t);
}

void _fastcall MAPM(Complex &cy, const Complex &cx, TunaryC2 f)
{
	if(noMatrix(cy, cx, f)) return;
	Pmatrix x= toMatrix(cx), y=toMatrix(cy);
	prepareM(cy, x);
	Complex z = ALLOCR(getPrecision(cy));
	for(int i=0; i < y->len; i++){
		ComplexItem t(x->A[i]);
		f(z, t);
		copyItem(y->A[i], z);
	}
	FREEC(z);
}

void _stdcall MAPM(Complex &cx, Tuint n, TbinaryCI0 f)
{
	if(!isMatrix(cx)){
		f(cx, n);
	}
	else{
		Pmatrix x= toMatrix(cx);
		Complex t= ALLOCR(getPrecision(cx));
		for(int i=0; i<x->len; i++){
			ComplexItem xi(x->A[i]);
			COPYC(t, xi);
			f(t, n);
			copyItem(x->A[i], t);
		}
		FREEC(t);
	}
}

void _stdcall MAPM(Complex &cy, const Complex &cx, Tuint n, TbinaryCI2 f)
{
	if(!isMatrix(cx)){
		matrixToComplex(cy);
		f(cy, cx, n);
	}
	else{
		Pmatrix x= toMatrix(cx), y=toMatrix(cy);
		prepareM(cy, x);
		Complex z= ALLOCR(getPrecision(cy));
		for(int i=0; i<x->len; i++){
			ComplexItem xi(x->A[i]);
			f(z, xi, n);
			copyItem(y->A[i], z);
		}
		FREEC(z);
	}
}

void _stdcall MAP1M(Complex &cy, const Complex &ca, const Complex &b, TbinaryC f)
{
	if(noMatrix(cy, ca, b, f)) return;
	if(isMatrix(b)){ errMatrix(); return; }
	Pmatrix a=toMatrix(ca), y=toMatrix(cy);
	prepareM(cy, a);
	Complex z = ALLOCR(getPrecision(cy));
	for(int i=0; i<y->len; i++){
		ComplexItem ai(a->A[i]);
		f(z, ai, b);
		copyItem(y->A[i], z);
	}
	FREEC(z);
}

void _stdcall MAPM(Complex &cy, const Complex &ca, const Complex &cb, TbinaryC f)
{
	if(noMatrix(cy, ca, cb, f)) return;
	if(!sameSizeM(ca, cb)) return;
	Pmatrix a=toMatrix(ca), b=toMatrix(cb), y=toMatrix(cy);
	prepareM(cy, a);
	Complex z = ALLOCR(getPrecision(cy));
	for(int i=0; i<y->len; i++){
		ComplexItem ta(a->A[i]), tb(b->A[i]);
		f(z, ta, tb);
		copyItem(y->A[i], z);
	}
	FREEC(z);
}

void _stdcall COPYM(Complex &dest, const Complex &src)
{
	if(!isMatrix(src))
	{
		matrixToComplex(dest);
		COPYC(dest, src);
		return;
	}
	if(src.r==dest.r) return;
	Pmatrix x= toMatrix(src), y=toMatrix(dest);
	prepareM(dest, x);
	Tint prec = getPrecision(dest);
	for(int i=0; i < y->len; i++){
		copyItem(y->A[i], x->A[i], prec);
	}
}

void _stdcall PLUSM(Complex &y, const Complex &a, const Complex &b)
{
	MAPM(y, a, b, PLUSC);
}
void _stdcall MINUSM(Complex &y, const Complex &a, const Complex &b)
{
	MAPM(y, a, b, MINUSC);
}
void _stdcall MULTCM(Complex &y, const Complex &a, const Complex &i)
{
	MAP1M(y, a, i, MULTC);
}
void _stdcall MULTIM(Complex &y, const Complex &x, Tuint n)
{
	MAPM(y, x, n, MULTIC);
}
void _stdcall DIVIM(Complex &y, const Complex &x, Tuint n)
{
	MAPM(y, x, n, DIVIC);
}
void _stdcall MULTI1M(Complex &x, Tuint n)
{
	MAPM(x, n, MULTI1C);
}
void _stdcall DIVI1M(Complex &x, Tuint n)
{
	MAPM(x, n, DIVI1C);
}
void _stdcall LSHIM(Complex &y, const Complex &a, int n)
{
	MAPM(y, a, n, (TbinaryCI2)LSHIC);
}

void _fastcall NEGM(Complex &cx)
{
	if(noMatrix(cx, NEGC)) return;
	Pmatrix x= toMatrix(cx);
	for(int i=0; i<x->len; i++){
		MatrixItem &m = x->A[i];
		if(m.denominator) m.denominator = -m.denominator;
		else NEGX(m.r);
		if(m.i) NEGX(m.i);
	}
}

void _fastcall CONJGM(Complex &x)
{
	MAPM(x, CONJGC);
}
void _fastcall REALM(Complex &x)
{
	MAPM(x, REALC);
}
void _fastcall IMAGM(Complex &x)
{
	MAPM(x, IMAGC);
}
void _fastcall ROUNDM(Complex &x)
{
	MAPM(x, ROUNDC);
}
void _fastcall TRUNCM(Complex &x)
{
	MAPM(x, TRUNCC);
}
void _fastcall INTM(Complex &x)
{
	MAPM(x, INTC);
}
void _fastcall CEILM(Complex &x)
{
	MAPM(x, CEILC);
}
void _fastcall FRACM(Complex &x)
{
	MAPM(x, FRACC);
}
void _stdcall NOTM(Complex &y, const Complex &x)
{
	MAPM(y, x, NOTC);
}
void _stdcall ANDM(Complex &y, const Complex &a, const Complex &b)
{
	MAPM(y, a, b, ANDC);
}
void _stdcall ORM(Complex &y, const Complex &a, const Complex &b)
{
	MAPM(y, a, b, ORC);
}
void _stdcall NANDBM(Complex &y, const Complex &a, const Complex &b)
{
	MAPM(y, a, b, NANDBC);
}
void _stdcall NORBM(Complex &y, const Complex &a, const Complex &b)
{
	MAPM(y, a, b, NORBC);
}
void _stdcall XORM(Complex &y, const Complex &a, const Complex &b)
{
	MAPM(y, a, b, XORC);
}
void _stdcall IMPBM(Complex &y, const Complex &a, const Complex &b)
{
	MAPM(y, a, b, IMPBC);
}
void _stdcall EQVBM(Complex &y, const Complex &a, const Complex &b)
{
	MAPM(y, a, b, EQVBC);
}
void _stdcall RSHM(Complex &y, const Complex &a, const Complex &b)
{
	MAP1M(y, a, b, RSHC);
}
void _stdcall RSHIM(Complex &y, const Complex &a, const Complex &b)
{
	MAP1M(y, a, b, RSHIC);
}
void _stdcall LSHM(Complex &y, const Complex &a, const Complex &b)
{
	MAP1M(y, a, b, LSHC);
}
//-------------------------------------------------------------------

void _stdcall MINMAXM(Complex &y, const Complex &cx, int desc)
{
	if(noMatrixOrEmpty(y, cx, COPYC)) return;
	Pmatrix x= toMatrix(cx);
	MatrixItem *A= x->A;
	int num= x->len;
	ComplexItem z(A[--num]);
	while(num--){
		ComplexItem u(A[num]);
		if(CMPC(z, u)*desc > 0)  z.set(u);
	}
	COPYM(y, z);
}

void _stdcall MINM(Complex &y, const Complex &cx)
{
	MINMAXM(y, cx, 1);
}

void _stdcall MAXM(Complex &y, const Complex &cx)
{
	MINMAXM(y, cx, -1);
}

void _stdcall MIN3M(Complex &y, const Complex &a, const Complex &b)
{
	if(isMatrix(a) || isMatrix(b)){
		errMatrix();
		return;
	}
	COPYC(y, CMPC(a, b)<0 ? a : b);
}

void _stdcall MAX3M(Complex &y, const Complex &a, const Complex &b)
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

int _stdcall SUMM(Complex &y0, const Complex &cx, int start, int step)
{
	int i, count=1;
	Complex t, y;

	if(step==2) checkLR(cx);
	if(noMatrixOrEmpty(y0, cx, COPYC)) return 1;
	Pmatrix x= toMatrix(cx);
	MatrixItem *A= x->A;
	int num= x->len;
	matrixToComplex(y0);
	y=y0;
	t=ALLOCR(y.r[-4]);
	ComplexItem a1(A[start]);
	COPYC(y, a1);
	for(i=start+step; i<num; i+=step){
		ComplexItem ai(A[i]);
		PLUSC(t, y, ai);
		std::swap(t, y);
		count++;
	}
	if(y.r!=y0.r){ COPYC(y0, y); t=y; }
	FREEC(t);
	return count;
}

void _stdcall SUM1M(Complex &y, const Complex &x)
{
	SUMM(y, x, 0, 1);
}

void _stdcall SUMXM(Complex &y, const Complex &x)
{
	SUMM(y, x, 0, 2);
}

void _stdcall SUMYM(Complex &y, const Complex &x)
{
	SUMM(y, x, 1, 2);
}

int _stdcall SUMMULM(Complex &y0, const Complex &cx, int start, int step, int diff=0)
{
	int i, count=0;
	Complex t, y, u;

	if(step==2) checkLR(cx);
	if(noMatrixOrEmpty(y0, cx, SQRC)) return 1;
	Pmatrix x= toMatrix(cx);
	MatrixItem *A= x->A;
	int num= x->len;
	matrixToComplex(y0);
	y=y0;
	ZEROC(y);
	t=ALLOCR(y.r[-4]);
	u=ALLOCR(y.r[-4]);
	for(i=start; i<num; i+=step){
		ComplexItem ai(A[i]);
		if(diff) {
			ComplexItem ad(A[i+diff]);
			MULTC(u, ai, ad);
		}
		else {
			SQRC(u, ai);
		}
		PLUSC(t, y, u);
		std::swap(t, y);
		count++;
	}
	if(y.r!=y0.r){ COPYC(y0, y); t=y; }
	FREEC(u);
	FREEC(t);
	return count;
}

void _stdcall SUM2M(Complex &y, const Complex &x)
{
	SUMMULM(y, x, 0, 1);
}

void _stdcall SUMX2M(Complex &y, const Complex &x)
{
	SUMMULM(y, x, 0, 2);
}

void _stdcall SUMY2M(Complex &y, const Complex &x)
{
	SUMMULM(y, x, 1, 2);
}

int _stdcall SUMXYM(Complex &y, const Complex &x)
{
	return SUMMULM(y, x, 0, 2, 1);
}

//ave=sum/n
void _stdcall AVEM(Complex &y, const Complex &cx, int start, int step)
{
	DIVI1C(y, SUMM(y, cx, start, step));
}

void _stdcall AVE1M(Complex &y, const Complex &x)
{
	AVEM(y, x, 0, 1);
}

void _stdcall AVEXM(Complex &y, const Complex &x)
{
	AVEM(y, x, 0, 2);
}

void _stdcall AVEYM(Complex &y, const Complex &x)
{
	AVEM(y, x, 1, 2);
}

void _stdcall AVEMULM(Complex &y, const Complex &cx, int start, int step, int diff=0)
{
	DIVI1C(y, SUMMULM(y, cx, start, step, diff));
}

void _stdcall AVE2M(Complex &y, const Complex &x)
{
	AVEMULM(y, x, 0, 1);
}

void _stdcall AVEX2M(Complex &y, const Complex &x)
{
	AVEMULM(y, x, 0, 2);
}

void _stdcall AVEY2M(Complex &y, const Complex &x)
{
	AVEMULM(y, x, 1, 2);
}

//var=(sumq-sum^2/n)/(n-sample)
void _stdcall VARM(Complex &y, const Complex &cx, unsigned sample, int start, int step, bool sqrt=false)
{
	Complex t, u;

	if(step==2) checkLR(cx);
	matrixToComplex(y);
	if(!isMatrix(cx)){
		ZEROC(y);
		return;
	}
	t=ALLOCR(y.r[-4]);
	u=ALLOCR(y.r[-4]);
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

void _stdcall VAR0M(Complex &y, const Complex &x)
{
	VARM(y, x, 0, 0, 1);
}

void _stdcall VAR1M(Complex &y, const Complex &x)
{
	VARM(y, x, 1, 0, 1);
}

void _stdcall VARX0M(Complex &y, const Complex &x)
{
	VARM(y, x, 0, 0, 2);
}

void _stdcall VARX1M(Complex &y, const Complex &x)
{
	VARM(y, x, 1, 0, 2);
}

void _stdcall VARY0M(Complex &y, const Complex &x)
{
	VARM(y, x, 0, 1, 2);
}

void _stdcall VARY1M(Complex &y, const Complex &x)
{
	VARM(y, x, 1, 1, 2);
}

void _stdcall STDEV0M(Complex &y, const Complex &x)
{
	VARM(y, x, 0, 0, 1, true);
}

void _stdcall STDEV1M(Complex &y, const Complex &x)
{
	VARM(y, x, 1, 0, 1, true);
}

void _stdcall STDEVX0M(Complex &y, const Complex &x)
{
	VARM(y, x, 0, 0, 2, true);
}

void _stdcall STDEVX1M(Complex &y, const Complex &x)
{
	VARM(y, x, 1, 0, 2, true);
}

void _stdcall STDEVY0M(Complex &y, const Complex &x)
{
	VARM(y, x, 0, 1, 2, true);
}

void _stdcall STDEVY1M(Complex &y, const Complex &x)
{
	VARM(y, x, 1, 1, 2, true);
}


//b=(n*sumxy-sumx*sumy)/(n*sumxq-sumx^2)
//a=(sumy-b*sumx)/n
void _stdcall LRABM(Complex &a, Complex &b, const Complex &x)
{
	Complex t, u, v, sx, sy;
	int n;

	Tint prec=getPrecision(b);
	t=ALLOCR(prec);
	u=ALLOCR(prec);
	v=ALLOCR(prec);
	sx=ALLOCR(prec);
	sy=ALLOCR(prec);
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

void _stdcall LRAM(Complex &y, const Complex &x)
{
	Complex b=ALLOCR(getPrecision(y));
	LRABM(y, b, x);
	FREEC(b);
}

void _stdcall LRBM(Complex &y, const Complex &x)
{
	Complex a;
	a.r=0;
	LRABM(a, y, x);
}

//x=(y-a)/b
void _stdcall LRXM(Complex &x, const Complex &d, const Complex &y)
{
	Complex a, b;

	matrixToComplex(x);
	Tint prec=getPrecision(y);
	a=ALLOCR(prec);
	b=ALLOCR(prec);
	LRABM(a, b, d);
	MINUSC(x, y, a);
	DIVC(a, x, b);
	COPYC(x, a);
	FREEC(b);
	FREEC(a);
}

//y=a+b*x
void _stdcall LRYM(Complex &y, const Complex &d, const Complex &x)
{
	Complex a, b;

	matrixToComplex(y);
	Tint prec=getPrecision(y);
	a=ALLOCR(prec);
	b=ALLOCR(prec);
	LRABM(a, b, d);
	MULTC(y, b, x);
	PLUSC(b, a, y);
	COPYC(y, b);
	FREEC(b);
	FREEC(a);
}

//r=(n*sumxy-sumx*sumy)/sqrt((n*sumxq-sumx^2)*(n*sumyq-sumy^2))
void _stdcall LRRM(Complex &y, const Complex &x)
{
	Complex t, u, sx, sy;
	int n;

	Tint prec=getPrecision(y);
	t=ALLOCR(prec);
	u=ALLOCR(prec);
	sx=ALLOCR(prec);
	sy=ALLOCR(prec);
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

void _stdcall HARMONM(Complex &y0, const Complex &cx)
{
	int i;
	Complex t, u, y;

	if(noMatrixOrEmpty(y0, cx, COPYC)) return;
	Pmatrix x= toMatrix(cx);
	MatrixItem *A= x->A;
	int num= x->len;
	matrixToComplex(y0);
	Tint prec=getPrecision(y0);
	y=y0;
	t=ALLOCR(prec);
	u=ALLOCR(prec);
	ZEROC(y);
	for(i=0; i<num; i++){
		ComplexItem a(A[i]);
		INVERTC(u, a);
		PLUSC(t, y, u);
		std::swap(t, y);
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

int _stdcall PRODUCTM(Complex &y0, const Complex &cx)
{
	int i, count=2;
	Complex t, y;

	if(noMatrixOrEmpty(y0, cx, COPYC)) return 1;
	Pmatrix x= toMatrix(cx);
	MatrixItem *A= x->A;
	int num= x->len;
	matrixToComplex(y0);
	y=y0;
	t=ALLOCR(y.r[-4]);
	ComplexItem a0(A[0]), a1(A[1]);
	MULTC(y, a0, a1);
	for(i=2; i<num; i++){
		ComplexItem ai(A[i]);
		MULTC(t, y, ai);
		std::swap(t, y);
		count++;
	}
	if(y.r!=y0.r){ COPYC(y0, y); t=y; }
	FREEC(t);
	return count;
}


//geom=product^(1/n)
void _stdcall GEOMM(Complex &y, const Complex &cx)
{
	Complex t, n;
	n=ALLOCR(2);
	t=ALLOCR(getPrecision(y));
	SETC(n, PRODUCTM(t, cx));
	ROOTC(y, n, t);
	FREEC(t);
	FREEC(n);
}


void _stdcall REPEATOPX(Tbinary f, Complex &y, const Complex &cx, int errId, char *errStr)
{
	int i;
	Pint t, y0;

	if(noMatrixOrEmpty(y, cx, COPYC)) return;
	Pmatrix x= toMatrix(cx);
	MatrixItem *A= x->A;
	int num= x->len;
	for(i=0; i<num; i++){
		if(!isZero_safe(A[i].i)){
			cerror(errId, errStr);
			return;
		}
	}
	y0=y.r;
	t=ALLOCX(y0[-4]);
	ZEROX_safe(y.i);
	ComplexItem a0(A[0]), a1(A[1]);
	f(y0, a0.c.r, a1.c.r);
	for(i=2; i<num; i++){
		ComplexItem ai(A[i]);
		f(t, y0, ai.c.r);
		std::swap(t, y0);
	}
	if(y.r!=y0){
		COPYX(y.r, y0);
		t=y0;
	}
	FREEX(t);
}

void _stdcall GCDM(Complex &y, const Complex &cx)
{
	REPEATOPX(GCDX, y, cx, 1026, "The greatest common divisor of complex numbers");
}

void _stdcall LCMM(Complex &y, const Complex &cx)
{
	REPEATOPX(LCMX, y, cx, 1027, "The least common multiple of complex numbers");
}

int __cdecl SORTCMP(const void *elem1, const void *elem2)
{
	ComplexItem c1(*(const MatrixItem*)elem1), c2(*(const MatrixItem*)elem2);
	return CMPC(c1, c2);
}

int __cdecl SORTDCMP(const void *elem1, const void *elem2)
{
	return SORTCMP(elem2, elem1);
}

void _fastcall SORTM(Complex &cx)
{
	if(!isMatrix(cx)) return;
	Pmatrix x= toMatrix(cx);
	qsort(x->A, x->len, sizeof(MatrixItem), SORTCMP);
}

void _fastcall SORTDM(Complex &cx)
{
	if(!isMatrix(cx)) return;
	Pmatrix x= toMatrix(cx);
	qsort(x->A, x->len, sizeof(MatrixItem), SORTDCMP);
}

void _fastcall REVERSEM(Complex &cx)
{
	if(!isMatrix(cx)) return;
	Pmatrix x= toMatrix(cx);
	MatrixItem *b, *e;
	if(x->len)
		for(b=x->A, e=&x->A[x->len-1]; b<e; b++, e--){
			std::swap(*b, *e);
		}
}

//warning: cx will be sorted
void _stdcall MEDIANM(Complex &y, Complex &cx)
{
	if(noMatrixOrEmpty(y, cx, COPYC)) return;
	Pmatrix x= toMatrix(cx);
	int num= x->len;
	MatrixItem *A= x->A;
	matrixToComplex(y);
	SORTM(cx);
	if(num&1){
		ComplexItem a(A[num>>1]);
		COPYC(y, a);
	}
	else{
		ComplexItem a1(A[num>>1]), a2(A[(num>>1)-1]);
		PLUSC(y, a1, a2);
		DIVI1C(y, 2);
	}
}

//warning: cx will be sorted
void _stdcall MODEM(Complex &y, Complex &cx)
{
	int i, j, m, im=0;

	if(noMatrixOrEmpty(y, cx, COPYC)) return;
	Pmatrix x= toMatrix(cx);
	MatrixItem *A= x->A;
	ComplexItem first(A[0]);
	SORTM(cx);
	m=1;
	j=1;
	for(i=1; i<x->len; i++){
		ComplexItem a0(A[i-1]), a1(A[i]);
		if(CMPC(a0, a1)==0){
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

struct IntegralData
{
	Complex var;
	Tcompiled *jit;
	int bits, depth;

	/*
	u= (F[0] + 4*F[1] + F[2])/2
	y= (F[0] + 4*f(a+h/2) + 2*F[1] + 4*f(a+h*3/2) + F[2])/4
	*/
	void integralRecurse(Complex &y, Complex *F0, Complex &a, Complex &h0, int finalDepth)
	{
		Complex t, u, *v, h, oldV, F[5];
		int i;
		const int D=3;

		depth++;
		F[1].r=F[1].i=F[3].r=F[3].i=0;
		Tint prec=getPrecision(y);
		Pint mem=ALLOCN(3, prec, &t.r, &u.r, &h.r);
		t.i = u.i = h.i = 0;
		DIVIM(h, h0, 2);
		PLUSM(t, a, h);
		PLUSM(u, t, h0);
		v=deref1(var);
		oldV=*v;
		//function values
		for(i=1; i<=3; i+=2) {
			*v= (i==1) ? t : u;
			if(error) goto lfree;
			jitRun(jit);
			if(error) goto lfree;
			F[i]=*numStack--;
			deref(F[i]);
			//logx("integral x", v->r);
			//logx("integral depth %d: F[%d]", F[i].r, depth, i);
		}
		F[0]=F0[0];
		F[2]=F0[1];
		F[4]=F0[2];
		//y= F[0]/4 + F[1] + F[2]/2 + F[3] + F[4]/4
		PLUSM(t, F[0], F[4]);
		DIVI1M(t, 2);
		PLUSM(y, t, F[2]);
		DIVI1M(y, 2);
		PLUSM(u, y, F[1]);
		PLUSM(y, u, F[3]);
		//u= F[0]/2 + 2*F[2] + F[4]/2
		DIVI1M(t, 2);
		PLUSM(u, t, F[2]);
		MULTI1M(u, 2);

		if(finalDepth>D) finalDepth++;
		else {
			//difference
			MINUSM(t, u, y);
			LSHIM(u, y, (D-finalDepth)*4 - bits);
			ABSM(u);
			ABSM(t);
			if(CMPC(t, u)<=0) finalDepth++;
			else if(finalDepth>0) finalDepth--;
		}
		//recurse
		if(finalDepth<=D && !error && depth<50) {
			PLUSM(t, a, h0);
			integralRecurse(u, F+2, t, h, finalDepth);
			integralRecurse(t, F, a, h, finalDepth);
			PLUSM(y, u, t);
			DIVI1M(y, 2);
		}
	lfree:
		*v=oldV;
		FREEM(F[3]);
		FREEM(F[1]);
		FREE_ARRAYM(t);
		FREE_ARRAYM(u);
		FREE_ARRAYM(h);
		FREEX(t.i);
		FREEX(u.i);
		FREEX(h.i);
		FREEX(mem);
		depth--;
	}

	//integral= (b-a)/3 * integralRecurse
	void integral(Complex &y, Complex &a, Complex &b)
	{
		Complex h, z, F[3];
		int i;
		Tint oldyPrec, oldPrec;

		oldPrec=precision;
		oldyPrec=y.r[-4];
#ifdef ARIT64
		const int B = 39; //12 digits
#else
		const int B = 26; //8 digits
#endif
		y.r[-4]= precision= (bits<=B) ? 2 : min(oldyPrec, (bits+92*(TintBits/32))/TintBits);
		memset(F, 0, sizeof(F));
		h=ALLOCR(precision);
		z=ALLOCR(precision);
		PLUSM(h, a, b);
		DIVI1M(h, 2);
		//function values
		for(i=0; i<3; i++) {
			ASSIGNM(y, var, (i==0 ? a : (i==1 ? h : b)));
			jitRun(jit);
			if(error) goto lfree;
			F[i]=*numStack--;
			deref(F[i]);
		}
		MINUSM(h, b, a);
		DIVI1M(h, 2);

		integralRecurse(y, F, a, h, 0);

		MULTI1M(h, 2);
		if(isMatrix(h)) ABSM(h);
		MULTM(z, y, h);
		DIVIM(y, z, 3);
	lfree:
		FREEM(z);
		FREEM(h);
		FREEM(F[2]);
		FREEM(F[1]);
		FREEM(F[0]);
		precision=oldPrec;
		y.r[-4]=oldyPrec;
	}
};

void INTEGRALM(Complex &y, Complex &a, Complex &b, Complex &p, Complex &var, Tcompiled *jit)
{
	Tuint timeOrPrec;
	DWORD t0, t1, t2;
	IntegralData data;

	if(isMatrix(p) || isImag(p) || !isDword(p.r)){
		cerror(1058, "The fourth argument has to be integer");
		return;
	}
	data.jit=jit;
	data.var=var;
	data.depth=0;
	timeOrPrec= toDword(p.r);
	if(timeOrPrec<100){
		data.bits=int(timeOrPrec*3.322);
		data.integral(y, a, b);
	}
	else{
		t0=t2=GetTickCount();
		for(data.bits=16; data.bits<96; data.bits+=8){
			t1=t2;
			data.integral(y, a, b);
			t2=GetTickCount();
			if((t2-t0)+(t2-t1)*4 >(timeOrPrec/2) || error) break;
		}
	}
}
