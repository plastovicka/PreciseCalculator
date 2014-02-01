; (C) 2005-2014  Petr Lastovicka

; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License.

;compilation:  TASM32  aritm.asm /ml
;then use EDITBIN tool (from Microsoft Visual C++ 6.0) to convert OMF format to COFF

;calling convention is stdcall or fastcall
;  called functions remove arguments from the stack
;  arguments are passed from right to left
;the caller must allocate memory for a result

ideal
p386
model flat,pascal

extrn	_Alloc:proc, _Free:proc, _cerror:proc, _MULTX@12:proc, _DIVX@12:proc, _SQRTX@8:proc 
extrn	_base:dword, _baseIn:dword, _error:dword, _dwordDigits:dword
global	_overflow:proc;, MULTX:proc

RES	equ	52
HED	equ	16
;-------------------------------------
dataseg
E_1008	db	'Negative operand of sqrt',0
E_1010	db	'Division by zero',0
E_1011	db	'Overflow, number is too big',0
_digitTab db	'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ',0
dwordDigitsI db	0,0, 32, 20, 16, 13, 12, 11, 10, 10, 9, 9, 8, 8, 8, 8, 8
	db	7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
dwordMax dd	0,0, 0, 3486784401, 0, 1220703125, 2176782336, 1977326743
	dd	1073741824, 3486784401, 1000000000, 2357947691, 429981696
	dd	815730721, 1475789056, 2562890625, 0
	dd	410338673, 612220032, 893871739, 1280000000, 1801088541
	dd	2494357888, 3404825447, 191102976, 244140625, 308915776
	dd	387420489, 481890304, 594823321, 729000000, 887503681
	dd	1073741824, 1291467969, 1544804416, 1838265625, 2176782336

codeseg
;-------------------------------------
proc	@normx
uses	esi,edi
	mov	edi,eax
	call	norm
	ret
endp	@normx
;-------------------------------------
proc	@addii
	add	eax,edx
	jno	@@r
	call	_overflow	
@@r:	ret
endp	@addii
;-------------------------------------
;[edi]:=[esi]
;nemÏnÌ esi,edi,ebx
proc	copyx
uses	esi,edi
	cmp	esi,edi
	jz	@@ret
	cld
	mov	ecx,[esi-12]
	cmp	ecx,-2
	jnz	@@0
	mov	[edi-12],ecx
	mov	eax,8
	sub	edi,eax
	sub	esi,eax
	movsd
	movsd
	movsd
	movsd
	ret
@@0:	mov	eax,[edi-16]
	mov	edx,edi
	cmp	ecx,eax
	pushf
	jbe	@@1
	mov	ecx,eax
@@1:	mov	[edi-12],ecx
	add	ecx,2	;okopÌruj takÈ znamÈnko a exponent
	mov	eax,8
	sub	edi,eax
	sub	esi,eax
	rep movsd
	popf
	jbe	@@ret
;zaokrouhlenÌ
	mov	eax,[esi]
	test	eax,eax
	jns	@@ret
	mov	edi,edx
	push	ebx
	call	incre
	pop	ebx
@@ret:	ret
endp	copyx

proc	COPYX
arg	a1,a0
uses	esi,edi
	mov	edi,[a0]
	mov	esi,[a1]
	call	copyx
	ret
endp	COPYX
;-------------------------------------
;alokuje ËÌslo s mantisou dÈlky eax
proc	@allocx
	push	eax
	lea	eax,[eax*4+HED+RES]	;vËetnÏ hlaviËky a rezervovanÈho mÌsta
	push	eax
	call	_Alloc
	pop	edx
	pop	ecx
	test	eax,eax
	jz	@@ret
	add	eax,HED
	mov	[eax-16],ecx
	mov	[dword eax-12],0	;inicializuj na nulu
	mov	[dword eax-4],1
	mov	[dword eax-8],0
@@ret:	ret
endp	@allocx

proc ALLOCNX
	mov	eax,[esp+8]
	lea	eax,[eax*4+HED+RES]	;vËetnÏ hlaviËky a rezervovanÈho mÌsta
	push	eax
	mul	[dword esp+8]
	push	eax
	call	_Alloc
	pop	edx
	pop	ecx
	test	eax,eax
	jz	@@ret
	lea	eax,[eax+edx+HED]
@@lp:	
	sub	eax,ecx
	mov	edx,[esp+8]
	mov	[eax-16],edx
	mov	[dword eax-12],0	;inicializuj na nulu
	mov	[dword eax-4],1
	mov	[dword eax-8],0
	mov	edx,[esp+4]
	mov	edx,[esp+4*edx+8]
	mov	[edx],eax
	dec	[dword esp+4]
	jnz	@@lp
@@ret:	ret
endp ALLOCNX
;-------------------------------------
;vytvo¯Ì kopii operandu
proc	@newcopyx
uses	esi,edi
	mov	ecx,[eax-16]
	push	eax
	lea	eax,[ecx*4+HED+RES]
	push	eax
	call	_Alloc
	pop	edx
	pop	esi
	test	eax,eax
	jz	@@ret
;okopÌruj vËetnÏ hlaviËky
	mov	edi,eax
	mov	ecx,[esi-12]
	test	ecx,ecx
	jns	@@1
	neg	ecx
@@1:	add	ecx,HED/4
	mov	edx,HED
	add	eax,edx
	sub	esi,edx
	cld
	rep movsd
@@ret:	ret
endp	@newcopyx
;-------------------------------------
;uvolnÏnÌ pamÏti
proc	@freex
	test	eax,eax
	jz	@@ret
	sub	eax,HED
	push	eax
	call	_Free
	pop	eax
@@ret:	ret
endp	@freex
;-------------------------------------
proc	@setx
	test	edx,edx
	jz	@zerox
	mov	[dword eax-12],-2
	mov	[dword eax-8],0
	mov	[dword eax-4],1
	mov	[eax],edx
	mov	[dword eax+4],1
	ret
endp	@setx

proc	@setxn
	test	edx,edx
	jns	@setx
	neg	edx
	mov	ecx,1
	mov	[dword eax-12],-2
	mov	[dword eax-8],ecx
	mov	[dword eax-4],ecx
	mov	[eax],edx
	mov	[eax+4],ecx
	ret
endp	@setxn

proc	@zerox
	and	[dword eax-12],0
	ret
endp	@zerox

proc	@onex
	mov	edx,1
	mov	[dword eax-12],-2
	and	[dword eax-8],0
	mov	[eax-4],edx
	mov	[eax],edx
	mov	[eax+4],edx
	ret
endp	@onex
;-------------------------------------
;[esi]-=eax;  nesmÌ dojÌt k podteËenÌ
proc	decra
	sub	[esi],eax
	jnc	trim
@@1:	sub	esi,4
decr:	sub	[dword esi],1
	jc	@@1

;u¯ÌznutÌ koncov˝ch nul [edi]
trim:	mov	ecx,[edi-12]
	lea	esi,[edi+4*ecx-4]
trims:	mov	eax,[esi]
	test	eax,eax
	jnz	@ret
	sub	esi,4
	dec	[dword edi-12]
	jnz	trims
	ret
;dekrementace [edi];  Ë·rka musÌ b˝t uvnit¯ mantisy; nesmÌ dojÌt k podteËenÌ
decre:	mov	ecx,[edi-12]
	lea	esi,[edi+4*ecx-4]
	jmp	decr
endp
;-------------------------------------
;[edi+ecx]+=eax
proc	incri
	mov	edx,[edi-12]
	mov	ebx,[edi-16]
	test	ecx,ecx
	js	@@sh		;p¯ed ËÌslem
	lea	esi,[edi+4*ecx]
	cmp	ecx,edx
	jc	incra		;uvnit¯ ËÌsla
	cmp	ecx,ebx
	ja	@@ret		;za ËÌslem d·l neû o 1
	mov	[edi-12],ecx
	jnz	@@2
;tÏsnÏ za ËÌslem - rozhodne se o zaokrouhlenÌ
	test	eax,eax
	jns	@@ret
@@2:	setc	bl
;increment leûÌ v alokovanÈm mÌstÏ za ËÌslem
	push	edi
	mov	[esi],eax
;vynuluj oblast mezi ËÌslem a inkrementem
	xor	eax,eax
	lea	edi,[esi-4]
	sub	ecx,edx
	std
	rep stosd
	cld
	pop	edi
	test	bl,bl
	jz	round2
	inc	[dword edi-12]
@@ret:	ret
;inkrement je p¯ed ËÌslem - ËÌslo se posune vpravo
@@sh:	neg	ecx
	add	[edi-4],ecx	;uprav exponent
	cmp	ecx,ebx
	ja	@@d		;ËÌslo je moc malÈ -> zruöÌm ho
	sub	ebx,ecx
	cmp	ebx,edx
	push	edi
	push	ecx
	jae	@@1
;ztr·ta p¯esnosti
	mov	edx,[edi-16]
	inc	edx
	mov	[edi-12],edx
	jmp	@@cp
;zvÏtöenÌ dÈlky ËÌsla
@@1:	add	[edi-12],ecx
	mov	ebx,edx
;posun bloku dÈlky ebx smÏrem vpravo o ecx integer˘
@@cp:	lea	esi,[edi+4*ebx-4]
	lea	edi,[esi+4*ecx]
	mov	ecx,ebx
	std
	rep movsd
	cld
	pop	ecx
	pop	edi
;zapiö inkrement na prvnÌ pozici
	mov	[edi],eax
;vynuluj mÌsto mezi
	xor	eax,eax
	dec	ecx
	push	edi
	add	edi,4
	cld
	rep stosd
	pop	edi
;zaokrouhli
	mov	eax,[edi-16]
	cmp	eax,[edi-12]
	jae	@@ret
	dec	[dword edi-12]
	jmp	roundi
@@d:	mov	[edi],eax
	mov	[dword edi-12],1
	ret
endp	incri
;-------------------------------------
;[edi+ecx]-=eax
;nesmÌ dojÌt k podteËenÌ !
proc	decri
	test	eax,eax
	jz	@@ret
	mov	edx,[edi-12]
	mov	ebx,[edi-16]
	lea	esi,[edi+4*ecx]
	cmp	ecx,edx
	jc	decra		;uvnit¯ ËÌsla
	cmp	ecx,ebx
	ja	@@ret		;za ËÌslem d·l neû o 1
        jnz	@@3
;tÏsnÏ za ËÌslem - rozhodne se o zaokrouhlenÌ
	test	eax,eax
	jns	@@ret
@@3:	push	ecx
	push	edi
	push	eax
	call	decre
	pop	eax
	pop	edi
	pop	ecx
	mov	[edi-12],ecx
;decrement leûÌ v alokovanÈm mÌstÏ za ËÌslem
	push	edi
	neg	eax
	mov	[esi],eax
	sbb	eax,eax		;0ffffffffH
	lea	edi,[esi-4]
	sub	ecx,edx
	std
	rep stosd
	cld
	pop	edi
	jmp	roundi
@@ret:	ret
endp	decri
;-------------------------------------
;p¯iËtenÌ jedniËky na konec mantisy [edi]
incre:	mov	ecx,[edi-12]
	lea	esi,[edi+4*ecx-4]
	jmp	incr
;edi, [esi]+=eax
incra:	add	[esi],eax
	jnc	trim
	sub	esi,4
	cmp	esi,edi
	jc	carry1
;edi, [esi]++
@incr:	inc	[dword esi]
	jnz	trim
	sub	esi,4
incr:	cmp	esi,edi
	jnc	@incr
carry1:	mov	eax,1
;[edi]=eax:[edi]
carry:	test	eax,eax
	jz	@ret
	mov	ecx,[edi-12]
	lea	esi,[edi+4*ecx-4]
	cmp	ecx,[edi-16]
	mov	ebx,[esi]
	jae	@1
;zvÏtöi dÈlku
	inc	[dword edi-12]
	xor	ebx,ebx
;zvÏtöi exponent
@1:	inc	[dword edi-4]
	jo	_overflow
;posun celÈ mantisy o jeden ¯·d vpravo
@2:	push	esi
	lea	edi,[esi+4]
	std
	rep movsd
	cld
;p¯id·nÌ p¯enosu
	mov	[edi],eax
	test	ebx,ebx
	pop	esi
	js	@incr
	jmp	trim
