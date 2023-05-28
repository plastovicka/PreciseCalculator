/*
(C) Petr Lastovicka

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License.
*/
#include "hdr.h"
#ifdef CONSOLE
#include "preccalc.h"

#pragma comment(lib,"version.lib")

char *lng(int, char *s)
{
	return s;
}

bool isParam(char* &s0, char c, char *name)
{
	char *s=s0;
	if(*s==c && !isLetter(s[1])) {
		s0=s+1;
		return true;
	}
	if(*s=='-') s++;
	int len = strleni(name);
	if(!_strnicmp(s, name, len) && !isLetter(s[len])) {
		s0=s+len;
		return true;
	}
	return false;
}

bool isParam(char* &s, char c, char *name, int &value, int minval, int maxval)
{
	if(!isParam(s, c, name)) return false;
	value=strtol(s, &s, 10);
	if(value<minval || value>maxval || *s!=' ' && *s) {
		printf("Invalid value of the parameter %s\n", name);
		error=3;
	}
	return true;
}

void version()
{
	VS_FIXEDFILEINFO *v;
	UINT i;
	void *s=LockResource(LoadResource(0, FindResource(0, (char*)VS_VERSION_INFO, RT_VERSION)));
	if(s && VerQueryValue(s, "\\", (void**)&v, &i)) {
		printf(HIWORD(v->dwFileVersionLS) ? "%d.%d.%d\n" : "%d.%d\n",
			HIWORD(v->dwFileVersionMS), LOWORD(v->dwFileVersionMS), HIWORD(v->dwFileVersionLS));
	}
}

void help()
{
	puts("Parameters:\n\
-p, -precision   number of significant digits\n\
-i, -fix         fixed number of digits after the decimal point\n\
-s, -sci         scientific result format\n\
-e, -eng         engineering result format\n\
-a, -fraction    enable calculation with fractions\n\
-deg             degrees angle units (default)\n\
-r, -rad         radians angle units\n\
-grad            gradients angle units\n\
-b, -base        base of the numeral system\n\
-dec             decimal base (default)\n\
-hex             hexadecimal base\n\
-bin             binary base\n\
-oct             octal base\n\
-freqBefore      frequency of the separator before the decimal point\n\
-freqAfter       frequency of the separator after the decimal point\n\
-separBefore     separator before the decimal point (default is a space)\n\
-separAfter      separator after the decimal point (default is a space)\n\
-disableRounding disable rounding of the last digit\n\
-version         show version number of this calculator\n\
\n\
The mathematical expression must be the last parameter.\n\
The expression must start with a quotation mark if it contains <, >, ^, |, &.\n\
");
}

int main()
{
	initFuncTab();
	ans=ALLOCC(1);
	enableFractions= useSeparator1= useSeparator2= 0;

	char *s= GetCommandLine();
	//printf("%s<CR>\n",s);
	if(s[0]=='"') s = strchr(s+1, '"');
	else s=strchr(s, ' ');
	if(!s++ || !*s) {
		printf("Precise Calculator "); version();
		help();
		return 1;
	}
	for(;;) {
		while(*s==' ') s++;
		if(*s!='-' || (!isLetter(s[1]) && s[1]!='-')) break;
		s++;
		if(isParam(s, 'p', "precision", digits, 5, 1000000000)) {}
		else if(isParam(s, 'i', "fix", fixDigits, 0, 99999))  numFormat=MODE_FIX;
		else if(isParam(s, 's', "sci")) numFormat=MODE_SCI;
		else if(isParam(s, 'e', "eng")) numFormat=MODE_ENG;
		else if(isParam(s, 'a', "fraction")) enableFractions=1;
		else if(isParam(s, 0, "deg")) angleMode=ANGLE_DEG;
		else if(isParam(s, 'r', "rad")) angleMode=ANGLE_RAD;
		else if(isParam(s, 0, "grad")) angleMode=ANGLE_GRAD;
		else if(isParam(s, 'b', "base", base, 2, 36)) {}
		else if(isParam(s, 0, "dec")) base=10;
		else if(isParam(s, 0, "hex")) base=16;
		else if(isParam(s, 0, "bin")) base=2;
		else if(isParam(s, 0, "oct")) base=8;
		else if(isParam(s, 't', "freqBefore", sepFreq1, 2, 99999)) useSeparator1 = 1;
		else if(isParam(s, 'q', "freqAfter", sepFreq2, 2, 99999)) useSeparator2 = 1;
		else if(isParam(s, 0, "separBefore")) { separator1=*s; if(*s) s++; useSeparator1=1; }
		else if(isParam(s, 0, "separAfter")) { separator2=*s; if(*s) s++; useSeparator2=1; }
		else if(isParam(s, 0, "disableRounding")) disableRounding=1;
		else if(isParam(s, 0, "version")) { version(); return 1; }
		else if(isParam(s, 'h', "help")) { help(); return 1; }
		else {
			printf("Unknown parameter ");
			while(*s!=' ' && *s) putchar(*s++);
			return 2;
		}
	}
	if(*s=='"') s++;
	if(!*s) {
		puts("Expression is missing"); 
		return 4;
	}
	if(!error) calcThread(s);
	return error;
}
#endif
