/*
	(C) Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "arit.h"
#if GMP
// The GNU Multiple Precision Arithmetic Library
// https://github.com/BrianGladman/mpir
#include "gmp.h"

static HANDLE gmpThread;
static HANDLE gmpEventStart;
static HANDLE gmpEventDone;
static void (*gmpFunc)();
static void *gmpParam;
HANDLE gmpHeap;

void ConvertFromGMP(Pint x, mpf_ptr f)
{
	x[-1]= f->_mp_exp;
	x[-2]= f->_mp_size < 0;
	int n = abs(f->_mp_size);
	assert(n<=x[-4]+2);
	x[-3]=min(n,x[-4]);
	for(int i=--n; i>=0; i--)
		x[i]=f->_mp_d[n-i]; //reverse
}

void ConvertToGMP(Pint x, mpf_ptr f)
{
	//import is safe and compatible but it is slower and needs more memory
	//mpz_t o;
	//mpz_init(o);
	//mpz_import(o, x[-3], 1, sizeof(Tint), 0, 0, x);
	//mpf_init2(f, precision*TintBits);
	//mpf_set_z(f, o);
	//mp_bitcnt_t b = x[-1]-x[-3];
	//if(b) mpf_mul_2exp(f, f, b*64);
	//mpz_clear(o);

	//direct conversion to mpf_t
	mpf_init2(f, precision*TintBits);
	f->_mp_exp= (mp_exp_t)x[-1];
	int n = f->_mp_size = (int)x[-3];
	for(int i=--n; i>=0; i--)
		f->_mp_d[i]=x[n-i]; //reverse
}

static void NotEnoughMemory()
{
	HeapDestroy(gmpHeap);
	gmpHeap = 0;
	cerror(1028, "Not enough memory !!!");
#ifndef CONSOLE
	SetEvent(gmpEventDone);
	CloseHandle(gmpThread);
	gmpThread = 0;
	ExitThread(99);
#else
	ExitProcess(99);
#endif
}

static void InitHeap()
{
	if(!gmpHeap) gmpHeap = HeapCreate(0, 0, 0);
}

void* gmp_alloc(size_t size)
{
	InitHeap();
	void* p = HeapAlloc(gmpHeap, 0, size);
	if (!p) NotEnoughMemory();
	return p;
}

void* gmp_realloc(void* ptr, size_t, size_t new_size)
{
	InitHeap();
	void* p = HeapReAlloc(gmpHeap, 0, ptr, new_size);
	if (!p) NotEnoughMemory();
	return p;
}

void gmp_free(void* ptr, size_t)
{
	HeapFree(gmpHeap, 0, ptr);
}

void gmp_Init()
{
	mp_set_memory_functions(gmp_alloc, gmp_realloc, gmp_free);
	gmpEventStart = CreateEvent(NULL, FALSE, FALSE, NULL);
	gmpEventDone = CreateEvent(NULL, FALSE, FALSE, NULL);
}

void gmp_Stop()
{
	if(gmpThread) {
		TerminateThread(gmpThread, 0);
		SetEvent(gmpEventDone);
		CloseHandle(gmpThread);
		gmpThread = 0;
	}
	if(gmpHeap) {
		HeapDestroy(gmpHeap);
		gmpHeap = 0;
	}
}

static DWORD WINAPI GmpThreadProc(LPVOID)
{
	for(;;) {
		WaitForSingleObject(gmpEventStart, INFINITE);
		gmpFunc();
		SetEvent(gmpEventDone);
	}
}

static void RunOnGmpThread(void (*func)())
{
	if(error) return;
	if(!gmpThread) {
		ResetEvent(gmpEventDone);
		gmpThread = CreateThread(NULL, 0, GmpThreadProc, NULL, 0, NULL);
	}
	gmpFunc = func;
	SetEvent(gmpEventStart);
	WaitForSingleObject(gmpEventDone, INFINITE);
}

static const char* READX_GMP_Impl(Pint x, const char *buf)
{
	const char *s;
	bool dot=false;
	for(s=buf;; s++) {
		char c=*s;
		if(!(c>='0' && c<='9' && c<'0'+baseIn || c>='A' && c<'A'-10+baseIn || c>='a' && c<'a'-10+baseIn)) {
			if(c=='.' && !dot) dot=true;
			else break;
		}
	}
	size_t len=s-buf;
	if(len<21 || !dot && len<250) return 0; //don't use GMP for small numbers
	
	//GMP needs null-terminated string
	char* buf2=(char*)gmp_alloc(len+1);
	memcpy(buf2, buf, len);
	buf2[len]=0;

	mpf_t f;
	mpf_init2(f, x[-4]*TintBits);
	int err=mpf_set_str(f, buf2, baseIn);
	assert(!err);
	gmp_free(buf2, len+1);
	if(!err) ConvertFromGMP(x, f);
	mpf_clear(f);
	return err ? 0 : s;
}

struct ReadGmpParams { Pint x; const char *buf; const char *result; };

static void READX_GMP_Work()
{
	ReadGmpParams *params = (ReadGmpParams*)gmpParam;
	params->result = READX_GMP_Impl(params->x, params->buf);
}

const char* _stdcall READX_GMP(Pint x, const char *buf)
{
	ReadGmpParams params = { x, buf, 0 };
	gmpParam = &params;
	RunOnGmpThread(READX_GMP_Work);
	return params.result;
}

//returns 32bit exponent, output buffer must be large enough
static mp_exp_t WRITEX_GMP_Impl(char *buf, Pint x, int _digits)
{
	if(x[-2]) *buf++='-'; //negative number

	mpf_t f;
	ConvertToGMP(x, f);
	mp_exp_t e;
	mpf_get_str(buf+1, &e, -base, _digits, f);
	mpf_clear(f);

	if(e>-20 && e<=_digits && (numFormat==MODE_NORM || numFormat==MODE_FIX))
	{
		//number is without exponent
		size_t len=strlen(buf+1);
		if(e>0)
		{
			memmove(buf, buf+1, min(len,e));
			//fill trailing zeros
			if(len<e) memset(buf+len, '0', e-len);
			buf[e]= len>e ? '.' : 0;
		}
		else {
			memmove(buf+2-e, buf+1, len+1);
			//fill leading zeros
			memset(buf, '0', 2-e);
			buf[1]='.';
		}
		return 0;
	}
	else {
		//insert dot after the first digit
		buf[0]=buf[1];
		buf[1]= buf[2] ? '.' : 0;
		e--;

		if(numFormat==MODE_ENG) {
			int m = e%3;
			if(m) {
				if(m<0) m+=3;
				if(!buf[2]) {
					buf[2]='0'; buf[3]=0;
				}
				if(!buf[3] && m==2) {
					buf[3]='0'; buf[4]=0;
				}
				buf[1]=buf[2];
				buf[2]=buf[3];
				buf[m+1]='.';
				e-=m;
			}
		}
		return e; //exponent
	}
}

struct TWriteGmpParams { char *buf; Pint x; int digits; mp_exp_t result; } WriteGmpParams;

static void WRITEX_GMP_Work()
{
	TWriteGmpParams &p = WriteGmpParams;
	p.result = WRITEX_GMP_Impl(p.buf, p.x, p.digits);
}

__int64 _stdcall WRITEX_GMP(char *buf, Pint x, int _digits)
{
	WriteGmpParams = { buf, x, _digits, 0 };
	RunOnGmpThread(WRITEX_GMP_Work);
	return (__int64)WriteGmpParams.result;
}

#endif