@ret:	ret

;-------------------------------------
;zmenöÌ dÈlku [edi] na maximum a zaokrouhlÌ podle cifry za mantisou
proc	round
	mov	ecx,[edi-16]
	cmp	ecx,[edi-12]
	jb	round1
	ret
endp	round

;zvÏtöÌ dÈlku [edi] o jedna nebo zaokrouhlÌ
proc	roundi
	mov	ecx,[edi-16]
	cmp	ecx,[edi-12]
	ja	@@1
round1:	mov	[edi-12],ecx
;podÌvej se do rezerovanÈho mÌsta za mantisou
	lea	esi,[edi+4*ecx]
round2:	mov	eax,[esi]
	sub	esi,4
	test	eax,eax
	jns	trims
;zaokrouhlenÌ nahoru
	jmp	incr
;pokud je jeötÏ volnÈ mÌsto, pak jen zvÏtöi dÈlku ËÌsla
@@1:	inc	[dword edi-12]
@@ret:	ret
endp	roundi
;-------------------------------------
;normalizace [edi] - mantisa nebude zaËÌnat ani konËit na nulu
;zmÏnÌ esi,edi
proc	norm
	cmp	[dword edi-12],0
	jbe	@@ret    ;zlomek nebo nula
	call	trim
	mov	ecx,[edi-12]
	test	ecx,ecx
	jz	@@ret   ;trim v˝sledek vynulovalo
	xor	edx,edx
	cmp	[edi],edx
	jnz	@@ret
	mov	eax,edi
@@lp:	add	eax,4
	dec	[dword edi-4]
	jo	@@nul
	dec	ecx
	jz	@@nul
	cmp	[eax],edx
	jz	@@lp
@@e:	mov	esi,eax
	mov	[edi-12],ecx
	cld
	rep movsd
@@ret:	ret
@@nul:	mov	[dword edi-12],0
	ret
endp	norm
;-------------------------------------
;eax:=ecx:=gcd(eax,ecx), edx:=0
proc	gcd0
	div	ecx
	mov	eax,ecx
	mov	ecx,edx
gcd:	xor	edx,edx
	test	ecx,ecx
	jnz	gcd0
	mov	ecx,eax
	ret
endp

;zkr·tÌ zlomek [esi],[edi]
proc	reduce
	mov	eax,[esi]
	mov	ecx,[edi]
	call	gcd
	mov	eax,[esi]
	div	ecx
	mov	[esi],eax
	mov	eax,[edi]
	div	ecx
	mov	[edi],eax
	ret
endp	reduce

proc	fracReduce
	push	esi
	lea	esi,[edi+4]
	call	reduce
	pop	esi

fracexp:mov	ecx,[edi]
	cmp	ecx,[edi+4]
	setae	al
	mov	[edi-4],al
	mov	[dword edi-12],-2
	test	ecx,ecx
	jnz	@@ret
	and	[dword edi-12],0
@@ret:	ret
endp	fracReduce

;-------------------------------------
;a0:=a1/ai
;a0 m˘ûe b˝t rovno a1
proc	DIVI
arg	ai,a1,a0
uses	esi,edi,ebx
	mov	esi,[a1]
	mov	edi,[a0]
	mov	ebx,[ai]
	cmp	ebx,1
	ja	@@z
	jnz	@@e
;a0=a1*1
	call	copyx
	ret
@@e:	lea	eax,[E_1010]
	push	eax
	push	1010
	call	_cerror
	add	esp,8
	ret
@@z:	cmp	[dword esi-12],-2
	jnz	@@0
;zlomek
	call	copyx
	lea	esi,[ai]
	call	reduce
	mov	ebx,[ai]
	mov	eax,[edi+4]
	mul	ebx
	jo	@@f1
	mov	[edi+4],eax
	call	fracexp
	jmp	@@ret
;convert fraction to real
@@f1:	mov	eax,edi
	call	@fractox
	mov	esi,edi
;okopÌruj znamÈnko a exponent
@@0:	mov	eax,[esi-4]
	mov	[edi-4],eax
	mov	edx,[esi-8]
	mov	[edi-8],edx
;nastav dÈlku v˝sledku jako minimum z dÈlek operand˘
	mov	ecx,[esi-12]
	mov	eax,[edi-16]
	cmp	ecx,eax
	jbe	@@1
	mov	ecx,eax
@@1:	mov	[edi-12],ecx
	test	ecx,ecx
	jz	@@ret     ; 0/ai=0
;naËti prvnÌ cifru dÏlence
	mov	edx,[esi]
	cmp	edx,ebx
	jnc	@@2
;prvnÌ dÏlenÌ bude 64-bitovÈ a v˝sledek bude kratöÌ
	add	esi,4
	dec	[dword edi-4]
	jno	@@6
;podteËenÌ -> v˝sledek nulov˝
	mov	[dword edi-12],0
	ret
@@6:	dec	[dword edi-12]
	dec	ecx
	jnz	@@4
;dÏlÏnec m· dÈlku 1 a je menöÌ neû dÏlitel
        mov	esi,edi
	jmp	@@3
@@2:	xor	edx,edx
;ecx=dÈlka v˝sledku, ebx=dÏlitel, edx=hornÌ Ëty¯byte dÏlÏnce
@@4:	push	edi
;hlavnÌ cyklus
@@lp:	mov	eax,[esi]
	add	esi,4
	div	ebx
	mov	[edi],eax
	add	edi,4
	dec	ecx
	jnz	@@lp

	mov	esi,edi
	test	edx,edx
	pop	edi
	jz	@@ret   ;nulov˝ zbytek
	mov	ecx,[edi-12]
	jmp	@@3
;za vstupnÌ operand doplÚuj nuly a pokraËuj v dÏlenÌ
@@lp2:	xor	eax,eax
	div	ebx
	mov	[esi],eax
	add	esi,4
	inc	ecx
@@3:	cmp	ecx,[edi-16]
	jnz	@@lp2
	sub	esi,4
	mov	[edi-12],ecx
;zaokrouhli podle zbytku
	shl	edx,1
	jc	@@5
	cmp	edx,ebx
	jnc	@@5
	call	trim
	jmp	@@ret
@@5:	call	incr
@@ret:	ret
endp	DIVI
;-------------------------------------
;konverze zlomku na desetinnÈ ËÌslo
proc	@fractox
	cmp	[dword eax-12],-2
	jnz	@@ret
fractox:
	mov	[dword eax-12],1
	mov	[dword eax-4],1
	push	[dword eax+4]
	push	eax
	push	eax
	call	DIVI
@@ret:	ret
endp	@fractox

;len=[edi-16], a/b=[esi]
;return eax
proc	fractonewx2
	cmp	[dword esi-12],-2
	mov	eax,esi
	jnz	@newcopyx
fractonewx:
	mov	eax,[edi-16]
	call	@allocx
	push	eax
	mov	ecx,[esi]
	mov	[eax],ecx
	mov	ecx,[esi+4]
	mov	[eax+4],ecx
	call	fractox
	pop	eax
	ret
endp	fractonewx2
;-------------------------------------
;cmp [edx],[ecx]
proc	cmpu
uses	esi,edi
;test na nulovost operand˘
	cmp	[dword edx-12],0
	jnz	@@1
	cmp	[dword ecx-12],0
	jz	@@ret		;oba jsou 0
	jmp	@@gr2		;prvnÌ je 0
@@1:	cmp	[dword ecx-12],1
	jc	@@gr1		;druh˝ je 0
;zlomky
	cmp	[dword edx-12],-2
	jnz	@@f1
	cmp	[dword ecx-12],-2
	jnz	@@f2
;a/b ? c/d
	mov	edi,edx
	mov	eax,[edx]
	mul	[dword ecx+4]
	push	edx
	push	eax
	mov	eax,[ecx]
	mul	[dword edi+4]
	pop	ecx
	pop	edi
;edi:ecx ? edx:eax
	cmp	edi,edx
	jnz	@@ret
	cmp	ecx,eax
	jmp	@@ret
@@f1:   cmp	[dword ecx-12],-2
	jnz	@@e
; x ? (a/b)
	mov	esi,ecx
	mov	edi,edx
	call	fractonewx
	mov	edx,edi
	mov	ecx,eax
	jmp	@@r
; (a/b) ? x
@@f2:	mov	esi,edx
	mov	edi,ecx
	call	fractonewx
	mov	edx,eax
	mov	ecx,edi
@@r:	mov	esi,eax
	call	cmpu
	pushf
	mov	eax,esi
	call	@freex
	popf
	ret
;exponenty
@@e:	mov	eax,[edx-4]
	cmp	eax,[ecx-4]
	jg	@@gr1
	jl	@@gr2
;rychlÈ porovn·nÌ nejvyööÌho ¯·du
	mov	eax,[edx]
	cmp	eax,[ecx]
	jnz	@@ret
;o¯ÌznutÌ koncov˝ch nul
	mov	eax,[edx-12]
	add	edx,4
	lea	esi,[edx+4*eax]
@@t1:	sub	esi,4
	mov	eax,[esi-4]
	test	eax,eax
	jz	@@t1
	mov	eax,[ecx-12]
	add	ecx,4
	lea	edi,[ecx+4*eax]
@@t2:	sub	edi,4
	mov	eax,[edi-4]
	test	eax,eax
	jz	@@t2
;mantisy
@@lp:	cmp	edx,esi
	jnc	@@2
	cmp	ecx,edi
	jnc	@@3
	mov	eax,[edx]
	cmp	eax,[ecx]
	jnz	@@ret
	add	edx,4
	add	ecx,4
	jmp	@@lp
;prvnÌ operand je kratöÌ nebo jsou si rovny
@@2:	cmp	ecx,edi	;ZF=1 nebo CF=1
	ret
;druh˝ operand je kratöÌ
@@3:	cmp	esi,edx	;nastav ZF=0, CF=0
	ret
@@gr2:	stc
	ret
@@gr1:	clc
@@ret:	ret
endp	cmpu

proc	CMPU
arg	a2,a1
	mov	edx,[a1]
	mov	ecx,[a2]
	jmp	cmpu1
endp	CMPU
;-------------------------------------
;porovn·nÌ, v˝sledek -1,0,+1
proc	CMPX
arg	a2,a1
	mov	edx,[a1]
	mov	ecx,[a2]
	cmp	[dword edx-12],0
	jnz	@@1
	cmp	[dword ecx-12],0
	jz	@@eq
@@1:	mov	eax,[edx-8]
	cmp	eax,[ecx-8]
	ja	@@gr2
	jb	@@gr1
	test	eax,eax
	jz	cmpu1
	call	cmpu
	ja	@@gr2
	jb	@@gr1
	jmp	@@eq
cmpu1:	call	cmpu
	jb	@@gr2
	ja	@@gr1
@@eq:	xor	eax,eax
	ret
@@gr1:	mov	eax,1
	ret
@@gr2:	mov	eax,-1
	ret
endp	CMPX
;-------------------------------------
proc	plusfrac
	mov	eax,[esi+4]
	mov	ecx,[ebx+4]
	call	gcd
;lcm(b,d)
	mov	eax,[esi+4]
	mul	[dword ebx+4]
	cmp	edx,ecx
	jae	@@fe
	div	ecx
	mov	[edi+4],eax
;a*lcm(b,d)/b
	mov	eax,[ebx]
	mul	[dword edi+4]
	mov	ecx,[ebx+4]
	cmp	edx,ecx
	jae	@@fe
	div	ecx
	mov	[edi],eax
;c*lcm(b,d)/d
	mov	eax,[esi]
	mul	[dword edi+4]
	mov	ecx,[esi+4]
	cmp	edx,ecx
	jae	@@fe
	div	ecx
	stc
@@fe:	ret
endp	plusfrac

;bezznamÈnkovÈ plus
proc	PLUSU
arg	a2,a1,a0
local	k0,k1,k2,e0,e1,e2
uses	esi,edi,ebx
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
;prohoÔ operandy, aby byl prvnÌ vÏtöÌ
plusuf:	mov	eax,[ebx-4]
	cmp	eax,[ecx-4]
	jge	@@1
	mov	eax,ebx
	mov	ebx,ecx
	mov	ecx,eax
	mov	[a1],ebx
	mov	[a2],ecx
@@1:	mov	esi,ecx
	mov	edi,edx
;test na nulovost 1.operandu
	cmp	[dword ebx-12],0
	jnz	@@0
;okopÌruj 2.operand
	push	[dword edi-8]
	call	copyx
	pop	[dword edi-8]
	ret
;test na nulovost 2.operandu
@@0:	cmp	[dword ecx-12],0
	jz	@@cp1a
	cmp	[dword ecx-12],-2
	jnz	@@f1
	cmp	[dword ebx-12],-2
	jnz	@@fe
;(a/b)+(c/d)
	call	plusfrac
	jnc	@@fe
	add	[edi],eax
	jc	@@fe
	call	fracReduce
	ret
@@f1:	cmp	[dword ebx-12],-2
	jnz	@@f2
;(a/b)+x
	mov	esi,ebx
	mov	ebx,ecx
;x+(c/d)
@@fe:	call	fractonewx
	push	eax
	push	eax
	push	ebx
	push	edi
	call	PLUSU
	pop	eax
	call	@freex
	ret
;zvÏtöi p¯esnost o jedna
@@f2:	inc	[dword edx-16]
;nastav promÏnnÈ urËujÌcÌ exponent na konci operand˘
	mov	eax,[ebx-4]
	sub	eax,[ebx-12]
	mov	[k1],eax
	mov	eax,[ecx-4]
	sub	eax,[ecx-12]
	mov	[k2],eax
	mov	eax,[ebx-4]
	mov	esi,[edx-16]
	mov	[edx-4],eax	;exponent v˝sledku
	mov	[edx-12],esi	;dÈlka v˝sledku
	sub	eax,esi		;[k0]
	cmp	eax,[ecx-4]
	jl	@@7
;2.operand je moc mal˝
@@cp1:	dec	[dword edx-16]
@@cp1a:	mov	esi,ebx
	mov	edi,edx
	push	[dword edi-8]
	call	copyx
	pop	[dword edi-8]
	ret
@@7:	mov	[k0],eax
;spoËti adresy konc˘ operand˘
	mov	eax,[ebx-12]
	lea	eax,[4*eax+ebx-4]
	mov	[e1],eax
	mov	eax,[ecx-12]
	lea	esi,[4*eax+ecx-4]
	mov	[e2],esi
	mov	eax,[edx-12]
	lea	edi,[4*eax+edx-4]
	mov	[e0],edi
;o¯Ìzni operandy, kterÈ jsou moc dlouhÈ
	mov	eax,[k0]
	sub	eax,[k1]
	jle	@@2
	add	[k1],eax
	shl	eax,2
	sub	[e1],eax
@@2:	mov	edi,[k0]
	sub	edi,[k2]
	jle	@@3
	add	[k2],edi
	shl	edi,2
	sub	[e2],edi
;zmenöi dÈlku v˝sledku, pokud jsou operandy moc kr·tkÈ
@@3:	cmp	eax,edi
	jge	@@4
	mov	eax,edi
@@4:	test	eax,eax
	jns	@@5
	add	[edx-12],eax
	sub	[k0],eax
	shl	eax,2
	add	[e0],eax
;zjisti, kter˝ operand je delöÌ
@@5:	pushf
	std
	mov	edx,ecx
	mov	edi,[e0]
	mov	ecx,[k1]
	sub	ecx,[k2]
	jz	@@plus
	jg	@@6
;okopÌruj konec 1.operandu
	neg	ecx
	mov	esi,[e1]
	rep movsd
	mov	[e1],esi
	jmp	@@plus
@@6:	mov	eax,[k1]
	sub	eax,[edx-4]
	mov	esi,[e2]
	jle	@@cpe
;operandy se nep¯ekr˝vajÌ
	sub	ecx,eax
	rep movsd
;volnÈ mÌsto mezi operandy vyplÚ nulami
	mov	ecx,eax
	xor	eax,eax
	rep stosd
	mov	esi,[e1]
	jmp	@@cpb
;okopÌruj konec 2.operandu
@@cpe:	rep movsd
	mov	[e2],esi
;seËti p¯ekr˝vajÌcÌ se Ë·sti operand˘
;edi, e1,e2
@@plus:	mov	ecx,[a2]
	sub	ecx,[e2]
	sar	ecx,2
	dec	ecx
	mov	esi,[e1]
	mov	ebx,[e2]
	lea	edi,[edi+4*ecx]
	lea	esi,[esi+4*ecx]
	lea	ebx,[ebx+4*ecx]
	neg	ecx
	clc
	jz	@@cpb
@@lp:	mov	eax,[esi+4*ecx]
	adc	eax,[ebx+4*ecx]
	mov	[edi+4*ecx],eax
	dec	ecx
	jnz	@@lp
;okopÌruj zaË·tek 1.operandu z esi na edi
@@cpb:	setc	bl
	push	edi
	mov	eax,[a1]
	mov	ecx,esi
	sub	ecx,eax
	sar	ecx,2
	inc	ecx	;ecx= (esi+4-[a1])/4
	rep movsd
	pop	esi
	add	edi,4
;edi=[a0], esi=pozice p¯enosu
;zpracuj p¯eteËenÌ
	test	bl,bl
	jz	@@end
	call	incr
@@end:	popf
	dec	[dword edi-16]
	call	round
	ret
endp	PLUSU
;-------------------------------------
proc	negf
;edi,esi ukazujÌ na poslednÌ dword
;ecx je dÈlka
;vracÌ CF
	neg	ecx
	lea	edi,[edi+4*ecx]
	lea	esi,[esi+4*ecx]
	neg	ecx
	xor	ebx,ebx
@@lpe:	mov	eax,ebx
	sbb	eax,[esi+4*ecx]
	mov	[edi+4*ecx],eax
	dec	ecx
	jnz	@@lpe
	ret
endp	negf
;-------------------------------------
;bezznamÈnkovÈ minus
proc	MINUSU
arg	a2,a1,a0
local	k0,k1,k2,e0,e1,e2
uses	esi,edi,ebx
minusua:mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	edi,edx
	mov	esi,ecx
;test na nulovost 2.operandu
@@0:	cmp	[dword ecx-12],0
	jz	@@cp1a
;test na zlomek
	cmp	[dword ecx-12],-2
	jnz	@@f1
	cmp	[dword ebx-12],-2
	jnz	@@fe
;(a/b)-(c/d)
	call	plusfrac
	jnc	@@fe
	sub	[edi],eax
	call	fracReduce
	ret
@@f1:	cmp	[dword ebx-12],-2
	jnz	@@f2
;(a/b)-x
	push	esi
	mov	esi,ebx
	call	fractonewx
	mov	esi,eax
	push	eax
	jmp	@@fm
;x-(c/d)
@@fe:	call	fractonewx
	mov	esi,eax
	push	eax
	push	ebx
@@fm:	push	edi
	call	MINUSU
	mov	eax,esi
	call	@freex
	ret
;zvÏtöi p¯esnost o jedna
@@f2:	inc	[dword edx-16]
;nastav promÏnnÈ urËujÌcÌ exponent na konci operand˘
	mov	eax,[ebx-4]
	sub	eax,[ebx-12]
	mov	[k1],eax
	mov	eax,[ecx-4]
	sub	eax,[ecx-12]
	mov	[k2],eax
	mov	eax,[ebx-4]
	mov	esi,[edx-16]
	mov	[edx-4],eax	;exponent v˝sledku
	mov	[edx-12],esi	;dÈlka v˝sledku
	sub	eax,esi		;[k0]
	cmp	eax,[ecx-4]
	jl	@@7
;2.operand je moc mal˝
@@cp1:	dec	[dword edx-16]
@@cp1a:	mov	esi,ebx
	mov	edi,edx
	push	[dword edi-8]
	call	copyx
	pop	[dword edi-8]
	ret
@@7:	mov	[k0],eax
;spoËti adresy konc˘ operand˘
	mov	eax,[ebx-12]
	lea	eax,[4*eax+ebx-4]
	mov	[e1],eax
	mov	eax,[ecx-12]
	lea	esi,[4*eax+ecx-4]
	mov	[e2],esi
	mov	eax,[edx-12]
	lea	edi,[4*eax+edx-4]
	mov	[e0],edi
;o¯Ìzni operandy, kterÈ jsou moc dlouhÈ
	mov	eax,[k0]
	sub	eax,[k1]
	jle	@@2
	add	[k1],eax
	shl	eax,2
	sub	[e1],eax
@@2:	mov	edi,[k0]
	sub	edi,[k2]
	jle	@@3
	add	[k2],edi
	shl	edi,2
	sub	[e2],edi
;zmenöi dÈlku v˝sledku, pokud jsou operandy moc kr·tkÈ
@@3:	cmp	eax,edi
	jge	@@4
	mov	eax,edi
@@4:	test	eax,eax
	jns	@@5
	add	[edx-12],eax
	sub	[k0],eax
	shl	eax,2
	add	[e0],eax
;zjisti, kter˝ operand je delöÌ
@@5:	pushf
	std
	mov	edx,ecx
	mov	edi,[e0]
	mov	ecx,[k1]
	sub	ecx,[k2]
	jz	@@minus
	jg	@@6
;okopÌruj konec 1.operandu
	neg	ecx
	mov	esi,[e1]
	rep movsd
	mov	[e1],esi
	clc
	jmp	@@minus
@@6:	mov	eax,[k1]
	sub	eax,[edx-4]
	mov	esi,[e2]
	jle	@@nege
;operandy se nep¯ekr˝vajÌ
	sub	ecx,eax
	push	eax
	call	negf
	pop	ecx
;volnÈ mÌsto mezi operandy vyplÚ nulami nebo jedniËkami
	sbb	eax,eax
	rep stosd	;nemÏnÌ CF
	mov	esi,[e1]
	jmp	@@cpb
;neguj konec 2.operandu
@@nege:	call	negf
	mov	[e2],esi
;odeËti p¯ekr˝vajÌcÌ se Ë·sti operand˘
;edi, e1,e2, cf
@@minus:lahf
	mov	ecx,[a2]
	sub	ecx,[e2]
	sar	ecx,2
	dec	ecx
	mov	esi,[e1]
	mov	ebx,[e2]
	lea	edi,[edi+4*ecx]
	lea	esi,[esi+4*ecx]
	lea	ebx,[ebx+4*ecx]
	neg	ecx
	sahf
	inc	ecx
	dec	ecx
	jz	@@cpb
@@lp:	mov	eax,[esi+4*ecx]
	sbb	eax,[ebx+4*ecx]
	mov	[edi+4*ecx],eax
	dec	ecx
	jnz	@@lp
;okopÌruj zaË·tek 1.operandu z esi na edi
@@cpb:	setc	bl
	push	edi
	mov	eax,[a1]
	mov	ecx,esi
	sub	ecx,eax
	sar	ecx,2
	inc	ecx	;ecx= (esi+4-[a1])/4
	rep movsd
	pop	esi
	add	edi,4
;zpracuj p¯eteËenÌ
	test	bl,bl
	jz	@@end
	call	decr
@@end:	call	norm
	popf
	mov	edi,[a0]
	dec	[dword edi-16]
	call	round
	ret
endp	MINUSU
;-------------------------------------
proc	PLUSX
arg	a2,a1,a0
local	k0,k1,k2,e0,e1,e2
uses	esi,edi,ebx
	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	edx,[a0]
	mov	eax,[ebx-8]
	mov	[edx-8],eax
	cmp	eax,[ecx-8]
	jz	plusuf
	mov	edx,ebx
	call	cmpu
	jnc	minusua
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	eax,[ecx-8]
	mov	[a1],ecx
	mov	[a2],ebx
	mov	[edx-8],eax
	jmp	minusua
endp	PLUSX
;-------------------------------------
proc	MINUSX
arg	a2,a1,a0
local	k0,k1,k2,e0,e1,e2
uses	esi,edi,ebx
	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	edx,[a0]
	mov	eax,[ebx-8]
	mov	[edx-8],eax
	cmp	eax,[ecx-8]
	jnz	plusuf
;stejn· znamÈnka
	mov	edx,ebx
	call	cmpu
	jnc	minusua
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	eax,[ecx-8]
	xor	eax,1
	mov	[a1],ecx
	mov	[a2],ebx
	mov	[edx-8],eax
	jmp	minusua
endp	MINUSX
;-------------------------------------
;bezznamÈnkovÈ or nebo xor
proc	ORXORU0
arg	a2,a1,a0
local	k0,k1,k2,e0,e1,e2
local	isxor:byte
uses	esi,edi,ebx
	mov	[isxor],al
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
;prohoÔ operandy, aby byl prvnÌ vÏtöÌ
	mov	eax,[ebx-4]
	cmp	eax,[ecx-4]
	jge	@@1
	mov	eax,ebx
	mov	ebx,ecx
	mov	ecx,eax
	mov	[a1],ebx
	mov	[a2],ecx
;test na nulovost 1.operandu
@@1:	cmp	[dword ebx-12],0
	jnz	@@0
;okopÌruj 2.operand
	mov	esi,ecx
	mov	edi,edx
	push	[dword edi-8]
	call	copyx
	pop	[dword edi-8]
	ret
;test na nulovost 2.operandu
@@0:	cmp	[dword ecx-12],0
	jz	@@cp1
;nastav promÏnnÈ urËujÌcÌ exponent na konci operand˘
	mov	eax,[ebx-4]
	sub	eax,[ebx-12]
	mov	[k1],eax
	mov	eax,[ecx-4]
	sub	eax,[ecx-12]
	mov	[k2],eax
	mov	eax,[ebx-4]
	mov	esi,[edx-16]
	mov	[edx-4],eax	;exponent v˝sledku
	mov	[edx-12],esi	;dÈlka v˝sledku
	sub	eax,esi		;[k0]
	cmp	eax,[ecx-4]
	jl	@@7
;2.operand je moc mal˝
@@cp1:	mov	esi,ebx
	mov	edi,edx
	push	[dword edi-8]
	call	copyx
	pop	[dword edi-8]
	ret
@@7:	mov	[k0],eax
;spoËti adresy konc˘ operand˘
	mov	eax,[ebx-12]
	lea	eax,[4*eax+ebx-4]
	mov	[e1],eax
	mov	eax,[ecx-12]
	lea	esi,[4*eax+ecx-4]
	mov	[e2],esi
	mov	eax,[edx-12]
	lea	edi,[4*eax+edx-4]
	mov	[e0],edi
;o¯Ìzni operandy, kterÈ jsou moc dlouhÈ
	mov	eax,[k0]
	sub	eax,[k1]
	jle	@@2
	add	[k1],eax
	shl	eax,2
	sub	[e1],eax
@@2:	mov	edi,[k0]
	sub	edi,[k2]
	jle	@@3
	add	[k2],edi
	shl	edi,2
	sub	[e2],edi
;zmenöi dÈlku v˝sledku, pokud jsou operandy moc kr·tkÈ
@@3:	cmp	eax,edi
	jge	@@4
	mov	eax,edi
@@4:	test	eax,eax
	jns	@@5
	add	[edx-12],eax
	sub	[k0],eax
	shl	eax,2
	add	[e0],eax
;zjisti, kter˝ operand je delöÌ
@@5:	pushf
	std
	mov	edx,ecx
	mov	edi,[e0]
	mov	ecx,[k1]
	sub	ecx,[k2]
	jz	@@plus
	jg	@@6
;okopÌruj konec 1.operandu
	neg	ecx
	mov	esi,[e1]
	rep movsd
	mov	[e1],esi
	jmp	@@plus
@@6:	mov	eax,[k1]
	sub	eax,[edx-4]
	mov	esi,[e2]
	jle	@@cpe
;operandy se nep¯ekr˝vajÌ
	sub	ecx,eax
	rep movsd
;volnÈ mÌsto mezi operandy vyplÚ nulami
	mov	ecx,eax
	xor	eax,eax
	rep stosd
	mov	esi,[e1]
	jmp	@@cpb
;okopÌruj konec 2.operandu
@@cpe:	rep movsd
	mov	[e2],esi
;zpracuj p¯ekr˝vajÌcÌ se Ë·sti operand˘
;edi, e1,e2
@@plus:	mov	ecx,[a2]
	sub	ecx,[e2]
	sar	ecx,2
	dec	ecx
	mov	esi,[e1]
	mov	ebx,[e2]
	lea	edi,[edi+4*ecx]
	lea	esi,[esi+4*ecx]
	lea	ebx,[ebx+4*ecx]
	neg	ecx
	jz	@@cpb

	cmp	[byte isxor],0
	jnz	@@xor
@@or:	mov	eax,[esi+4*ecx]
	or	eax,[ebx+4*ecx]
	mov	[edi+4*ecx],eax
	dec	ecx
	jnz	@@or
	jmp	@@cpb
@@xor:	mov	eax,[esi+4*ecx]
	xor	eax,[ebx+4*ecx]
	mov	[edi+4*ecx],eax
	dec	ecx
	jnz	@@xor
;okopÌruj zaË·tek 1.operandu z esi na edi
@@cpb:	mov	eax,[a1]
	mov	ecx,esi
	sub	ecx,eax
	sar	ecx,2
	inc	ecx	;ecx= (esi+4-[a1])/4
	rep movsd
	popf            ;obnov direction flag
	mov	edi,[a0]
	call	norm
	ret
endp	ORXORU0
;-------------------------------------
proc	ANDU0
arg	a2,a1,a0
uses	esi,edi,ebx
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
;zjisti rozsah v˝sledku
	mov	edi,[ebx-4]
	mov	esi,[ecx-4]
	mov	eax,edi
	cmp	eax,esi
	jl	@@1
	mov	eax,esi
@@1:	mov	[edx-4],eax    ;exponent v˝sledku
	mov	eax,[ebx-12]
	test	eax,eax
	jz	@@zero
	sub	edi,eax
	mov	eax,[ecx-12]
	test	eax,eax
	jz	@@zero
	sub	esi,eax
	cmp	edi,esi
	jl	@@2
	mov	esi,edi
;spoËti dÈlku v˝sledku
@@2:	mov	edi,[edx-4]
	sub	edi,esi
	jg	@@3
;operandy se nep¯ekr˝vajÌ
@@zero:	and	[dword edx-12],0
	ret
@@3:	mov	eax,[edx-16]
	cmp	eax,edi
	jae	@@4
;v˝sledek je kratöÌ neû operandy
	mov	edi,eax
@@4:	mov	[edx-12],edi
;spoËti adresy zaË·tk˘
	mov	esi,[edx-4]
	mov	eax,[ebx-4]
	sub	eax,esi
	lea	ebx,[ebx+4*eax]
	mov	eax,[ecx-4]
	sub	eax,esi
	lea	ecx,[ecx+4*eax]
	dec	edi
;hlavnÌ cyklus
@@and:	mov	eax,[ebx+4*edi]
	and	eax,[ecx+4*edi]
	mov	[edx+4*edi],eax
	dec	edi
	jns	@@and
;normalizace
	mov	edi,edx
	call	norm
	ret
endp	ANDU0
;-------------------------------------
;eax=function
proc	defrac
arg	a2,a1,a0
uses	esi,edi,ebx
	mov	edi,[a0]
	mov	ebx,[a1]
	mov	esi,[a2]
	cmp	[dword ebx-12],-2
	jz	@@f
	cmp	[dword esi-12],-2
	jz	@@f
	push	esi
	push	ebx
	push	edi
	call	eax
	ret
@@f:	push	eax
	call	fractonewx2
	mov	esi,ebx
	mov	ebx,eax
	call	fractonewx2
	mov	esi,eax
	pop	eax
	push	ebx
	push	esi
	push	edi
	call	eax
	mov	eax,esi
	call	@freex
	mov	eax,ebx
	call	@freex
	ret
endp	defrac

ORU0:	mov	al,0
	jmp	ORXORU0
XORU0:	mov	al,1
	jmp	ORXORU0
ORU:	lea	eax,[ORU0]
	jmp	defrac
XORU:	lea	eax,[XORU0]
	jmp	defrac
ANDU:	lea	eax,[ANDU0]
	jmp	defrac
;-------------------------------------
proc	@negx
	xor	[byte eax-8],1
	ret
endp	@negx

proc	@absx
	and	[byte eax-8],0
	ret
endp	@absx

proc	@signx
	cmp	[dword eax-12],0
	jz	@@ret
	mov	[dword eax-12],-2
	mov	[dword eax-4],1
	mov	[dword eax],1
	mov	[dword eax+4],1
@@ret:	ret
endp	@signx

proc	@scalex
	test	edx,edx
	jz	@@ret
	push	edx
	push	eax
	call	@fractox
	pop	eax
	pop	edx
@@1:	add	[dword eax-4],edx
	jo	_overflow
@@ret:	ret
endp	@scalex
;-------------------------------------
;o¯ÌznutÌ desetinnÈ Ë·sti
proc	@truncx
	mov	edx,[eax-4]
	test	edx,edx
	js	@zerox
	cmp	edx,[eax-12]
	jae	@@ret
	cmp	[dword eax-12],-2
	jz	truncf
	mov	[eax-12],edx
@@ret:	ret
;zlomek
truncf:	mov	ecx,eax
	mov	eax,[ecx]
	xor	edx,edx
	div	[dword ecx+4]
	mov	[ecx],eax
	mov	[dword ecx+4],1
	mov	[dword ecx-4],1
	test	eax,eax
	jnz	@@ret
	mov	[dword ecx-12],eax
	ret
endp	@truncx

;o¯ÌznutÌ celÈ Ë·sti
proc	@fracx
	cmp	[dword eax-12],-2
	jnz	@@1
;zlomek
	mov	ecx,eax
	mov	eax,[ecx]
	xor	edx,edx
	div	[dword ecx+4]
	test	edx,edx
	mov	eax,ecx
	jz	@zerox
	mov	[ecx],edx
	call	fractox
	ret
@@1:	mov	edx,[eax-4]
	test	edx,edx
	js	@@ret
	jz	@@ret
	mov	ecx,[eax-12]
	sub	ecx,edx
	jbe	@zerox
	mov	[eax-12],ecx
	sub	[eax-4],edx  ; =0
	push	edi
	push	esi
	mov	edi,eax
	lea	esi,[eax+4*edx]
	rep movsd
	mov	edi,eax
	call	norm
	pop	esi
	pop	edi
@@ret:	ret
endp	@fracx
;-------------------------------------
;zaokrouhlenÌ na celÈ ËÌslo
proc	@roundx
	mov	edx,[eax-4]
	test	edx,edx
	js	@zerox  ;z·porn˝ exponent -> v˝sledek nula
	cmp	edx,[eax-12]
	jl	@@1
;integer or fraction
	cmp	[dword eax-12],-2
	jnz	@@ret
	push	[dword eax+4]
	call	truncf
	pop	eax
	shl	edx,1   ;remainder
	jc	@@i
	cmp	eax,edx
	ja	@@ret
@@i:	inc	[dword ecx]
	cmp	[dword ecx-12],0
	jnz	@@ret
	mov	[dword ecx-12],-2
@@ret:	ret
@@1:	push	esi
	push	edi
	mov	[eax-12],edx   ;zkraù dÈlku
	mov	edi,eax
	mov	eax,[edi+4*edx]
	test	eax,eax
	jns	@@e
;zaokrouhli
	lea	esi,[edi+4*edx-4]
	call	incr
@@e:	pop	edi
	pop	esi
	ret
endp	@roundx
;-------------------------------------
proc	@minus1
	mov	edx,1
	mov	[eax+4],edx
	mov	[eax],edx
	mov	[eax-4],edx
	mov	[eax-8],edx
	mov	[dword eax-12],-2
	ret
endp
;-------------------------------------
;zaokrouhlenÌ smÏrem dol˘
proc	@intx
	cmp	[byte eax-8],0
	jz	@truncx
;operand je z·porn˝
	cmp	[dword eax-12],0
	jg	@@1
	jz	@@ret
;fraction
	call	truncf
	test	edx,edx
	jz	@@ret ;integer
	inc	[dword ecx]
	cmp	[dword ecx-12],0
	jnz	@@ret
	mov	[dword ecx-12],-2
@@ret:	ret
@@1:	mov	edx,[eax-4]
	test	edx,edx
	js	@minus1
	jz	@minus1
	push	edi
	push	esi
	mov	edi,eax
	call	trim
	mov	edx,[edi-4]
	cmp	edx,[edi-12]
	jge	@@e
	mov	[edi-12],edx
	lea	esi,[edi+4*edx-4]
	call	incr
@@e:	pop	esi
	pop	edi
	ret
endp	@intx

;zaokrouhlenÌ smÏrem nahoru
proc	@ceilx
	xor	[byte eax-8],1
	push	eax
	call	@intx
	pop	eax
	xor	[byte eax-8],1
	ret
endp	@ceilx
;-------------------------------------
;[edi]*= esi
proc	mult1
	cmp	esi,1
	ja	@@z
	jz	@@ret
	and	[dword edi-12],0
	ret
@@z:	cmp	[dword edi-12],-2
	jnz	@@0
;zlomek
	lea	edi,[edi+4]
	push	esi
	mov	esi,esp
	call	reduce
	pop	esi
	lea	edi,[edi-4]
	mov	eax,[edi]
	mul	esi
	jo	@@f1
	mov	[edi],eax
	call	fracexp
	jmp	@@ret
;convert fraction to real
@@f1:	mov	eax,edi
	call	@fractox
@@0:	xor	ebx,ebx   ;p¯enos
	mov	ecx,[edi-12]
	dec	ecx
	js	@@ret
@@lp:	mov	eax,[edi+4*ecx]
	mul	esi
	add	eax,ebx
	mov	ebx,edx
	mov	[edi+4*ecx+4],eax
	adc	ebx,0
	dec	ecx
	jns	@@lp
	mov	[edi],ebx
	test	ebx,ebx
	jnz	@@1
;zruö nulov˝ p¯enos
	mov	ecx,[edi-12]
	lea	esi,[edi+4]
	cld
	rep movsd
@@ret:	ret
@@1:	inc	[dword edi-4]
	jno	roundi
	jmp	_overflow
endp	mult1

;a0*=ai
proc	MULTI1
arg	ai,a0
uses	esi,edi,ebx
	mov	edi,[a0]
	mov	esi,[ai]
	call	mult1
	ret
endp	MULTI1
;-------------------------------------
;a0:=a1*ai
;a0 m˘ûe b˝t rovno a1
proc	MULTI
arg	ai,a1,a0
uses	esi,edi,ebx
	mov	edi,[a0]
multif:	mov	esi,[a1]
	mov	ecx,[esi-12]
	cmp	ecx,-2
	jnz	@@0
;zlomek
	call	copyx
	mov	esi,[ai]
	call	mult1
	ret
;okopÌruj znamÈnko a exponent
@@0:	mov	eax,[esi-4]
	mov	[edi-4],eax
	mov	edx,[esi-8]
	mov	[edi-8],edx
;nastav dÈlku v˝sledku
	mov	eax,[edi-16]
	cmp	ecx,eax
	jbe	@@1
	mov	ecx,eax
@@1:	mov	[edi-12],ecx
	add	edi,4
	dec	ecx
	js	@@ret    ;operand je 0
	xor	ebx,ebx  ;p¯enos
;hlavnÌ cyklus
@@lp:	mov	eax,[esi+4*ecx]
	mul	[ai]
	add	eax,ebx
	mov	[edi+4*ecx],eax
	mov	ebx,edx
	adc	ebx,0
	dec	ecx
	jns	@@lp
;zapiö p¯eteËenÌ
	sub	edi,4
	mov	[edi],ebx
	test	ebx,ebx
	jnz	@@2
;zruö nulov˝ p¯enos
	mov	ecx,[edi-12]
	lea	esi,[edi+4]
	cld
	rep movsd
	ret
@@2:	inc	[dword edi-4]
	jo	_overflow
	call	roundi
@@ret:	ret
endp	MULTI

proc	MULTIN
arg	ai,a1,a0
uses	esi,edi,ebx
	mov	eax,[ai]
	mov	edi,[a0]
	test	eax,eax
	jns	multif
	neg	eax
	push	eax
	push	[a1]
	push	edi
	call	MULTI
	xor	[dword edi-8],1
	ret
endp	MULTIN
;-------------------------------------
proc	DIVX2
arg	a2,a1,a0
local	d1,d2,d,t1,t2
uses	esi,edi,ebx
	xor	esi,esi
	mov	ebx,[a1]	;dÏlenec
	mov	ecx,[a2]	;dÏlitel
	mov	edx,[a0]	;v˝sledek
	cmp	[dword ebx-12],-2
	jnz	@@f1
	cmp	[dword ecx-12],-2
	jnz	@@f0
;(a/b)/(c/d)
	mov	eax,[ecx-8]
	xor	eax,[ebx-8]
	mov	[edx-8],eax
	push	[dword ebx]   ;a
	push	[dword ecx+4] ;d
	push	[dword ebx+4] ;b
	push	[dword ecx]   ;c
	lea	esi,[esp]     ;c
	lea	edi,[esp+12]  ;a
	call	reduce
	lea	esi,[esp+4]   ;b
	lea	edi,[esp+8]   ;d
	call	reduce
	mov	edi,[a0]
	pop	eax
	pop	ecx
	mul	ecx	;b*c
	jo	@@fe1
	mov	[edi+4],eax
	pop	eax
	pop	ecx
	mul	ecx	;a*d
	jo	@@fe2
	mov	[edi],eax
	call	fracexp
	ret
;zlomek p¯etekl
@@fe1:	add	esp,8
@@fe2:	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	edx,edi
	jmp	@@f3
;(a/b)/x
@@f0:	sub	esp,HED+8
;a uloû na z·sobnÌk
	lea	eax,[esp+HED]
	mov	[dword eax-16],1
	mov	[dword eax-12],1
	mov	[dword eax-4],1
	mov	edx,[ebx-8]
	mov	[eax-8],edx
	mov	edx,[ebx]
	mov	[eax],edx
;a/x
	push	ecx
	push	eax
	push	[a0]
	call	DIVX
	add	esp,HED+8
;(a/x)/b
	push	[dword ebx+4]
	push	[a0]
	push	[a0]
	call	DIVI
	ret
@@f1:	cmp	[dword ecx-12],-2
	jnz	@@f2
;x/(a/b)
@@f3:	push	[dword ecx+4]
	push	[dword ecx]
;p¯ekopÌruj dÏlenec do v˝sledku
	mov	edi,edx
	mov	esi,ebx
	call	copyx
;znamÈnko
	mov	ecx,[a2]
	mov	eax,[ecx-8]
	xor	[edi-8],eax
	push	edi
	push	edi
	call	DIVI	;x/a
	pop	esi
	call	mult1	;(x/a)*b
	ret
;operandy nejsou zlomky
@@f2:	cmp	[dword ecx-12],0
	jnz	@@tz
;division by zero
	lea	eax,[E_1010]
	push	eax
	push	1010
	call	_cerror
	add	esp,8
	jmp	@@ret
@@tz:	cmp	[ebx-12],esi
	mov	[edx-12],esi
	jz	@@ret		; dÏlÏnec je nula
	cmp	[dword ecx-12],1
	ja	@@1
;jednocifern˝ dÏlitel
	push	[dword ecx]
	push	ebx
	push	edx
	call	DIVI
	mov	ecx,[a2]
	mov	edx,[a0]
	mov	eax,[ecx-8]	;znamÈnko dÏlitele
	xor	[edx-8],eax
	mov	eax,[ecx-4]	;exponent dÏlitele
	dec	eax
	sub	[edx-4],eax
	jo	_overflow
	ret
;znamÈnko
@@1:	mov	eax,[ecx-8]
	push	eax
	xor	eax,[ebx-8]
	mov	[edx-8],eax
	mov	[ecx-8],esi	;kladn˝ dÏlitel
;exponent
	mov	edi,[ecx-4]
	mov	eax,[ebx-4]
	push	edi
	inc	eax
	sub	eax,edi
	mov	[ecx-4],esi	;dÏlitel bez exponentu
	mov	[edx-4],eax
	jno	@@2
	call	_overflow
	jmp	@@r
@@2:	mov	eax,[ebx-12]
	mov	edi,[ecx-12]
	cmp	eax,edi
	jbe	@@6
	mov	edi,eax
@@6:	add	edi,2
;alokuj pomocnou promÏnnou t2
	mov	eax,[ecx-12]
	inc	eax
	call	@allocx
	mov	[t2],eax
;alokuj pomocnou promÏnnou t1
	mov	eax,edi
	call	@allocx
	mov	[t1],eax
;alokuj zbytek
	mov	eax,edi
	call	@allocx
;edi:=zbytek, esi:=dÏlÏnec, ebx:=dÏlitel
	mov	esi,ebx
	mov	edi,eax
	mov	ebx,[a2]
;vyn·sob dÏlenec i dÏlitel, aby dÏlitel zaËÌnal velkou cifrou
	xor	eax,eax
	mov	edx,1
	mov	ecx,[ebx] ;prvnÌ cifra dÏlitele
	inc	ecx
	jz	@@c
	div	ecx
	cmp	eax,1
	jz	@@c
	push	eax
	mov	eax,[ebx-12]
	call	@allocx
	mov	[d],eax
	pop	ecx
	push	ecx
	push	ebx
	push	eax
	mov	ebx,eax
	push	ecx
	push	esi
	push	edi
	call	MULTI
	call	MULTI
	jmp	@@12
;p¯ekopÌruj do zbytku dÏlenec
@@c:	call	copyx
	mov	[d],0
;exponent zbytku
@@12:	mov	eax,[edi-4]
	sub	eax,[esi-4]
	mov	[edi-4],eax
	mov	[dword edi-8],0
;edi:=v˝sledek, esi:=zbytek
	mov	esi,edi
	mov	edi,[a0]
;porovn·nÌ dÏlence a dÏlitele
	mov	ecx,esi
	mov	edx,ebx
	call	cmpu
	jna	@@3
	inc	[dword esi-4]
	dec	[dword edi-4]
;nastav dÈlku v˝sledku
@@3:	mov	ecx,[edi-16]
	mov	[edi-12],ecx
	push	ecx	;poËÌtadlo for cyklu
;[d1]:= prvnÌ cifra dÏlitele
	mov	eax,[ebx]
	mov	[d1],eax
;[d2]:= druh· cifra dÏlitele
	mov	eax,[ebx+4]
	mov	[d2],eax
;hlavnÌ cyklus, esi=zbytek, ebx=dÏlitel
;edi=pr·vÏ poËÌtan· cifra v˝sledku
@@lp:	xor	edx,edx
	cmp	[esi-12],edx
	jz	@@e	;zbytek je 0
;naËti nejvyööÌ ¯·d dÏlence do edx:eax
	mov	eax,[esi]
	cmp	[esi-4],edx
	jz	@@4
	mov	[edi],edx ;0
	jl	@@w	;zbytek < dÏlitel
	mov	edx,eax
	xor	eax,eax
	cmp	[dword esi-12],1
	jz	@@4	;jednocifern˝ zbytek
	mov	eax,[esi+4]
@@4:	cmp	edx,[d1]
	jb	@@9
;v˝sledek dÏlenÌ je vÏtöÌ neû 32-bit.
	mov	ecx,eax
	stc
	sbb	eax,eax	;0xFFFFFFFF
	add	ecx,[d1]
	jc	@@8
	jmp	@@10
;odhadni v˝slednou cifru
@@9:	div	[d1]
	mov	ecx,edx	;zbytek dÏlenÌ
@@10:	push	eax	;uchovej v˝slednou cifru
	mul	[d2]
	sub	edx,ecx
	jc	@@11
	mov	ecx,[esi-4]
	inc	ecx
	cmp	[dword esi-12],ecx
	jbe	@@f	;jednocifern˝ nebo dvoucifern˝ zbytek
	sub	eax,[esi+4*ecx]
	sbb	edx,0
	jc	@@11
;dokud je zbytek z·porn˝, sniûuj v˝slednou cifru
@@f:	mov	ecx,eax
	or	ecx,edx
	jz	@@11
	dec	[dword esp]
	sub	eax,[d2]
	sbb	edx,[d1]
	jnc	@@f
@@11:	pop	eax
;od zbytku odeËti dÏlitel * eax
@@8:	push	eax
	push	ebx
	push	[t2]
	mov	[edi],eax
	call	MULTI
	push	esi
	push	[t2]
	push	esi
	push	[t1]
	call	MINUSX
	mov	esi,[t1]
	pop	[t1]
;oprav v˝sledek, aby zbytek nebyl z·porn˝
	cmp	[byte esi-8],0
	jz	@@w	;zbytek je kladn˝
	cmp	[dword esi-12],0
	jz	@@w	;zbytek je nulov˝
@@d:	dec	[dword edi]
	push	esi
	push	ebx
	push	esi
	push	[t1]
	call	PLUSX
	mov	esi,[t1]
	pop	[t1]
;pro jistotu jeötÏ zkontroluj znamÈnko zbytku
	cmp	[dword esi-12],0
	jz	@@w	;zbytek je nulov˝
	cmp	[byte esi-8],0
	jnz	@@d	;tento skok se asi nikdy neprovede
;posun na dalöÌ cifru v˝sledku
@@w:	add	edi,4
	inc	[dword esi-4]
	pop	ecx
	dec	ecx
	push	ecx
	jnz	@@lp
;oprav dÈlku v˝sledku (p¯i nulovÈm zbytku)
@@e:	pop	ecx
	mov	eax,[a0]
	sub	[eax-12],ecx
;smaû pomocnÈ promÏnnÈ
	mov	eax,esi
	call	@freex
	mov	eax,[t1]
	call	@freex
	mov	eax,[t2]
	call	@freex
	mov	eax,[d]
	call	@freex
;obnov znamÈnko a exponent dÏlitele
@@r:	mov	ebx,[a2]
	pop	[dword ebx-4]
	pop	[dword ebx-8]
@@ret:	ret
endp	DIVX2
;-------------------------------------
proc	MULTX1
arg	a2,a1,a0
uses	esi,edi,ebx
local	e2
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
	xor	esi,esi
	mov	[edx-12],esi
	cmp	[ebx-12],esi
	jz	@@ret
	cmp	[ecx-12],esi
	jz	@@ret
	cmp	[dword ebx-12],-2
	jnz	@@f1
;fraction
@@f0:	sub	esp,HED+8
	lea	eax,[esp+HED]
	mov	[dword eax-16],2
	mov	[dword eax-12],-2
	mov	edi,[ebx-8]
	mov	[eax-8],edi
	mov	edi,[ebx]
	mov	[eax+4],edi
	mov	edi,[ebx+4]
	mov	[eax],edi
	push	eax
	push	ecx
	push	edx
	call	DIVX2
	add	esp,HED+8
	ret
@@f1:   cmp	[dword ecx-12],-2
	jnz	@@f2
	mov	eax,ecx
	mov	ecx,ebx
	mov	ebx,eax
	jmp	@@f0
;spoËti znamÈnko
@@f2:	mov	eax,[ebx-8]
	xor	eax,[ecx-8]
	mov	[edx-8],eax
;spoËti v˝sledn˝ exponent
	mov	eax,[ebx-4]
	dec	eax
	add	eax,[ecx-4]
	jno	@@1
	call	_overflow
	jmp	@@ret
@@1:	mov	[edx-4],eax	;exponent
;zjisti dÈlku v˝sledku
	mov	eax,[edx-16]
	add	eax,10          ;zvÏtöi dÈlku kv˘li p¯esnosti
	mov	[edx-16],eax
	mov	[edx-12],eax
	mov	esi,[ebx-12]
	add	esi,[ecx-12]
	dec	esi
	cmp	esi,eax
	jnb	@@2
;v˝sledek nem˘ûe b˝t delöÌ neû souËet dÈlek operand˘
	mov	[edx-12],esi
@@2:	mov	eax,[ecx-12]
	lea	eax,[ecx+4*eax]
	mov	[e2],eax
;vynuluj v˝sledek
	mov	ecx,[edx-12]
	mov	edi,edx
	xor	eax,eax
	cld
	rep stosd
;esi:= poË·teËnÌ pozice v 1.operandu
	mov	ecx,[a2]
	mov	esi,[edx-12]
	mov	eax,[ebx-12]
	lea	edi,[edx+4*esi-4]
	cmp	eax,esi
	lea	esi,[ebx+4*esi-4]
	jnc	@@7
	lea	esi,[ebx+4*eax-4]
;ebx:= poË·teËnÌ pozice v 2.operandu
@@7:	mov	eax,[edx-12]
	sub	eax,[ebx-12]
	lea	ebx,[ecx+4*eax]
	cmp	ebx,ecx
	jnc	@@5
	mov	ebx,ecx
;vnÏjöÌ cyklus p¯es 1.operand
@@5:	push	ebx
	push	esi
	push	edi
	xor	ecx,ecx		;p¯enos
;vnit¯nÌ cyklus p¯es 2.operand
@@lp:	mov	eax,[esi]
	mul	[dword ebx]
	add	eax,ecx
	mov	ecx,edx
	adc	ecx,0
	add	[edi],eax
	adc	ecx,0
	sub	edi,4
	sub	ebx,4
	cmp	ebx,[a2]
	jnc	@@lp

	cmp	edi,[a0]
	jc	@@e
;zapiö p¯enos
	add	[edi],ecx
	pop	edi
	pop	esi
	pop	ebx
;posuÚ se na dalöÌ ¯·d
	sub	esi,4
	add	ebx,4
	cmp	ebx,[e2]
	jnz	@@5
@@6:	sub	edi,4
	sub	ebx,4
	jmp	@@5
;konec
@@e:	pop	edi
	pop	esi
	pop	ebx
	mov	edi,[a0]
	mov	eax,ecx
	push	edi
	call	carry
	pop	edi
	sub	[dword edi-16],10
	call	round
@@ret:	ret
endp	MULTX1
;-------------------------------------
;vracÌ zbytek dÏlenÌ 32-bitov˝m ËÌslem
;nem· smysl pro re·ln˝ operand
proc	MODI
arg	ai,a1
uses	esi,ebx
	mov	esi,[a1]
	mov	ebx,[ai]
	test	ebx,ebx
	jnz	@@0
	lea	eax,[E_1010]
	push	eax
	push	1010
	call	_cerror
	add	esp,8
	ret
@@0:	mov	ecx,[esi-12]
	xor	edx,edx
	test	ecx,ecx
	jns	@@1
;32bit
	mov	eax,[esi]
	div	ebx
	jmp	@@r
@@1:	jz	@@r     ; 0 mod ai=0
;hlavnÌ cyklus
@@lp:	mov	eax,[esi]
	add	esi,4
	div	ebx
	dec	ecx
	jnz	@@lp
	test	edx,edx
	jz	@@r   ;nulov˝ zbytek
	mov	esi,[a1]
	mov	ecx,[esi-4]
	sub	ecx,[esi-12]
	jle	@@r
;za vstupnÌ operand doplÚuj nuly a pokraËuj v dÏlenÌ
@@lp2:	xor	eax,eax
	div	ebx
	dec	ecx
	jnz	@@lp2
@@r:	mov	eax,edx
	ret
endp	MODI
;-------------------------------------
;vypÌöe ËÌslo do bufferu
;neumÌ zobrazit exponent -> Ë·rka musÌ b˝t uvnit¯ mantisy !!!
proc	WRITEX1
arg	a1,buf
uses	esi,edi,ebx
	mov	esi,[a1]
	mov	eax,[esi-12]
	mov	edx,[buf]
	test	eax,eax
	jnz	@@1
;nula
	mov	[byte edx],48
	inc	edx
	mov	[byte edx],0
	ret
;znamÈnko
@@1:	cmp	[dword esi-8],0
	jz	@@5
	mov	[byte edx],45  ;'-'
	inc	edx
	mov	[buf],edx
@@5:	mov	[byte edx],0
;okopÌruj operand
	mov	eax,[a1]
	call	@newcopyx
	push	eax
	mov	edi,eax
	mov	eax,[edi-4]
	test	eax,eax
	js	@@f  ;Ë·rka je vlevo od mantisy
	cmp	eax,[edi-16]
	jg	@@f  ;Ë·rka je vpravo od mantisy
	mov	edx,[edi-12]
	sub	eax,edx
	jle	@@3
;vynuluj Ë·st za mantisou
	mov	ecx,eax
	push	edi
	lea	edi,[edi+4*edx]
	xor	eax,eax
	rep stosd
	pop	edi
;alokuj buffer pro meziv˝sledek vlevo od teËky
@@3:	mov	ecx,[edi-4]
	inc	ecx
	shl	ecx,2
	lea	eax,[ecx+2*ecx] ; *12
	cmp	[_base],8
	jnc	@@a
	lea	eax,[ecx*8] ; *32
@@a:	push	eax
	call	_Alloc
	mov	esi,eax
	mov	[esp],eax
;esi=buf, edi=x, obÏ hodnoty jsou i na z·sobnÌku
	mov	ecx,[edi-4]
	test	ecx,ecx
	jnz	@@high
;p¯ed teËkou je nula
	mov	eax,[buf]
	mov	[byte eax],48  ;'0'
	inc	eax
	jmp	@@tecka

;Ë·st vlevo od teËky
@@high:	mov	edx,[_base]
	mov	ebx,[dwordMax+edx*4]
@@h1:	push	ecx
	push	edi
	test	ebx,ebx
	jnz	@@h2
	mov	edx,[edi+4*ecx-4]
	jmp	@@h3
;vydÏl ËÌslo p¯ed teËkou mocninou z·kladu
@@h2:	xor	edx,edx
@@lpd:	mov	eax,[edi]
	div	ebx
	mov	[edi],eax
	add	edi,4
	dec	ecx
	jnz	@@lpd
;edx=skupina ËÌslic
@@h3:	mov	edi,[_base]
	mov	eax,edx
	mov	cl,[dwordDigitsI+edi]
@@2:	xor	edx,edx
	div	edi
;zbytek dÏlenÌ je v˝sledn· ËÌslice
@@d:	mov	dl,[_digitTab+edx]
	mov	[esi],dl
	inc	esi
	dec	cl
	jnz	@@2
;p¯esun na dalöÌ Ëty¯byte
	pop	edi
	pop	ecx
	test	ebx,ebx
	jz	@@dc
	mov	eax,[edi]
	test	eax,eax
	jnz	@@h1
	add	edi,4
@@dc:	dec	ecx
	jnz	@@h1
;u¯Ìzni poË·teËnÌ nuly
@@tr:	dec	esi
	cmp	[byte esi],48
	jz	@@tr
;obraù v˝sledek
	mov	eax,[buf]
@@rev:	mov	cl,[esi]
	mov	[eax],cl
	inc	eax
	dec	esi
	cmp	esi,[esp]
	jae	@@rev
;eax=ukazatel za celou Ë·st v˝sledku
@@tecka:
	mov	edi,[esp+4]
	mov	edx,[edi-4]
	mov	ecx,[edi-12]
	mov	ebx,[edi-16]
	lea	edi,[edi+4*edx]
	sub	ecx,edx
	jle	@@end  ;celÈ ËÌslo
;edi=desetinn· Ë·st okopÌrovanÈho vstupu,  ecx=jejÌ dÈlka
;vypiö teËku
	mov	[byte eax],46  ;'.'
	inc	eax
	mov	esi,eax
;odhadni poËet cifer
	push	ebx
	mov	eax,[_base]
	fild	[dword esp]
	fld	[qword _dwordDigits+8*eax]
	fmul
	fistp	[dword esp]
	pop	edx
	sub	edx,5
	dec	ecx
;Ë·st vpravo od teËky
;edx=poËet cifer, esi=v˝stup, edi=ukazatel na desetinnou Ë·st, ecx=dÈlka-1
@@low:	push	edx
	push	edi
	push	ecx
	mov	eax,[_base]
	mov	eax,[dwordMax+eax*4]
	test	eax,eax
	jnz	@@low1
	mov	ebx,[edi]
	add	[dword esp+4],4
	dec	[dword esp]
	jmp	@@low2
@@low1: push	esi
;n·sobenÌ mocninou z·kladu
	mov	esi,eax
	xor	ebx,ebx  ;carry
@@lpm:	mov	eax,[edi+4*ecx]
	mul	esi
	add	eax,ebx
	mov	[edi+4*ecx],eax
	mov	ebx,edx
	adc	ebx,0
	dec	ecx
	jns	@@lpm
	pop	esi
;ebx=skupina ËÌslic
@@low2:	mov	eax,ebx
	mov	edi,[_base]
	movzx	ecx,[dwordDigitsI+edi]
	mov	ebx,ecx
@@d2:	xor	edx,edx
	div	edi
;zbytek dÏlenÌ je v˝sledn· ËÌslice
	mov	dl,[_digitTab+edx]
	dec	ecx
	mov	[esi+ecx],dl
	jnz	@@d2
	add	esi,ebx
	pop	ecx
	pop	edi
	pop	edx
;poËÌtadlo cyklu
	sub	edx,ebx
	ja	@@tc
	add	esi,edx
	jmp	@@lend
@@tc:	test	ecx,ecx
	js	@@lend
	mov	eax,[_error]
	test	eax,eax
	jnz	@@lend
;u¯Ìzni koncovÈ nuly vstupnÌho operandu
	mov	eax,[edi+4*ecx]
	test	eax,eax
	jnz	@@low
	dec	ecx
	jns	@@low
@@lend:	mov	eax,esi
;zapiö koncovou nulu
@@end:	mov	[byte eax],0
;smaû pomocnÈ buffery
	call	_Free
	pop	edx
@@f:	pop	eax
	call	@freex
	ret
endp	WRITEX1
;-------------------------------------
;znak [esi] p¯evede na ËÌslo ebx
;p¯i chybÏ vracÌ CF=1
proc	rdDigit
	xor	ebx,ebx
	mov	bl,[byte esi]
	sub	bl,48 ; '0'
	jc	@@ret
	cmp	bl,10
	jc	@@1
	sub	bl,7  ; 'A'-'0'-10
	cmp	bl,10
	jc	@@ret
	cmp	bl,42
	jc	@@1
	sub	bl,32 ; 'a'-'A'
@@1:	cmp	ebx,[_baseIn]
	cmc
@@ret:	ret
endp	rdDigit
;-------------------------------------
;naËte kr·tkÈ desetinnÈ ËÌslo a p¯evede ho na zlomek
;vracÌ CY=1, pokud je ËÌslo moc velkÈ, jinak [eax]=konec vstupu
;[edi]=v˝sledek, [esi]=vstup
;mÏnÌ ebx
proc	rdFrac
uses	esi
	cmp	[dword edi-16],2
	jc	@@ret
	mov	ecx,-1  ;dÈlka desetinnÈ Ë·sti
;spoËti Ëitatel
	xor	eax,eax
@@lpn:	test 	ecx,ecx
	jge	@@1
	cmp	[byte esi],46  ;'.'
	jnz	@@3
	inc	esi
@@1:	inc	ecx
@@3:	call	rdDigit
	jc	@@4
	inc	esi
	mov	edx,[_baseIn]
	mul	edx
	test	edx,edx
	jnz	@@e
	add	eax,ebx
	jnc	@@lpn
	jmp	@@e
@@4:	mov	[edi],eax
;spoËti jmenovatel
	mov	eax,1
	mov	ebx,[_baseIn]
@@lpd:	dec	ecx
	js	@@2
	mul	ebx
	test	edx,edx
	jz	@@lpd
@@e:	stc
	ret
@@2:	mov	[edi+4],eax
	call	fracReduce
	mov	eax,esi
	clc
@@ret:	ret
endp	rdFrac
;-------------------------------------
proc	READX1
arg	buf,a1
uses	esi,edi,ebx
	mov	edi,[a1]
	mov	esi,[buf]
;vyplÚ dÈlku a vynuluj exponent
	mov	eax,[edi-16]
	xor	ecx,ecx
	mov	[edi-12],eax
	mov	[edi-4],ecx
	cmp	[esi],cl
	jnz	@@1
;pr·zdn˝ ¯etÏzec => nula
	mov	[edi-12],ecx
	mov	eax,esi
@@ret:	ret
;znamÈnko
@@1:	cmp	[byte esi],45	; -
	mov	[edi-8],ecx
	jnz	@@5
	inc	esi
	inc	[byte edi-8]
@@5:	cmp	[byte esi],43	; '+'
	jnz	@@f
	inc	esi
@@f:	call	rdFrac
	jnc	@@ret
;Ë·st nalevo od teËky
	xor	ecx,ecx
;naËti dalöÌ ËÌslici
@@m0:	call	rdDigit
	jc	@@me
	inc	esi
	test	ecx,ecx
	jz	@@m1
;n·sobenÌ z·kladem a p¯iËtenÌ ebx
	push	edi
	push	esi
	push	ecx
	lea	edi,[edi+4*ecx]
	neg	ecx
	mov	esi,[_baseIn]
@@lpm:	mov	eax,[edi+4*ecx]
	mul	esi
	add	eax,ebx
	mov	[edi+4*ecx],eax
	mov	ebx,edx
	adc	ebx,0
	inc	ecx
	jnz	@@lpm
	pop	ecx
	pop	esi
	pop	edi
	test	ebx,ebx
	jz	@@m0
;zapiö p¯eteËenÌ a zvÏtöi velikost v˝sledku
@@m1:	mov	[edi+4*ecx],ebx
	inc	ecx
	inc	[dword edi-4]
	cmp	ecx,[edi-12]
	jbe	@@m0
;ztr·ta p¯esnosti, vstupnÌ ¯etÏzec je moc dlouh˝
	dec	ecx
	push	edi
	push	esi
	push	ecx
	lea	esi,[edi+4]
	cld
	rep movsd
	pop	ecx
	pop	esi
	pop	edi
	jmp	@@m0
@@me:	push	esi
;obr·cenÌ v˝sledku
	lea	esi,[edi+4*ecx-4]
	jmp	@@rev1
@@rev:	mov	eax,[esi]
	mov	ebx,[edi]
	mov	[edi],eax
	mov	[esi],ebx
	add	edi,4
	sub	esi,4
@@rev1:	cmp	edi,esi
	jb	@@rev
	pop	esi
;teËka
	mov	edi,[a1]
	mov	al,[esi]
	mov	[edi-12],ecx
	cmp	al,46	; '.'
	jnz	@@end  ;celÈ ËÌslo
;dojeÔ na konec, a potom Ëti ËÌslo pozp·tku
@@sk:	inc	esi
	call	rdDigit
	jnc	@@sk
 	push	esi  ;n·vratov· hodnota
;desetinn· ËÌsla majÌ maxim·lnÌ dÈlku
	mov	edx,[edi-16]
	xor	eax,eax
	mov	[edi-12],edx
	push	edi  ;ukazatel na v˝sledek
;vynuluj desetinnou Ë·st v˝sledku
	sub	edx,ecx
	test	edx,edx
	jz	@@de
	lea	edi,[edi+4*ecx]
	mov	ecx,edx
	push	edi
	cld
	rep stosd
;edi:=ukazatel na desetinnou Ë·st, ecx:=jejÌ dÈlka, esi:=konec vstupu
	pop	edi
	mov	ecx,edx
	mov	esi,[esp+4]
;naËti dalöÌ ËÌslici do edx
@@d0:	dec	esi
	call	rdDigit
	jc	@@de
	mov	edx,ebx
;dÏlenÌ z·kladem
	push	ecx
	push	edi
	mov	ebx,[_baseIn]
@@lpd:	mov	eax,[edi]
	div	ebx
	mov	[edi],eax
	add	edi,4
	dec	ecx
	jnz	@@lpd
	pop	edi
	pop	ecx
        jmp	@@d0
;u¯ÌznutÌ koncov˝ch nul a normalizace
@@de:	pop	edi
@@trim:	call	norm
	pop	eax
	ret
@@end:	push	esi
	jmp	@@trim
endp	READX1
;-------------------------------------
proc	FACTORIALI
arg	ai,a0
uses	esi,edi,ebx
	mov	eax,[a0]
	call	@onex
	mov	esi,[ai]
	test	esi,esi
	jz	@@ret     ;0! =1
@@lp:	push	esi
	mov	edi,[a0]
	call	mult1
	pop	esi
	mov	eax,[_error]
	test	eax,eax
	jnz	@@ret
	dec	esi
	jnz	@@lp
@@ret:	ret
endp	FACTORIALI

proc	FFACTI
arg	ai,a0
uses	esi,edi,ebx
	mov	eax,[a0]
	call	@onex
	mov	esi,[ai]
	test	esi,esi
	jz	@@ret     ;0!! =1
@@lp:	push	esi
	mov	edi,[a0]
	call	mult1
	pop	esi
	mov	eax,[_error]
	test	eax,eax
	jnz	@@ret
	dec	esi
	jz	@@ret
	dec	esi
	jnz	@@lp
@@ret:	ret
endp	FFACTI
;-------------------------------------
;[ecx]=eax^2
proc	sqr
	mul	eax
	test	edx,edx
	jnz	@@1
	mov	[ecx],eax
	mov	[ecx-4],edx
	mov	[dword ecx-12],1
	ret
@@1:	mov	[ecx],edx
	mov	[ecx+4],eax
	mov	[dword ecx-4],1
	mov	[dword ecx-12],2
	ret
endp	sqr
;-------------------------------------
; y=0;
; for(c=0x80000000; c>0; c>>=1){
;   c>>=1;
;   y+=c;
;   if(z>=y){
;     z-=y;
;     y+=c;
;   }else{
;     y-=c;
;   }
;   y>>=1;
; }
; return y;

;eax:=sqrt(edx:eax)
proc	@sqrti
uses	esi,edi
	mov	ecx,080000000h
	xor	edi,edi
	xor	esi,esi

@@0:	ror	ecx,1
	add	esi,ecx
	sub	eax,edi
	sbb	edx,esi
	jnc	@@1
	add	eax,edi
	adc	edx,esi
	sub	esi,ecx
	sub	esi,ecx
@@1:	add	esi,ecx
	shr	esi,1
	rcr	edi,1
	ror	ecx,1
	jnc	@@0

@@3:	ror	ecx,1
	add	edi,ecx
	sub	eax,edi
	sbb	edx,esi
	jnc	@@2
	add	eax,edi
	adc	edx,esi
	sub	edi,ecx
	sub	edi,ecx
@@2:	add	edi,ecx
	shr	esi,1
	rcr	edi,1
	ror	ecx,1
	jnc	@@3
	mov	eax,edi
	ret
endp	@sqrti

;odmocnina ze 64-bitovÈho ËÌsla
;zaokrouhluje smÏrem dol˘
proc	SQRTI
	mov	eax,[esp+4]
	mov	edx,[esp+8]
	call	@sqrti
	ret	8
endp	SQRTI
;-------------------------------------
proc	SQRTX2
arg	a1,a0
local	sq,s2,s3,s4,d1,d2,k,ro
uses	esi,edi,ebx
	mov	ecx,[a1]	;operand
	mov	edx,[a0]	;v˝sledek
	xor	esi,esi
	cmp	[ecx-12],esi
	mov	[edx-12],esi
	jz	@@ret		; sqrt(0)=0
	cmp	[ecx-8],esi
	jz	@@1
	lea	eax,[E_1008]
	push	eax
	push	1008
	call	_cerror
	add	esp,8
	jmp	@@ret
@@1:	cmp	[dword ecx-12],-2
	jnz	@@f2
;zlomek
	mov	edi,edx
	mov	esi,ecx
	mov	ecx,2
@@nd:	lea	ebx,[esi+ecx*4-4]
	push	0
	push	[dword ebx]
	fild	[qword esp]
	fsqrt
	fistp	[dword esp]
	pop	eax
	pop	edx
	lea	edx,[edi+ecx*4-4]
	mov	[edx],eax
	mul	eax
	cmp	eax,[ebx]
	jnz	@@fe
	dec	ecx
	jnz	@@nd
	and	[dword edi-8],0
	call	fracexp
	ret
@@fe:	call	fractonewx
	mov	esi,eax
	push	eax
	push	edi
	call	SQRTX
	mov	eax,esi
	call	@freex
	ret
;alokuj promÏnnou sq
@@f2:	sub	esp,8
	mov	[sq],esp
	sub	esp,HED
;znamÈnko
	mov	[esp+HED-8],esi	;kladnÈ
	mov	[edx-8],esi
;max(dÈlka v˝sledku+3, dÈlka operandu)+3
	mov	eax,[edx-16]
	mov	[edx-12],eax
	add	eax,3
	mov	edi,[ecx-12]
	cmp	eax,edi
	jbe	@@6
	mov	edi,eax
@@6:	add	edi,3
;alokuj pomocnou promÏnnou s2
	push	eax
	call	@allocx
	mov	[s2],eax
	pop	eax
;alokuj pomocnou promÏnnou s4
	call	@allocx
	mov	[s4],eax
;alokuj pomocnou promÏnnou s3
	mov	eax,edi
	call	@allocx
	mov	[s3],eax
;alokuj zbytek
	mov	eax,edi
	call	@allocx
	mov	edi,eax
	mov	esi,[a1]
;naËti nejvyööÌ ¯·d
	xor	edx,edx
	mov	ebx,[s2]
	mov	[ebx-4],edx
	test	[dword esi-4],1	;parita exponentu
	jz	@@2
	mov	eax,[esi]
	jmp	@@3
@@2:	mov	edx,[esi]
	mov	eax,[esi+4]
;vypoËti odmocninu z nejvyööÌho ¯·du
@@3:	call	@sqrti
	mov	ecx,eax
	mov	eax,80000000h
	test	ecx,ecx
	js	@@r
	xor	edx,edx
	inc	ecx
	div	ecx
	cmp	eax,1
	jbe	@@c
;zvÏtöi operand
@@r:	mov	[ro],eax
	mov	ecx,[sq]
	call	sqr
	push	[sq]
	push	esi
	push	edi
	call	MULTX
	jmp	@@12
@@c:	call	copyx
	mov	[ro],0
@@12:	mov	esi,edi
	mov	edi,[a0]
;exponent
	mov	eax,[esi-4]
	inc	eax
	sar	eax,1
	mov	[edi-4],eax
	mov	[dword esi-4],1
;zapiö prvnÌ ËÌslici do v˝sledku
	mov	edx,[esi]
	mov	eax,[esi+4]
	call	@sqrti
	mov	[edi],eax
;poËÌtadlo cyklu
	push	[dword edi-12]
;ebx=dÏlitel, edi=v˝sledek, esi=zbytek
;od zbytku odeËti druhou mocninu p¯idanÈ ËÌslice
@@lp:	mov	eax,[edi]
	test	eax,eax
	jz	@@ad	;nulovou ËÌslici neodeËÌtej od zbytku
	mov	ecx,[sq]
	call	sqr
	push	[sq]
	push	esi
	push	[s3]
	call	MINUSX
	mov	eax,[s3]
	cmp	[byte eax-8],0
	jz	@@0
	cmp	[dword eax-12],0
	jz	@@0
;zbytek je z·porn˝ ->  musÌ se opravit
	dec	[dword esi-4]
	dec	[dword ebx-4]
	jmp	@@d
@@0:	mov	[s3],esi
	mov	esi,eax
;posuÚ ukazatel na v˝sledek
@@ad:	mov	eax,[edi]	;pro v˝slednÈ zaokroulenÌ
	add	edi,4
;sniû poËÌtadlo
	pop	ecx
	dec	ecx
	js	@@z		;hotovo, podle eax zaokrouhli
	cmp	[dword esi-12],0
	jz	@@e		;zbytek je 0 -> zkraù v˝sledek
	push	ecx
;p¯idej dvojn·sobek dalöÌ ËÌslice k s2
	mov	ecx,[ebx-12]
	shl	eax,1
	mov	[ebx+4*ecx],eax
	inc	ecx
	mov	[ebx-12],ecx
	jnc	@@5
;p¯eteËenÌ na konci s2
	push	ecx
	dec	ecx
	dec	ecx
	mov	eax,1
	push	edi
	push	esi
	push	ebx
	mov	edi,ebx
	call	incri
	pop	ebx
	pop	esi
	pop	edi
	pop	[dword ebx-12]
;zvyö exponent zbytku
@@5:	inc	[dword esi-4]
;zapamatuj si nejvyööÌ ¯·d dÏlitele
	mov	eax,[ebx]
	mov	[d1],eax
	xor	eax,eax
	cmp	[dword ebx-12],1
	jz	@@7
	mov	eax,[ebx+4]
@@7:	mov	[d2],eax
;naËti nejvyööÌ ¯·d zbytku
	xor	edx,edx
	mov	eax,[esi]
	mov	ecx,[ebx-4]
	cmp	[esi-4],ecx
	jz	@@4
	mov	[dword edi],0	;zbytek < "dÏlitel"
	jl	@@o
	mov	edx,eax
	xor	eax,eax
	cmp	[dword esi-12],1
	jz	@@4
	mov	eax,[esi+4]
;ochrana proti p¯eteËenÌ
@@4:	cmp	edx,[d1]
	jb	@@9
	mov	ecx,eax
	stc
	sbb	eax,eax	;0xFFFFFFFF
	add	ecx,[d1]
	jc	@@8
	jmp	@@10
;odhadni v˝slednou ËÌslici
@@9:	div	[d1]
	mov	ecx,edx	;zbytek dÏlenÌ
@@10:	push	eax	;uchovej v˝slednou cifru
	mul	[d2]
	sub	edx,ecx
	jc	@@11
	mov	ecx,[esi-4]
	inc	ecx
	sub	ecx,[ebx-4]
	cmp	[dword esi-12],ecx
	jbe	@@k	;jednocifern˝ nebo dvoucifern˝ zbytek
	sub	eax,[esi+4*ecx]
	sbb	edx,0
	jc	@@11
;dokud je zbytek z·porn˝, sniûuj v˝slednou cifru
@@k:	mov	ecx,eax
	or	ecx,edx
	jz	@@11
	dec	[dword esp]
	sub	eax,[d2]
	sbb	edx,[d1]
	jnc	@@k
@@11:	pop	eax
;vyn·sob dÏlitel a odeËti od zbytku
@@8:	mov	[edi],eax
	push	eax
	push	ebx
	push	[s4]
	call	MULTI
	push	esi
	push	[s4]
	push	esi
	push	[s3]
	call	MINUSX
	mov	esi,[s3]
	pop	[s3]
	cmp	[byte esi-8],0
	jz	@@o	;zbytek je kladn˝
	cmp	[dword esi-12],0
	jz	@@o
;oprav v˝sledek, aby zbytek nebyl z·porn˝
@@d:	dec	[dword edi]
	push	esi
	push	ebx
	push	esi
	push	[s3]
	call	PLUSX
	mov	esi,[s3]
	pop	[s3]
	cmp	[dword esi-12],0
	jz	@@o
	cmp	[byte esi-8],0
	jnz	@@d
@@o:	inc	[dword esi-4]
	inc	[dword ebx-4]
	jmp	@@lp
@@e:	mov	edi,[a0]
	sub	[edi-12],ecx
	jmp	@@f
@@z:	test	eax,eax
	jns	@@f
	mov	edi,[a0]
	push	esi
	push	ebx
	call	roundi
	pop	ebx
	pop	esi
;smaû pomocnÈ promÏnnÈ
@@f:	mov	eax,esi
	call	@freex
	mov	eax,ebx
	call	@freex
	mov	eax,[s3]
	call	@freex
	mov	eax,[s4]
	call	@freex
	add	esp,8+HED
	mov	edi,[a0]
	cmp	[ro],0
	jz	@@t
	push	[ro]
	push	edi
	push	edi
	call	DIVI
@@t:	call	trim
@@ret:	ret
endp	SQRTX2
;-------------------------------------
proc	_overflow
	lea	eax,[E_1011]
	push	eax
	push	1011
	call	_cerror
	add	esp,8
	ret
endp	_overflow
;-------------------------------------
;calling convention _stdcall
MULTX:  	jmp	_MULTX@12
DIVX:   	jmp	_DIVX@12
SQRTX:  	jmp	_SQRTX@8
_ALLOCN:	jmp	ALLOCNX
_COPYX@8:	jmp	COPYX
_WRITEX1@8:	jmp	WRITEX1
_READX1@8:	jmp	READX1
_MULTX1@12:	jmp	MULTX1
_MULTI@12:	jmp	MULTI
_MULTIN@12:	jmp	MULTIN
_MULTI1@8:	jmp	MULTI1
_DIVX2@12:	jmp	DIVX2
_DIVI@12:	jmp	DIVI
_MODI@8:	jmp	MODI
_PLUSX@12:	jmp	PLUSX
_MINUSX@12:	jmp	MINUSX
_PLUSU@12:	jmp	PLUSU
_MINUSU@12:	jmp	MINUSU
_CMPX@8:	jmp	CMPX
_CMPU@8:	jmp	CMPU
_FACTORIALI@8:	jmp	FACTORIALI
_FFACTI@8:	jmp	FFACTI
_SQRTX2@8:	jmp	SQRTX2
_SQRTI@8:	jmp	SQRTI
_ANDU@12:	jmp	ANDU
_ORU@12:	jmp	ORU
_XORU@12:	jmp	XORU

;calling convention _fastcall (parameters in registers ECX,EDX)
@ALLOCX@4:	mov	eax,ecx
		jmp	@allocx
@FREEX@4:	mov	eax,ecx
		jmp	@freex
@NEWCOPYX@4:	mov	eax,ecx
		jmp	@newcopyx
@SETX@8:	mov	eax,ecx
		jmp	@setx
@SETXN@8:	mov	eax,ecx
		jmp	@setxn
@ZEROX@4:	mov	eax,ecx
		jmp	@zerox
@ONEX@4:	mov	eax,ecx
		jmp	@onex
@FRACTOX@4:	mov	eax,ecx
		jmp	@fractox
@NORMX@4:	mov	eax,ecx
		jmp	@normx
@NEGX@4:	mov	eax,ecx
		jmp	@negx
@ABSX@4:	mov	eax,ecx
		jmp	@absx
@SIGNX@4:	mov	eax,ecx
		jmp	@signx
@TRUNCX@4:	mov	eax,ecx
		jmp	@truncx
@INTX@4:	mov	eax,ecx
		jmp	@intx
@CEILX@4:	mov	eax,ecx
		jmp	@ceilx
@ROUNDX@4:	mov	eax,ecx
		jmp	@roundx
@FRACX@4:	mov	eax,ecx
		jmp	@fracx
@SCALEX@8:	mov	eax,ecx
		jmp	@scalex
@ADDII@8:	mov	eax,ecx
		jmp	@addii

;-------------------------------------
;Borland C++ Builder
public	@allocx,@freex,@newcopyx,COPYX,WRITEX1,READX1
public	@setx,@setxn,@zerox,@onex,@fractox,@normx
public	@negx,@absx,@signx,@truncx,@intx,@ceilx,@roundx,@fracx,@scalex,@addii
public	MULTX,MULTX1,MULTI,MULTIN,MULTI1,DIVX2,DIVI,MODI
public	PLUSX,MINUSX,PLUSU,MINUSU,ANDU,ORU,XORU
public	CMPX,CMPU,FACTORIALI,FFACTI,SQRTX2,SQRTI,ALLOCNX

;Microsoft Visual C++
public	@ALLOCX@4,@FREEX@4,@NEWCOPYX@4,_COPYX@8,_WRITEX1@8,_READX1@8
public	@SETX@8,@SETXN@8,@ZEROX@4,@ONEX@4,@FRACTOX@4,@NORMX@4
public	@NEGX@4,@ABSX@4,@SIGNX@4,@TRUNCX@4,@INTX@4,@CEILX@4,@ROUNDX@4,@FRACX@4,@SCALEX@8,@ADDII@8
public	_MULTX1@12,_MULTI@12,_MULTIN@12,_MULTI1@8,_DIVX2@12,_DIVI@12,_MODI@8
public	_PLUSX@12,_MINUSX@12,_PLUSU@12,_MINUSU@12,_ANDU@12,_ORU@12,_XORU@12
public	_CMPX@8,_CMPU@8,_FACTORIALI@8,_FFACTI@8,_SQRTX2@8,_SQRTI@8,_ALLOCN

public	_overflow,_digitTab
	end
