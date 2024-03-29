; (C) Petr Lastovicka
 
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License.

;compilation:  ML.exe -Cp aritm-masm.asm

;calling convention is stdcall or fastcall
;  called functions remove arguments from the stack
;  arguments are passed from right to left
;the caller must allocate memory for a result

;-16 maxlen
;-12 len
;-8  sign
;-4  exponent

.386
.model flat,stdcall

extrn	Alloc:proc, Free:proc, cerror:proc, MULTX@12:proc, DIVX@12:proc, SQRTX@8:proc
extrn	base:dword, baseIn:dword, error:dword, dwordDigits:dword
public	digitTab
public	c overflow

RES	equ	52
HED	equ	16
;-------------------------------------
.data
E_1008	db	'Negative operand of sqrt',0
E_1010	db	'Division by zero',0
E_1011	db	'Overflow, number is too big',0
digitTab	db	'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ',0
dwordDigitsI db	0,0, 32, 20, 16, 13, 12, 11, 10, 10, 9, 9, 8, 8, 8, 8, 8
	db	7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
dwordMax	dd	0,0, 0, 3486784401, 0, 1220703125, 2176782336, 1977326743
	dd	1073741824, 3486784401, 1000000000, 2357947691, 429981696
	dd	815730721, 1475789056, 2562890625, 0
	dd	410338673, 612220032, 893871739, 1280000000, 1801088541
	dd	2494357888, 3404825447, 191102976, 244140625, 308915776
	dd	387420489, 481890304, 594823321, 729000000, 887503681
	dd	1073741824, 1291467969, 1544804416, 1838265625, 2176782336

.code
;-------------------------------------
;[edi]:=[esi]
;nem�n� esi,edi,ebx
copyx	proc	uses esi edi
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
	add	ecx,2	;okop�ruj tak� znam�nko a exponent
	mov	eax,8
	sub	edi,eax
	sub	esi,eax
	rep movsd
	popf
	jbe	@@ret
;zaokrouhlen�
	mov	eax,[esi]
	test	eax,eax
	jns	@@ret
	mov	edi,edx
	push	ebx
	call	incre
	pop	ebx
@@ret:	ret
copyx	endp

COPYX	proc 	uses esi edi a0,a1

	mov	edi,[a0]
	mov	esi,[a1]
	call	copyx
	ret
COPYX	endp
;-------------------------------------
;alokuje ��slo s mantisou d�lky eax
@ALLOCX@4:	mov	eax,ecx
@allocx	proc
	push	eax
	lea	eax,[eax*4+HED+RES]	;v�etn� hlavi�ky a rezervovan�ho m�sta
	push	eax
	call	Alloc
	pop	edx
	pop	ecx
	test	eax,eax
	jz	@@ret
	add	eax,HED
	mov	[eax-16],ecx
	mov	dword ptr [eax-12],0	;inicializuj na nulu
	mov	dword ptr [eax-4],1
	mov	dword ptr [eax-8],0
@@ret:	ret
@allocx	endp

_ALLOCN:
ALLOCN	proc
	mov	eax,[esp+8]
	lea	eax,[eax*4+HED+RES]	;v�etn� hlavi�ky a rezervovan�ho m�sta
	push	eax
	mul	dword ptr [esp+8]
	push	eax
	call	Alloc
	pop	edx
	pop	ecx
	test	eax,eax
	jz	@@ret
	lea	eax,[eax+edx+HED]
@@lp:	
	sub	eax,ecx
	mov	edx,[esp+8]
	mov	[eax-16],edx
	mov	dword ptr [eax-12],0	;inicializuj na nulu
	mov	dword ptr [eax-4],1
	mov	dword ptr [eax-8],0
	mov	edx,[esp+4]
	mov	edx,[esp+4*edx+8]
	mov	[edx],eax
	dec	dword ptr [esp+4]
	jnz	@@lp
@@ret:	ret
ALLOCN	endp
;-------------------------------------
;vytvo�� kopii operandu
@NEWCOPYX@4:	mov	eax,ecx
@newcopyx	proc	uses esi edi
	mov	ecx,[eax-16]
	push	eax
	lea	eax,[ecx*4+HED+RES]
	push	eax
	call	Alloc
	pop	edx
	pop	esi
	test	eax,eax
	jz	@@ret
;okop�ruj v�etn� hlavi�ky
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
@newcopyx	endp
;-------------------------------------
;uvoln�n� pam�ti
@FREEX@4:	mov	eax,ecx
@freex	proc
	test	eax,eax
	jz	@@ret
	sub	eax,HED
	push	eax
	call	Free
	pop	eax
@@ret:	ret
@freex	endp
;-------------------------------------
@SETX@8:	mov	eax,ecx
@setx	proc
	test	edx,edx
	jz	@zerox
	mov	dword ptr [eax-12],-2
	mov	dword ptr [eax-8],0
	mov	dword ptr [eax-4],1
	mov	[eax],edx
	mov	dword ptr [eax+4],1
	ret
@setx	endp

@SETXN@8:	mov	eax,ecx
@setxn	proc
	test	edx,edx
	jns	@setx
	neg	edx
	mov	ecx,1
	mov	dword ptr [eax-12],-2
	mov	dword ptr [eax-8],ecx
	mov	dword ptr [eax-4],ecx
	mov	[eax],edx
	mov	[eax+4],ecx
	ret
@setxn	endp

@ZEROX@4:	mov	eax,ecx
@zerox	proc
	and	dword ptr [eax-12],0
	ret
@zerox	endp

@ONEX@4:	mov	eax,ecx
@onex	proc
	mov	edx,1
	mov	dword ptr [eax-12],-2
	and	dword ptr [eax-8],0
	mov	[eax-4],edx
	mov	[eax],edx
	mov	[eax+4],edx
	ret
@onex	endp
;-------------------------------------
;[esi]-=eax;  nesm� doj�t k podte�en�
decra:	sub	[esi],eax
	jnc	trim
@@:	sub	esi,4
decr:	sub	dword ptr [esi],1
	jc	@b
;u��znut� koncov�ch nul [edi]; nem�n� edi,ebx
trim:	mov	ecx,[edi-12]
	lea	esi,[edi+4*ecx-4]
trims:	mov	eax,[esi]
	test	eax,eax
	jnz	@ret
	sub	esi,4
	dec	dword ptr [edi-12]
	jnz	trims
	ret
;dekrementace [edi];  ��rka mus� b�t uvnit� mantisy; nesm� doj�t k podte�en�
decre:	mov	ecx,[edi-12]
	lea	esi,[edi+4*ecx-4]
	jmp	decr
;-------------------------------------
;[edi+ecx]+=eax
incri	proc
	mov	edx,[edi-12]
	mov	ebx,[edi-16]
	test	ecx,ecx
	js	@@sh		;p�ed ��slem
	lea	esi,[edi+4*ecx]
	cmp	ecx,edx
	jc	incra		;uvnit� ��sla
	cmp	ecx,ebx
	ja	@@ret		;za ��slem d�l ne� o 1
	mov	[edi-12],ecx
	jnz	@@2
;t�sn� za ��slem - rozhodne se o zaokrouhlen�
	test	eax,eax
	jns	@@ret
@@2:	setc	bl
;increment le�� v alokovan�m m�st� za ��slem
	push	edi
	mov	[esi],eax
;vynuluj oblast mezi ��slem a inkrementem
	xor	eax,eax
	lea	edi,[esi-4]
	sub	ecx,edx
	std
	rep stosd
	cld
	pop	edi
	test	bl,bl
	jz	round2
	inc	dword ptr [edi-12]
@@ret:	ret
;inkrement je p�ed ��slem - ��slo se posune vpravo
@@sh:	neg	ecx
	add	[edi-4],ecx	;uprav exponent
	cmp	ecx,ebx
	ja	@@d		;��slo je moc mal� -> zru��m ho
	sub	ebx,ecx
	cmp	ebx,edx
	push	edi
	push	ecx
	jae	@@1
;ztr�ta p�esnosti
	mov	edx,[edi-16]
	inc	edx
	mov	[edi-12],edx
	jmp	@@cp
;zv�t�en� d�lky ��sla
@@1:	add	[edi-12],ecx
	mov	ebx,edx
;posun bloku d�lky ebx sm�rem vpravo o ecx integer�
@@cp:	lea	esi,[edi+4*ebx-4]
	lea	edi,[esi+4*ecx]
	mov	ecx,ebx
	std
	rep movsd
	cld
	pop	ecx
	pop	edi
;zapi� inkrement na prvn� pozici
	mov	[edi],eax
;vynuluj m�sto mezi
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
	dec	dword ptr [edi-12]
	jmp	roundi
@@d:	mov	[edi],eax
	mov	dword ptr [edi-12],1
	ret
incri	endp
;-------------------------------------
;[edi+ecx]-=eax
;nesm� doj�t k podte�en� !
decri	proc
	test	eax,eax
	jz	@@ret
	mov	edx,[edi-12]
	mov	ebx,[edi-16]
	lea	esi,[edi+4*ecx]
	cmp	ecx,edx
	jc	decra		;uvnit� ��sla
	cmp	ecx,ebx
	ja	@@ret		;za ��slem d�l ne� o 1
        jnz	@@3
;t�sn� za ��slem - rozhodne se o zaokrouhlen�
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
;decrement le�� v alokovan�m m�st� za ��slem
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
decri	endp
;-------------------------------------
;p�i�ten� jedni�ky na konec mantisy [edi]
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
@incr:	inc	dword ptr [esi]
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
;zv�t�i d�lku
	inc	dword ptr [edi-12]
	xor	ebx,ebx
;zv�t�i exponent
@1:	inc	dword ptr [edi-4]
	jo	overflow
;posun cel� mantisy o jeden ��d vpravo
@2:	push	esi
	lea	edi,[esi+4]
	std
	rep movsd
	cld
;p�id�n� p�enosu
	mov	[edi],eax
	test	ebx,ebx
	pop	esi
	js	@incr
	jmp	trim
@ret:	ret

;-------------------------------------
;zmen�� d�lku [edi] na maximum a zaokrouhl� podle cifry za mantisou
round:	mov	ecx,[edi-16]
	cmp	ecx,[edi-12]
	jb	round1
	ret

;zv�t�� d�lku [edi] o jedna nebo zaokrouhl�
roundi:	mov	ecx,[edi-16]
	cmp	ecx,[edi-12]
	ja	inclen
round1:	mov	[edi-12],ecx
;pod�vej se do rezerovan�ho m�sta za mantisou
	lea	esi,[edi+4*ecx]
round2:	mov	eax,[esi]
	sub	esi,4
	test	eax,eax
	jns	trims
;zaokrouhlen� nahoru
	jmp	incr
;pokud je je�t� voln� m�sto, pak jen zv�t�i d�lku ��sla
inclen:	inc	dword ptr [edi-12]
	jmp	trim
;-------------------------------------
;normalizace [edi] - mantisa nebude za��nat ani kon�it na nulu
;zm�n� esi,edi
norm	proc
	cmp	dword ptr [edi-12],0
	jbe	@@ret    ;zlomek nebo nula
	call	trim
	mov	ecx,[edi-12]
	test	ecx,ecx
	jz	@@ret   ;trim v�sledek vynulovalo
	xor	edx,edx
	cmp	[edi],edx
	jnz	@@ret
	mov	eax,edi
@@lp:	add	eax,4
	dec	dword ptr [edi-4]
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
@@nul:	mov	dword ptr [edi-12],0
	ret
norm	endp

@NORMX@4:	mov	eax,ecx
@normx	proc	uses esi edi
	mov	edi,eax
	call	norm
	ret
@normx	endp
;-------------------------------------
;eax:=ecx:=gcd(eax,ecx), edx:=0
gcd	proc
	jmp	gcd1
gcd0:	div	ecx
	mov	eax,ecx
	mov	ecx,edx
gcd1:	xor	edx,edx
	test	ecx,ecx
	jnz	gcd0
	mov	ecx,eax
	ret
gcd	endp

;zkr�t� zlomek [esi],[edi]
reduce	proc
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
reduce	endp

fracexp	proc
	mov	ecx,[edi]
	cmp	ecx,[edi+4]
	setae	al
	mov	[edi-4],al
	mov	dword ptr [edi-12],-2
	test	ecx,ecx
	jnz	@@ret
	and	dword ptr [edi-12],0
@@ret:	ret
fracexp	endp

fracReduce	proc
	push	esi
	lea	esi,[edi+4]
	call	reduce
	pop	esi
	call	fracexp
	ret
fracReduce	endp


;-------------------------------------
;a0:=a1/ai
;a0 m��e b�t rovno a1
DIVI	proc 	uses esi edi ebx a0,a1,ai
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
	call	cerror
	add	esp,8
	ret
@@z:	cmp	dword ptr [esi-12],-2
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
;okop�ruj znam�nko a exponent
@@0:	mov	eax,[esi-4]
	mov	[edi-4],eax
	mov	edx,[esi-8]
	mov	[edi-8],edx
;nastav d�lku v�sledku jako minimum z d�lek operand�
	mov	ecx,[esi-12]
	mov	eax,[edi-16]
	cmp	ecx,eax
	jbe	@@1
	mov	ecx,eax
@@1:	mov	[edi-12],ecx
	test	ecx,ecx
	jz	@@ret     ; 0/ai=0
;na�ti prvn� cifru d�lence
	mov	edx,[esi]
	cmp	edx,ebx
	jnc	@@2
;prvn� d�len� bude 64-bitov� a v�sledek bude krat��
	add	esi,4
	dec	dword ptr [edi-4]
	jno	@@6
;podte�en� -> v�sledek nulov�
	mov	dword ptr [edi-12],0
	ret
@@6:	dec	dword ptr [edi-12]
	dec	ecx
	jnz	@@4
;d�l�nec m� d�lku 1 a je men�� ne� d�litel
        mov	esi,edi
	jmp	@@3
@@2:	xor	edx,edx
;ecx=d�lka v�sledku, ebx=d�litel, edx=horn� �ty�byte d�l�nce
@@4:	push	edi
;hlavn� cyklus
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
	jz	@@ret   ;nulov� zbytek
	mov	ecx,[edi-12]
	jmp	@@3
;za vstupn� operand dopl�uj nuly a pokra�uj v d�len�
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
DIVI	endp
;-------------------------------------
;konverze zlomku na desetinn� ��slo
@FRACTOX@4:	mov	eax,ecx
@fractox:	cmp	dword ptr [eax-12],-2
	jnz	@f
fractox:
	mov	dword ptr [eax-12],1
	mov	dword ptr [eax-4],1
	push	dword ptr [eax+4]
	push	eax
	push	eax
	call	DIVI
@@:	ret

;len=[edi-16], a/b=[esi]
;return eax
fractonewx2:
	cmp	dword ptr [esi-12],-2
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
;-------------------------------------
;cmp [edx],[ecx]
cmpu	proc	uses esi edi
;test na nulovost operand�
	cmp	dword ptr [edx-12],0
	jnz	@@1
	cmp	dword ptr [ecx-12],0
	jz	@@ret		;oba jsou 0
	jmp	@@gr2		;prvn� je 0
@@1:	cmp	dword ptr [ecx-12],1
	jc	@@gr1		;druh� je 0
;zlomky
	cmp	dword ptr [edx-12],-2
	jnz	@@f1
	cmp	dword ptr [ecx-12],-2
	jnz	@@f2
;a/b ? c/d
	mov	edi,edx
	mov	eax,[edx]
	mul	dword ptr [ecx+4]
	push	edx
	push	eax
	mov	eax,[ecx]
	mul	dword ptr [edi+4]
	pop	ecx
	pop	edi
;edi:ecx ? edx:eax
	cmp	edi,edx
	jnz	@@ret
	cmp	ecx,eax
	jmp	@@ret
@@f1:	cmp	dword ptr [ecx-12],-2
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
;rychl� porovn�n� nejvy���ho ��du
	mov	eax,[edx]
	cmp	eax,[ecx]
	jnz	@@ret
;o��znut� koncov�ch nul
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
;prvn� operand je krat�� nebo jsou si rovny
@@2:	cmp	ecx,edi	;ZF=1 nebo CF=1
	ret
;druh� operand je krat��
@@3:	cmp	esi,edx	;nastav ZF=0, CF=0
	ret
@@gr2:	stc
	ret
@@gr1:	clc
@@ret:	ret
cmpu	endp

CMPU	proc 	a1,a2
	mov	edx,[a1]
	mov	ecx,[a2]
	call	cmpu
	jb	@@gr2
	ja	@@gr1
	xor	eax,eax
	ret
@@gr1:	mov	eax,1
	ret
@@gr2:	mov	eax,-1
	ret
CMPU	endp
;-------------------------------------
;porovn�n�, v�sledek -1,0,+1
CMPX	proc 	a1,a2
	mov	edx,[a1]
	mov	ecx,[a2]
	cmp	dword ptr [edx-12],0
	jnz	@@1
	cmp	dword ptr [ecx-12],0
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
CMPX	endp
;-------------------------------------
plusfrac	proc
	mov	eax,[esi+4]
	mov	ecx,[ebx+4]
	call	gcd
;lcm(b,d)
	mov	eax,[esi+4]
	mul	dword ptr [ebx+4]
	cmp	edx,ecx
	jae	@@fe
	div	ecx
	mov	[edi+4],eax
;a*lcm(b,d)/b
	mov	eax,[ebx]
	mul	dword ptr [edi+4]
	mov	ecx,[ebx+4]
	cmp	edx,ecx
	jae	@@fe
	div	ecx
	mov	[edi],eax
;c*lcm(b,d)/d
	mov	eax,[esi]
	mul	dword ptr [edi+4]
	mov	ecx,[esi+4]
	cmp	edx,ecx
	jae	@@fe
	div	ecx
	stc
@@fe:	ret
plusfrac	endp

;bezznam�nkov� plus
PLUSU	proc 	uses esi edi ebx a0,a1,a2
local	z0,z1,z2,e0,e1,e2
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
;proho� operandy, aby byl prvn� v�t��
	mov	eax,[ebx-4]
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
	cmp	dword ptr [ebx-12],0
	jnz	@@0
;okop�ruj 2.operand
	push	dword ptr [edi-8]
	call	copyx
	pop	dword ptr [edi-8]
	ret
;test na nulovost 2.operandu
@@0:	cmp	dword ptr [ecx-12],0
	jz	@@cp1a
	cmp	dword ptr [ecx-12],-2
	jnz	@@f1
	cmp	dword ptr [ebx-12],-2
	jnz	@@fe
;(a/b)+(c/d)
	call	plusfrac
	jnc	@@fe
	add	[edi],eax
	jc	@@fe
	call	fracReduce
	ret
@@f1:	cmp	dword ptr [ebx-12],-2
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
;zv�t�i p�esnost o jedna
@@f2:	inc	dword ptr [edx-16]
;nastav prom�nn� ur�uj�c� exponent na konci operand�
	mov	eax,[ebx-4]
	sub	eax,[ebx-12]
	mov	[z1],eax
	mov	eax,[ecx-4]
	sub	eax,[ecx-12]
	mov	[z2],eax
	mov	eax,[ebx-4]
	mov	esi,[edx-16]
	mov	[edx-4],eax	;exponent v�sledku
	mov	[edx-12],esi	;d�lka v�sledku
	sub	eax,esi		;[z0]
	cmp	eax,[ecx-4]
	jl	@@7
;2.operand je moc mal�
@@cp1:	dec	dword ptr [edx-16]
@@cp1a:	mov	esi,ebx
	mov	edi,edx
	push	dword ptr [edi-8]
	call	copyx
	pop	dword ptr [edi-8]
	ret
@@7:	mov	[z0],eax
;spo�ti adresy konc� operand�
	mov	eax,[ebx-12]
	lea	eax,[4*eax+ebx-4]
	mov	[e1],eax
	mov	eax,[ecx-12]
	lea	esi,[4*eax+ecx-4]
	mov	[e2],esi
	mov	eax,[edx-12]
	lea	edi,[4*eax+edx-4]
	mov	[e0],edi
;o��zni operandy, kter� jsou moc dlouh�
	mov	eax,[z0]
	sub	eax,[z1]
	jle	@@2
	add	[z1],eax
	shl	eax,2
	sub	[e1],eax
@@2:	mov	edi,[z0]
	sub	edi,[z2]
	jle	@@3
	add	[z2],edi
	shl	edi,2
	sub	[e2],edi
;zmen�i d�lku v�sledku, pokud jsou operandy moc kr�tk�
@@3:	cmp	eax,edi
	jge	@@4
	mov	eax,edi
@@4:	test	eax,eax
	jns	@@5
	add	[edx-12],eax
	sub	[z0],eax
	shl	eax,2
	add	[e0],eax
;zjisti, kter� operand je del��
@@5:	pushf
	std
	mov	edx,ecx
	mov	edi,[e0]
	mov	ecx,[z1]
	sub	ecx,[z2]
	jz	@@plus
	jg	@@6
;okop�ruj konec 1.operandu
	neg	ecx
	mov	esi,[e1]
	rep movsd
	mov	[e1],esi
	jmp	@@plus
@@6:	mov	eax,[z1]
	sub	eax,[edx-4]
	mov	esi,[e2]
	jle	@@cpe
;operandy se nep�ekr�vaj�
	sub	ecx,eax
	rep movsd
;voln� m�sto mezi operandy vypl� nulami
	mov	ecx,eax
	xor	eax,eax
	rep stosd
	mov	esi,[e1]
	jmp	@@cpb
;okop�ruj konec 2.operandu
@@cpe:	rep movsd
	mov	[e2],esi
;se�ti p�ekr�vaj�c� se ��sti operand�
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
;okop�ruj za��tek 1.operandu z esi na edi
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
;edi=[a0], esi=pozice p�enosu
;zpracuj p�ete�en�
	test	bl,bl
	jz	@@end
	call	incr
@@end:	popf
	dec	dword ptr [edi-16]
	call	round
	ret
PLUSU	endp
;-------------------------------------
negf	proc
;edi,esi ukazuj� na posledn� dword
;ecx je d�lka
;vrac� CF
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
negf	endp
;-------------------------------------
;bezznam�nkov� minus
MINUSU	proc 	uses esi edi ebx a0,a1,a2
local	z0,z1,z2,e0,e1,e2
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	edi,edx
	mov	esi,ecx
;test na nulovost 2.operandu
@@0:	cmp	dword ptr [ecx-12],0
	jz	@@cp1a
;test na zlomek
	cmp	dword ptr [ecx-12],-2
	jnz	@@f1
	cmp	dword ptr [ebx-12],-2
	jnz	@@fe
;(a/b)-(c/d)
	call	plusfrac
	jnc	@@fe
	sub	[edi],eax
	call	fracReduce
	ret
@@f1:	cmp	dword ptr [ebx-12],-2
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
;zv�t�i p�esnost o jedna
@@f2:	inc	dword ptr [edx-16]
;nastav prom�nn� ur�uj�c� exponent na konci operand�
	mov	eax,[ebx-4]
	sub	eax,[ebx-12]
	mov	[z1],eax
	mov	eax,[ecx-4]
	sub	eax,[ecx-12]
	mov	[z2],eax
	mov	eax,[ebx-4]
	mov	esi,[edx-16]
	mov	[edx-4],eax	;exponent v�sledku
	mov	[edx-12],esi	;d�lka v�sledku
	sub	eax,esi		;[z0]
	cmp	eax,[ecx-4]
	jl	@@7
;2.operand je moc mal�
@@cp1:	dec	dword ptr [edx-16]
@@cp1a:	mov	esi,ebx
	mov	edi,edx
	push	dword ptr [edi-8]
	call	copyx
	pop	dword ptr [edi-8]
	ret
@@7:	mov	[z0],eax
;spo�ti adresy konc� operand�
	mov	eax,[ebx-12]
	lea	eax,[4*eax+ebx-4]
	mov	[e1],eax
	mov	eax,[ecx-12]
	lea	esi,[4*eax+ecx-4]
	mov	[e2],esi
	mov	eax,[edx-12]
	lea	edi,[4*eax+edx-4]
	mov	[e0],edi
;o��zni operandy, kter� jsou moc dlouh�
	mov	eax,[z0]
	sub	eax,[z1]
	jle	@@2
	add	[z1],eax
	shl	eax,2
	sub	[e1],eax
@@2:	mov	edi,[z0]
	sub	edi,[z2]
	jle	@@3
	add	[z2],edi
	shl	edi,2
	sub	[e2],edi
;zmen�i d�lku v�sledku, pokud jsou operandy moc kr�tk�
@@3:	cmp	eax,edi
	jge	@@4
	mov	eax,edi
@@4:	test	eax,eax
	jns	@@5
	add	[edx-12],eax
	sub	[z0],eax
	shl	eax,2
	add	[e0],eax
;zjisti, kter� operand je del��
@@5:	pushf
	std
	mov	edx,ecx
	mov	edi,[e0]
	mov	ecx,[z1]
	sub	ecx,[z2]
	jz	@@minus
	jg	@@6
;okop�ruj konec 1.operandu
	neg	ecx
	mov	esi,[e1]
	rep movsd
	mov	[e1],esi
	clc
	jmp	@@minus
@@6:	mov	eax,[z1]
	sub	eax,[edx-4]
	mov	esi,[e2]
	jle	@@nege
;operandy se nep�ekr�vaj�
	sub	ecx,eax
	push	eax
	call	negf
	pop	ecx
;voln� m�sto mezi operandy vypl� nulami nebo jedni�kami
	sbb	eax,eax
	rep stosd	;nem�n� CF
	mov	esi,[e1]
	jmp	@@cpb
;neguj konec 2.operandu
@@nege:	call	negf
	mov	[e2],esi
;ode�ti p�ekr�vaj�c� se ��sti operand�
;edi, e1,e2, cf
@@minus:	lahf
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
;okop�ruj za��tek 1.operandu z esi na edi
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
;zpracuj p�ete�en�
	test	bl,bl
	jz	@@end
	call	decr
@@end:	call	norm
	popf
	mov	edi,[a0]
	dec	dword ptr [edi-16]
	call	round
	ret
MINUSU	endp
;-------------------------------------
PLUSX	proc 	uses ebx a0,a1,a2
	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	edx,[a0]
	mov	eax,[ebx-8]
	mov	[edx-8],eax
	cmp	eax,[ecx-8]
	jz	@@plusu
	mov	edx,ebx
	call	cmpu
	jnc	@@minusu
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	eax,[ecx-8]
	mov	[a1],ecx
	mov	[a2],ebx
	mov	[edx-8],eax
@@minusu:	invoke	MINUSU,a0,a1,a2
	ret
@@plusu:	invoke	PLUSU,a0,a1,a2
	ret
PLUSX	endp
;-------------------------------------
MINUSX	proc 	uses ebx a0,a1,a2
	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	edx,[a0]
	mov	eax,[ebx-8]
	mov	[edx-8],eax
	cmp	eax,[ecx-8]
	jnz	@@plusu
;stejn� znam�nka
	mov	edx,ebx
	call	cmpu
	jnc	@@minusu
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	eax,[ecx-8]
	xor	eax,1
	mov	[a1],ecx
	mov	[a2],ebx
	mov	[edx-8],eax
@@minusu:	invoke	MINUSU,a0,a1,a2
	ret
@@plusu:	invoke	PLUSU,a0,a1,a2
	ret
MINUSX	endp
;-------------------------------------
;bezznam�nkov� or nebo xor
ORXORU0	proc 	uses esi edi ebx a0,a1,a2
local	z0,z1,z2,e0,e1,e2
local	isxor:byte	
	mov	[isxor],al
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
;proho� operandy, aby byl prvn� v�t��
	mov	eax,[ebx-4]
	cmp	eax,[ecx-4]
	jge	@@1
	mov	eax,ebx
	mov	ebx,ecx
	mov	ecx,eax
	mov	[a1],ebx
	mov	[a2],ecx
;test na nulovost 1.operandu
@@1:	cmp	dword ptr [ebx-12],0
	jnz	@@0
;okop�ruj 2.operand
	mov	esi,ecx
	mov	edi,edx
	push	dword ptr [edi-8]
	call	copyx
	pop	dword ptr [edi-8]
	ret
;test na nulovost 2.operandu
@@0:	cmp	dword ptr [ecx-12],0
	jz	@@cp1
;nastav prom�nn� ur�uj�c� exponent na konci operand�
	mov	eax,[ebx-4]
	sub	eax,[ebx-12]
	mov	[z1],eax
	mov	eax,[ecx-4]
	sub	eax,[ecx-12]
	mov	[z2],eax
	mov	eax,[ebx-4]
	mov	esi,[edx-16]
	mov	[edx-4],eax	;exponent v�sledku
	mov	[edx-12],esi	;d�lka v�sledku
	sub	eax,esi		;[z0]
	cmp	eax,[ecx-4]
	jl	@@7
;2.operand je moc mal�
@@cp1:	mov	esi,ebx
	mov	edi,edx
	push	dword ptr [edi-8]
	call	copyx
	pop	dword ptr [edi-8]
	ret
@@7:	mov	[z0],eax
;spo�ti adresy konc� operand�
	mov	eax,[ebx-12]
	lea	eax,[4*eax+ebx-4]
	mov	[e1],eax
	mov	eax,[ecx-12]
	lea	esi,[4*eax+ecx-4]
	mov	[e2],esi
	mov	eax,[edx-12]
	lea	edi,[4*eax+edx-4]
	mov	[e0],edi
;o��zni operandy, kter� jsou moc dlouh�
	mov	eax,[z0]
	sub	eax,[z1]
	jle	@@2
	add	[z1],eax
	shl	eax,2
	sub	[e1],eax
@@2:	mov	edi,[z0]
	sub	edi,[z2]
	jle	@@3
	add	[z2],edi
	shl	edi,2
	sub	[e2],edi
;zmen�i d�lku v�sledku, pokud jsou operandy moc kr�tk�
@@3:	cmp	eax,edi
	jge	@@4
	mov	eax,edi
@@4:	test	eax,eax
	jns	@@5
	add	[edx-12],eax
	sub	[z0],eax
	shl	eax,2
	add	[e0],eax
;zjisti, kter� operand je del��
@@5:	pushf
	std
	mov	edx,ecx
	mov	edi,[e0]
	mov	ecx,[z1]
	sub	ecx,[z2]
	jz	@@plus
	jg	@@6
;okop�ruj konec 1.operandu
	neg	ecx
	mov	esi,[e1]
	rep movsd
	mov	[e1],esi
	jmp	@@plus
@@6:	mov	eax,[z1]
	sub	eax,[edx-4]
	mov	esi,[e2]
	jle	@@cpe
;operandy se nep�ekr�vaj�
	sub	ecx,eax
	rep movsd
;voln� m�sto mezi operandy vypl� nulami
	mov	ecx,eax
	xor	eax,eax
	rep stosd
	mov	esi,[e1]
	jmp	@@cpb
;okop�ruj konec 2.operandu
@@cpe:	rep movsd
	mov	[e2],esi
;zpracuj p�ekr�vaj�c� se ��sti operand�
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

	cmp	byte ptr [isxor],0
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
;okop�ruj za��tek 1.operandu z esi na edi
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
ORXORU0	endp
;-------------------------------------
ANDU0	proc 	uses esi edi ebx a0,a1,a2
	mov	edx,[a0]
	mov	ebx,[a1]
	mov	ecx,[a2]
;zjisti rozsah v�sledku
	mov	edi,[ebx-4]
	mov	esi,[ecx-4]
	mov	eax,edi
	cmp	eax,esi
	jl	@@1
	mov	eax,esi
@@1:	mov	[edx-4],eax    ;exponent v�sledku
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
;spo�ti d�lku v�sledku
@@2:	mov	edi,[edx-4]
	sub	edi,esi
	jg	@@3
;operandy se nep�ekr�vaj�
@@zero:	and	dword ptr [edx-12],0
	ret
@@3:	mov	eax,[edx-16]
	cmp	eax,edi
	jae	@@4
;v�sledek je krat�� ne� operandy
	mov	edi,eax
@@4:	mov	[edx-12],edi
;spo�ti adresy za��tk�
	mov	esi,[edx-4]
	mov	eax,[ebx-4]
	sub	eax,esi
	lea	ebx,[ebx+4*eax]
	mov	eax,[ecx-4]
	sub	eax,esi
	lea	ecx,[ecx+4*eax]
	dec	edi
;hlavn� cyklus
@@and:	mov	eax,[ebx+4*edi]
	and	eax,[ecx+4*edi]
	mov	[edx+4*edi],eax
	dec	edi
	jns	@@and
;normalizace
	mov	edi,edx
	call	norm
	ret
ANDU0	endp
;-------------------------------------
;eax=function
defrac	proc 	uses esi edi ebx a0,a1,a2
	mov	edi,[a0]
	mov	ebx,[a1]
	mov	esi,[a2]
	cmp	dword ptr [ebx-12],-2
	jz	@@f
	cmp	dword ptr [esi-12],-2
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
defrac	endp

ORU0:	mov	al,0
	jmp	ORXORU0
XORU0:	mov	al,1
	jmp	ORXORU0
ORU@12:
ORU:	lea	eax,[ORU0]
	jmp	defrac
XORU@12:
XORU:	lea	eax,[XORU0]
	jmp	defrac
ANDU@12:
ANDU:	lea	eax,[ANDU0]
	jmp	defrac
;-------------------------------------
@NEGX@4:	mov	eax,ecx
@negx	proc
	xor	byte ptr [eax-8],1
	ret
@negx	endp

@ABSX@4:	mov	eax,ecx
@absx	proc
	and	byte ptr [eax-8],0
	ret
@absx	endp

@SIGNX@4:	mov	eax,ecx
@signx	proc
	cmp	dword ptr [eax-12],0
	jz	@@ret
	mov	dword ptr [eax-12],-2
	mov	dword ptr [eax-4],1
	mov	dword ptr [eax],1
	mov	dword ptr [eax+4],1
@@ret:	ret
@signx	endp

@SCALEX@8:	mov	eax,ecx
@scalex	proc
	test	edx,edx
	jz	@@ret
	push	edx
	push	eax
	call	@fractox
	pop	eax
	pop	edx
@@1:	add	dword ptr [eax-4],edx
	jo	overflow
@@ret:	ret
@scalex	endp
;-------------------------------------
;round up 0.99999...
;preserve eax,esi,edi,ebx
@fix999	proc
	mov	ecx,[eax-12]
	cmp	ecx,[eax-16]
	jnz	@@ret
	mov	edx,[eax-4]
	dec	ecx
	cmp	edx,ecx
	jge	@@ret
	test	edx,edx
	js	@@ret
	cmp	dword ptr [eax+4*ecx],-100
	jb	@@ret
@@1:	cmp	dword ptr [eax+4*edx],-1
	jnz	@@ret
	inc	edx
	cmp	edx,ecx
	jnz	@@1
	mov	edx,[eax-4]
	mov	[eax-12],edx
	push	esi
	push	edi
	push	ebx
	mov	edi,eax
	call	incre
	mov	eax,edi
	pop	ebx
	pop	edi
	pop	esi
@@ret:	ret
@fix999	endp

;round down 0.0000001
;preserve eax,esi,edi,ebx
@fix001	proc
	mov	ecx,[eax-12]
	cmp	ecx,[eax-16]
	jnz	@@ret
	mov	edx,[eax-4]
	dec	ecx
	cmp	edx,ecx
	jge	@@ret
	test	edx,edx
	js	@@ret
	cmp	dword ptr [eax+4*ecx],100
	ja	@@ret
@@1:	cmp	dword ptr [eax+4*edx],0
	jnz	@@ret
	inc	edx
	cmp	edx,ecx
	jnz	@@1
	mov	edx,[eax-4]
	mov	[eax-12],edx
@@ret:	ret
@fix001	endp

;o��znut� desetinn� ��sti
@TRUNCX@4:	mov	eax,ecx
@truncx	proc
	call	@fix999
	mov	edx,[eax-4]
	test	edx,edx
	js	@zerox
	cmp	edx,[eax-12]
	jae	@@ret
	cmp	dword ptr [eax-12],-2
	jz	truncf
	mov	[eax-12],edx  ;o��znut�
@@ret:	ret
@truncx	endp
;zlomek
truncf:	mov	ecx,eax
	mov	eax,[ecx]
	xor	edx,edx
	div	dword ptr [ecx+4]
	mov	[ecx],eax
	mov	dword ptr [ecx+4],1
	mov	dword ptr [ecx-4],1
	test	eax,eax
	jnz	@f
	mov	dword ptr [ecx-12],eax
@@:	ret

;o��znut� cel� ��sti
@FRACX@4:	mov	eax,ecx
@fracx	proc
	call	@fix001
	call	@fix999
	cmp	dword ptr [eax-12],-2
	jnz	@@1
;zlomek
	mov	ecx,eax
	mov	eax,[ecx]
	xor	edx,edx
	div	dword ptr [ecx+4]
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
@fracx	endp
;-------------------------------------
;zaokrouhlen� na cel� ��slo
@ROUNDX@4:	mov	eax,ecx
@roundx	proc
	mov	edx,[eax-4]
	test	edx,edx
	js	@zerox  ;z�porn� exponent -> v�sledek nula
	mov	ecx,[eax-12]
	cmp	ecx,[eax-16]
	jnz	@@r
	dec	ecx
	cmp	edx,ecx
	jge	@@r
	cmp	dword ptr [eax+4*edx],7fffffffh
	jnz	@@1
	cmp	byte ptr [eax+4*ecx+3],80h
	jb	@@1
	jmp	@@3
@@2:	cmp	dword ptr [eax+4*edx],-1
	jnz	@@4
@@3:	inc	edx
	cmp	edx,ecx
	jnz	@@2
;round up 0.499999...
	mov	edx,[eax-4]
	mov	byte ptr [eax+4*edx+3],80h
	jmp	@@1
@@4:	mov	edx,[eax-4]
@@r:	cmp	edx,[eax-12]
	jl	@@1
;integer or fraction
	cmp	dword ptr [eax-12],-2
	jnz	@@ret
	push	dword ptr [eax+4]
	call	truncf
	pop	eax
	shl	edx,1   ;remainder
	jc	@@i
	cmp	eax,edx
	ja	@@ret
@@i:	inc	dword ptr [ecx]
	cmp	dword ptr [ecx-12],0
	jnz	@@ret
	mov	dword ptr [ecx-12],-2
@@ret:	ret
@@1:	push	esi
	push	edi
	mov	[eax-12],edx   ;zkra� d�lku
	mov	edi,eax
	mov	eax,[edi+4*edx]
	test	eax,eax
	jns	@@e
;zaokrouhli
	push	ebx
	call	incre
	pop	ebx
@@e:	pop	edi
	pop	esi
	ret
@roundx	endp
;-------------------------------------
@minus1	proc
	mov	edx,1
	mov	[eax+4],edx
	mov	[eax],edx
	mov	[eax-4],edx
	mov	[eax-8],edx
	mov	dword ptr [eax-12],-2
	ret
@minus1	endp
;-------------------------------------
;zaokrouhlen� sm�rem dol�
@INTX@4:	mov	eax,ecx
@intx	proc
	cmp	byte ptr [eax-8],0
	jz	@truncx
;operand je z�porn�
	cmp	dword ptr [eax-12],0
	jg	@@1
	jz	@@ret
;fraction
	call	truncf
	test	edx,edx
	jz	@@ret
	inc	dword ptr [ecx]
	cmp	dword ptr [ecx-12],0
	jnz	@@ret
	mov	dword ptr [ecx-12],-2
@@ret:	ret
@@1:	call	@fix001
	mov	edx,[eax-4]
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
	push	ebx
	call	incr
	pop	ebx
@@e:	pop	esi
	pop	edi
	ret
@intx	endp

;zaokrouhlen� sm�rem nahoru
@CEILX@4:	mov	eax,ecx
@ceilx	proc
	xor	byte ptr [eax-8],1
	push	eax
	call	@intx
	pop	eax
	xor	byte ptr [eax-8],1
	ret
@ceilx	endp
;-------------------------------------
;[edi]*= esi
mult1	proc
	cmp	esi,1
	ja	@@z
	jz	@@ret
	and	dword ptr [edi-12],0
	ret
@@z:	cmp	dword ptr [edi-12],-2
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
@@0:	xor	ebx,ebx   ;p�enos
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
;zru� nulov� p�enos
	mov	ecx,[edi-12]
	lea	esi,[edi+4]
	cld
	rep movsd
@@ret:	ret
@@1:	inc	dword ptr [edi-4]
	jno	roundi
	jmp	overflow
mult1	endp

;a0*=ai
MULTI1	proc 	uses esi edi ebx a0,ai
	mov	edi,[a0]
	mov	esi,[ai]
	call	mult1
	ret
MULTI1	endp
;-------------------------------------
;a0:=a1*ai
;a0 m��e b�t rovno a1
MULTI	proc 	uses esi edi ebx a0,a1,ai
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
;okop�ruj znam�nko a exponent
@@0:	mov	eax,[esi-4]
	mov	[edi-4],eax
	mov	edx,[esi-8]
	mov	[edi-8],edx
;nastav d�lku v�sledku
	mov	eax,[edi-16]
	cmp	ecx,eax
	jbe	@@1
	mov	ecx,eax
@@1:	mov	[edi-12],ecx
	add	edi,4
	dec	ecx
	js	@@ret    ;operand je 0
	xor	ebx,ebx  ;p�enos
;hlavn� cyklus
@@lp:	mov	eax,[esi+4*ecx]
	mul	[ai]
	add	eax,ebx
	mov	[edi+4*ecx],eax
	mov	ebx,edx
	adc	ebx,0
	dec	ecx
	jns	@@lp
;zapi� p�ete�en�
	sub	edi,4
	mov	[edi],ebx
	test	ebx,ebx
	jnz	@@2
;zru� nulov� p�enos
	mov	ecx,[edi-12]
	lea	esi,[edi+4]
	cld
	rep movsd
	ret
@@2:	inc	dword ptr [edi-4]
	jo	overflow
	call	roundi
@@ret:	ret
MULTI	endp

MULTIN	proc 	a0,a1,ai
	mov	eax,[ai]
	test	eax,eax
	jns	@f
	neg	eax
	push	eax
	push	[a1]
	push	[a0]
	call	MULTI
	mov	ecx,[a0]
	xor	dword ptr [ecx-8],1
	ret
@@:	push	eax
	push	[a1]
	push	[a0]
	call	MULTI
	ret
MULTIN	endp
;-------------------------------------
DIVX2	proc 	uses esi edi ebx a0,a1,a2
local	d1,d2,d,t1,t2	
	xor	esi,esi
	mov	ebx,[a1]	;d�lenec
	mov	ecx,[a2]	;d�litel
	mov	edx,[a0]	;v�sledek
	cmp	dword ptr [ebx-12],-2
	jnz	@@f1
	cmp	dword ptr [ecx-12],-2
	jnz	@@f0
;(a/b)/(c/d)
	mov	eax,[ecx-8]
	xor	eax,[ebx-8]
	mov	[edx-8],eax
	push	dword ptr [ebx]   ;a
	push	dword ptr [ecx+4] ;d
	push	dword ptr [ebx+4] ;b
	push	dword ptr [ecx]   ;c
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
;zlomek p�etekl
@@fe1:	add	esp,8
@@fe2:	mov	ebx,[a1]
	mov	ecx,[a2]
	mov	edx,edi
	jmp	@@f3
;(a/b)/x
@@f0:	sub	esp,HED+8
;a ulo� na z�sobn�k
	lea	eax,[esp+HED]
	mov	dword ptr [eax-16],1
	mov	dword ptr [eax-12],1
	mov	dword ptr [eax-4],1
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
	push	dword ptr [ebx+4]
	push	[a0]
	push	[a0]
	call	DIVI
	ret
@@f1:	cmp	dword ptr [ecx-12],-2
	jnz	@@f2
;x/(a/b)
@@f3:	push	dword ptr [ecx+4]
	push	dword ptr [ecx]
;p�ekop�ruj d�lenec do v�sledku
	mov	edi,edx
	mov	esi,ebx
	call	copyx
;znam�nko
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
@@f2:	cmp	dword ptr [ecx-12],0
	jnz	@@tz
;division by zero
	lea	eax,[E_1010]
	push	eax
	push	1010
	call	cerror
	add	esp,8
	jmp	@@ret
@@tz:	cmp	[ebx-12],esi
	mov	[edx-12],esi
	jz	@@ret		; d�l�nec je nula
	cmp	dword ptr [ecx-12],1
	ja	@@1
;jednocifern� d�litel
	push	dword ptr [ecx]
	push	ebx
	push	edx
	call	DIVI
	mov	ecx,[a2]
	mov	edx,[a0]
	mov	eax,[ecx-8]	;znam�nko d�litele
	xor	[edx-8],eax
	mov	eax,[ecx-4]	;exponent d�litele
	dec	eax
	sub	[edx-4],eax
	jo	overflow
	ret
;znam�nko
@@1:	mov	eax,[ecx-8]
	push	eax
	xor	eax,[ebx-8]
	mov	[edx-8],eax
	mov	[ecx-8],esi	;kladn� d�litel
;exponent
	mov	edi,[ecx-4]
	mov	eax,[ebx-4]
	push	edi
	inc	eax
	sub	eax,edi
	mov	[ecx-4],esi	;d�litel bez exponentu
	mov	[edx-4],eax
	jno	@@2
	call	overflow
	jmp	@@r
@@2:	mov	eax,[ebx-12]
	mov	edi,[ecx-12]
	cmp	eax,edi
	jbe	@@6
	mov	edi,eax
@@6:	add	edi,2
;alokuj pomocnou prom�nnou t2
	mov	eax,[ecx-12]
	inc	eax
	call	@allocx
	mov	[t2],eax
;alokuj pomocnou prom�nnou t1
	mov	eax,edi
	call	@allocx
	mov	[t1],eax
;alokuj zbytek
	mov	eax,edi
	call	@allocx
;edi:=zbytek, esi:=d�l�nec, ebx:=d�litel
	mov	esi,ebx
	mov	edi,eax
	mov	ebx,[a2]
;vyn�sob d�lenec i d�litel, aby d�litel za��nal velkou cifrou
	xor	eax,eax
	mov	edx,1
	mov	ecx,[ebx] ;prvn� cifra d�litele
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
;p�ekop�ruj do zbytku d�lenec
@@c:	call	copyx
	mov	[d],0
;exponent zbytku
@@12:	mov	eax,[edi-4]
	sub	eax,[esi-4]
	mov	[edi-4],eax
	mov	dword ptr [edi-8],0
;edi:=v�sledek, esi:=zbytek
	mov	esi,edi
	mov	edi,[a0]
;porovn�n� d�lence a d�litele
	mov	ecx,esi
	mov	edx,ebx
	call	cmpu
	jna	@@3
	inc	dword ptr [esi-4]
	dec	dword ptr [edi-4]
;nastav d�lku v�sledku
@@3:	mov	ecx,[edi-16]
	mov	[edi-12],ecx
	push	ecx	;po��tadlo for cyklu
;[d1]:= prvn� cifra d�litele
	mov	eax,[ebx]
	mov	[d1],eax
;[d2]:= druh� cifra d�litele
	mov	eax,[ebx+4]
	mov	[d2],eax
;hlavn� cyklus, esi=zbytek, ebx=d�litel
;edi=pr�v� po��tan� cifra v�sledku
@@lp:	xor	edx,edx
	cmp	[esi-12],edx
	jz	@@e	;zbytek je 0
;na�ti nejvy��� ��d d�lence do edx:eax
	mov	eax,[esi]
	cmp	[esi-4],edx
	jz	@@4
	mov	[edi],edx ;0
	jl	@@w	;zbytek < d�litel
	mov	edx,eax
	xor	eax,eax
	cmp	dword ptr [esi-12],1
	jz	@@4	;jednocifern� zbytek
	mov	eax,[esi+4]
@@4:	cmp	edx,[d1]
	jb	@@9
;v�sledek d�len� je v�t�� ne� 32-bit.
	mov	ecx,eax
	stc
	sbb	eax,eax	;0xFFFFFFFF
	add	ecx,[d1]
	jc	@@8
	jmp	@@10
;odhadni v�slednou cifru
@@9:	div	[d1]
	mov	ecx,edx	;zbytek d�len�
@@10:	push	eax	;uchovej v�slednou cifru
	mul	[d2]
	sub	edx,ecx
	jc	@@11
	mov	ecx,[esi-4]
	inc	ecx
	cmp	dword ptr [esi-12],ecx
	jbe	@@f	;jednocifern� nebo dvoucifern� zbytek
	sub	eax,[esi+4*ecx]
	sbb	edx,0
	jc	@@11
;dokud je zbytek z�porn�, sni�uj v�slednou cifru
@@f:	mov	ecx,eax
	or	ecx,edx
	jz	@@11
	dec	dword ptr [esp]
	sub	eax,[d2]
	sbb	edx,[d1]
	jnc	@@f
@@11:	pop	eax
;od zbytku ode�ti d�litel * eax
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
;oprav v�sledek, aby zbytek nebyl z�porn�
	cmp	byte ptr [esi-8],0
	jz	@@w	;zbytek je kladn�
	cmp	dword ptr [esi-12],0
	jz	@@w	;zbytek je nulov�
@@d:	dec	dword ptr [edi]
	push	esi
	push	ebx
	push	esi
	push	[t1]
	call	PLUSX
	mov	esi,[t1]
	pop	[t1]
;pro jistotu je�t� zkontroluj znam�nko zbytku
	cmp	dword ptr [esi-12],0
	jz	@@w	;zbytek je nulov�
	cmp	byte ptr [esi-8],0
	jnz	@@d	;tento skok se asi nikdy neprovede
;posun na dal�� cifru v�sledku
@@w:	add	edi,4
	inc	dword ptr [esi-4]
	pop	ecx
	dec	ecx
	push	ecx
	jnz	@@lp
;oprav d�lku v�sledku (p�i nulov�m zbytku)
@@e:	pop	ecx
	mov	eax,[a0]
	sub	[eax-12],ecx
;sma� pomocn� prom�nn�
	mov	eax,esi
	call	@freex
	mov	eax,[t1]
	call	@freex
	mov	eax,[t2]
	call	@freex
	mov	eax,[d]
	call	@freex
;obnov znam�nko a exponent d�litele
@@r:	mov	ebx,[a2]
	pop	dword ptr [ebx-4]
	pop	dword ptr [ebx-8]
@@ret:	ret
DIVX2	endp
;-------------------------------------
MULTX1	proc 	uses esi edi ebx a0,a1,a2
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
	cmp	dword ptr [ebx-12],-2
	jnz	@@f1
;fraction
@@f0:	sub	esp,HED+8
	lea	eax,[esp+HED]
	mov	dword ptr [eax-16],2
	mov	dword ptr [eax-12],-2
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
@@f1:   cmp	dword ptr [ecx-12],-2
	jnz	@@f2
	mov	eax,ecx
	mov	ecx,ebx
	mov	ebx,eax
	jmp	@@f0
;spo�ti znam�nko
@@f2:	mov	eax,[ebx-8]
	xor	eax,[ecx-8]
	mov	[edx-8],eax
;spo�ti v�sledn� exponent
	mov	eax,[ebx-4]
	dec	eax
	add	eax,[ecx-4]
	jno	@@1
	call	overflow
	jmp	@@ret
@@1:	mov	[edx-4],eax	;exponent
;zjisti d�lku v�sledku
	mov	eax,[edx-16]
	add	eax,10          ;zv�t�i d�lku kv�li p�esnosti
	mov	[edx-16],eax
	mov	[edx-12],eax
	mov	esi,[ebx-12]
	add	esi,[ecx-12]
	dec	esi
	cmp	esi,eax
	jnb	@@2
;v�sledek nem��e b�t del�� ne� sou�et d�lek operand�
	mov	[edx-12],esi
@@2:	mov	eax,[ecx-12]
	lea	eax,[ecx+4*eax]
	mov	[e2],eax
;vynuluj v�sledek
	mov	ecx,[edx-12]
	mov	edi,edx
	xor	eax,eax
	cld
	rep stosd
;esi:= po��te�n� pozice v 1.operandu
	mov	ecx,[a2]
	mov	esi,[edx-12]
	mov	eax,[ebx-12]
	lea	edi,[edx+4*esi-4]
	cmp	eax,esi
	lea	esi,[ebx+4*esi-4]
	jnc	@@7
	lea	esi,[ebx+4*eax-4]
;ebx:= po��te�n� pozice v 2.operandu
@@7:	mov	eax,[edx-12]
	sub	eax,[ebx-12]
	lea	ebx,[ecx+4*eax]
	cmp	ebx,ecx
	jnc	@@5
	mov	ebx,ecx
;vn�j�� cyklus p�es 1.operand
@@5:	push	ebx
	push	esi
	push	edi
	xor	ecx,ecx		;p�enos
;vnit�n� cyklus p�es 2.operand
@@lp:	mov	eax,[esi]
	mul	dword ptr [ebx]
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
;zapi� p�enos
	add	[edi],ecx
	pop	edi
	pop	esi
	pop	ebx
;posu� se na dal�� ��d
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
	sub	dword ptr [edi-16],10
	call	round
@@ret:	ret
MULTX1	endp
;-------------------------------------
;vrac� zbytek d�len� 32-bitov�m ��slem
;nem� smysl pro re�ln� operand
MODI	proc 	uses esi ebx a1,ai
	mov	esi,[a1]
	mov	ebx,[ai]
	test	ebx,ebx
	jnz	@@0
	lea	eax,[E_1010]
	push	eax
	push	1010
	call	cerror
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
;hlavn� cyklus
@@lp:	mov	eax,[esi]
	add	esi,4
	div	ebx
	dec	ecx
	jnz	@@lp
	test	edx,edx
	jz	@@r   ;nulov� zbytek
	mov	esi,[a1]
	mov	ecx,[esi-4]
	sub	ecx,[esi-12]
	jle	@@r
;za vstupn� operand dopl�uj nuly a pokra�uj v d�len�
@@lp2:	xor	eax,eax
	div	ebx
	dec	ecx
	jnz	@@lp2
@@r:	mov	eax,edx
	ret
MODI	endp
;-------------------------------------
;vyp�e ��slo do bufferu
;neum� zobrazit exponent -> ��rka mus� b�t uvnit� mantisy !!!
WRITEX1	proc 	uses esi edi ebx buf,a1
	mov	esi,[a1]
	mov	eax,[esi-12]
	mov	edx,[buf]
	test	eax,eax
	jnz	@@1
;nula
	mov	byte ptr [edx],48
	inc	edx
	mov	byte ptr [edx],0
	ret
;znam�nko
@@1:	cmp	dword ptr [esi-8],0
	jz	@@5
	mov	byte ptr [edx],45  ;'-'
	inc	edx
	mov	[buf],edx
@@5:	mov	byte ptr [edx],0
;okop�ruj operand
	mov	eax,[a1]
	call	@newcopyx
	push	eax
	mov	edi,eax
	mov	eax,[edi-4]
	test	eax,eax
	js	@@f  ;��rka je vlevo od mantisy
	cmp	eax,[edi-16]
	jg	@@f  ;��rka je vpravo od mantisy
	mov	edx,[edi-12]
	sub	eax,edx
	jle	@@3
;vynuluj ��st za mantisou
	mov	ecx,eax
	push	edi
	lea	edi,[edi+4*edx]
	xor	eax,eax
	rep stosd
	pop	edi
;alokuj buffer pro meziv�sledek vlevo od te�ky
@@3:	mov	ecx,[edi-4]
	inc	ecx
	shl	ecx,2
	lea	eax,[ecx+2*ecx] ; *12
	cmp	[base],8
	jnc	@@a
	lea	eax,[ecx*8] ; *32
@@a:	push	eax
	call	Alloc
	mov	esi,eax
	mov	[esp],eax
;esi=buf, edi=x, ob� hodnoty jsou i na z�sobn�ku
	mov	ecx,[edi-4]
	test	ecx,ecx
	jnz	@@high
;p�ed te�kou je nula
	mov	eax,[buf]
	mov	byte ptr [eax],48  ;'0'
	inc	eax
	jmp	@@tecka

;��st vlevo od te�ky
@@high:	mov	edx,[base]
	mov	ebx,[dwordMax+edx*4]
@@h1:	push	ecx
	push	edi
	test	ebx,ebx
	jnz	@@h2
	mov	edx,[edi+4*ecx-4]
	jmp	@@h3
;vyd�l ��slo p�ed te�kou mocninou z�kladu
@@h2:	xor	edx,edx
@@lpd:	mov	eax,[edi]
	div	ebx
	mov	[edi],eax
	add	edi,4
	dec	ecx
	jnz	@@lpd
;edx=skupina ��slic
@@h3:	mov	edi,[base]
	mov	eax,edx
	mov	cl,[dwordDigitsI+edi]
@@2:	xor	edx,edx
	div	edi
;zbytek d�len� je v�sledn� ��slice
@@d:	mov	dl,[digitTab+edx]
	mov	[esi],dl
	inc	esi
	dec	cl
	jnz	@@2
;p�esun na dal�� �ty�byte
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
;u��zni po��te�n� nuly
@@tr:	dec	esi
	cmp	byte ptr [esi],48
	jz	@@tr
;obra� v�sledek
	mov	eax,[buf]
@@rev:	mov	cl,[esi]
	mov	[eax],cl
	inc	eax
	dec	esi
	cmp	esi,[esp]
	jae	@@rev
;eax=ukazatel za celou ��st v�sledku
@@tecka:
	mov	edi,[esp+4]
	mov	edx,[edi-4]
	mov	ecx,[edi-12]
	mov	ebx,[edi-16]
	lea	edi,[edi+4*edx]
	sub	ecx,edx
	jle	@@end  ;cel� ��slo
;edi=desetinn� ��st okop�rovan�ho vstupu,  ecx=jej� d�lka
;vypi� te�ku
	mov	byte ptr [eax],46  ;'.'
	inc	eax
	mov	esi,eax
;odhadni po�et cifer
	push	ebx
	mov	eax,[base]
	fild	dword ptr [esp]
	fld	qword ptr [dwordDigits+8*eax]
	fmul
	fistp	dword ptr [esp]
	pop	edx
	sub	edx,5
	dec	ecx
;��st vpravo od te�ky
;edx=po�et cifer, esi=v�stup, edi=ukazatel na desetinnou ��st, ecx=d�lka-1
@@low:	push	edx
	push	edi
	push	ecx
	mov	eax,[base]
	mov	eax,[dwordMax+eax*4]
	test	eax,eax
	jnz	@@low1
	mov	ebx,[edi]
	add	dword ptr [esp+4],4
	dec	dword ptr [esp]
	jmp	@@low2
@@low1:	push	esi
;n�soben� mocninou z�kladu
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
;ebx=skupina ��slic
@@low2:	mov	eax,ebx
	mov	edi,[base]
	movzx	ecx,[dwordDigitsI+edi]
	mov	ebx,ecx
@@d2:	xor	edx,edx
	div	edi
;zbytek d�len� je v�sledn� ��slice
	mov	dl,[digitTab+edx]
	dec	ecx
	mov	[esi+ecx],dl
	jnz	@@d2
	add	esi,ebx
	pop	ecx
	pop	edi
	pop	edx
;po��tadlo cyklu
	sub	edx,ebx
	ja	@@tc
	add	esi,edx
	jmp	@@lend
@@tc:	test	ecx,ecx
	js	@@lend
	mov	eax,[error]
	test	eax,eax
	jnz	@@lend
;u��zni koncov� nuly vstupn�ho operandu
	mov	eax,[edi+4*ecx]
	test	eax,eax
	jnz	@@low
	dec	ecx
	jns	@@low
@@lend:	mov	eax,esi
;zapi� koncovou nulu
@@end:	mov	byte ptr [eax],0
;sma� pomocn� buffery
	call	Free
	pop	edx
@@f:	pop	eax
	call	@freex
	ret
WRITEX1	endp
;-------------------------------------
;znak [esi] p�evede na ��slo ebx
;p�i chyb� vrac� CF=1
rdDigit	proc
	xor	ebx,ebx
	mov	bl,byte ptr [esi]
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
@@1:	cmp	ebx,[baseIn]
	cmc
@@ret:	ret
rdDigit	endp
;-------------------------------------
;na�te kr�tk� desetinn� ��slo a p�evede ho na zlomek
;vrac� CY=1, pokud je ��slo moc velk�, jinak [eax]=konec vstupu
;[edi]=v�sledek, [esi]=vstup
;m�n� ebx
rdFrac	proc	uses esi
	cmp	dword ptr [edi-16],2
	jc	@@ret
	mov	ecx,-1  ;d�lka desetinn� ��sti
;spo�ti �itatel
	xor	eax,eax
@@lpn:	test 	ecx,ecx
	jge	@@1
	cmp	byte ptr [esi],46  ;'.'
	jnz	@@3
	inc	esi
@@1:	inc	ecx
@@3:	call	rdDigit
	jc	@@4
	inc	esi
	mov	edx,[baseIn]
	mul	edx
	test	edx,edx
	jnz	@@e
	add	eax,ebx
	jnc	@@lpn
	jmp	@@e
@@4:	mov	[edi],eax
;spo�ti jmenovatel
	mov	eax,1
	mov	ebx,[baseIn]
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
rdFrac	endp
;-------------------------------------
READX1	proc	uses esi edi ebx a1,buf
	mov	edi,[a1]
	mov	esi,[buf]
;vypl� d�lku a vynuluj exponent
	mov	eax,[edi-16]
	xor	ecx,ecx
	mov	[edi-12],eax
	mov	[edi-4],ecx
	cmp	[esi],cl
	jnz	@@1
;pr�zdn� �et�zec => nula
	mov	[edi-12],ecx
	mov	eax,esi
@@ret:	ret
;znam�nko
@@1:	cmp	byte ptr [esi],45	; -
	mov	[edi-8],ecx
	jnz	@@5
	inc	esi
	inc	byte ptr [edi-8]
@@5:	cmp	byte ptr [esi],43	; '+'
	jnz	@@f
	inc	esi
@@f:	call	rdFrac
	jnc	@@ret
;��st nalevo od te�ky
	xor	ecx,ecx
;na�ti dal�� ��slici
@@m0:	call	rdDigit
	jc	@@me
	inc	esi
	test	ecx,ecx
	jz	@@m1
;n�soben� z�kladem a p�i�ten� ebx
	push	edi
	push	esi
	push	ecx
	lea	edi,[edi+4*ecx]
	neg	ecx
	mov	esi,[baseIn]
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
;zapi� p�ete�en� a zv�t�i velikost v�sledku
@@m1:	mov	[edi+4*ecx],ebx
	inc	ecx
	inc	dword ptr [edi-4]
	cmp	ecx,[edi-12]
	jbe	@@m0
;ztr�ta p�esnosti, vstupn� �et�zec je moc dlouh�
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
;obr�cen� v�sledku
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
;te�ka
	mov	edi,[a1]
	mov	al,[esi]
	mov	[edi-12],ecx
	cmp	al,46	; '.'
	jnz	@@end  ;cel� ��slo
;doje� na konec, a potom �ti ��slo pozp�tku
@@sk:	inc	esi
	call	rdDigit
	jnc	@@sk
 	push	esi  ;n�vratov� hodnota
;desetinn� ��sla maj� maxim�ln� d�lku
	mov	edx,[edi-16]
	xor	eax,eax
	mov	[edi-12],edx
	push	edi  ;ukazatel na v�sledek
;vynuluj desetinnou ��st v�sledku
	sub	edx,ecx
	test	edx,edx
	jz	@@de
	lea	edi,[edi+4*ecx]
	mov	ecx,edx
	push	edi
	cld
	rep stosd
;edi:=ukazatel na desetinnou ��st, ecx:=jej� d�lka, esi:=konec vstupu
	pop	edi
	mov	ecx,edx
	mov	esi,[esp+4]
;na�ti dal�� ��slici do edx
@@d0:	dec	esi
	call	rdDigit
	jc	@@de
	mov	edx,ebx
;d�len� z�kladem
	push	ecx
	push	edi
	mov	ebx,[baseIn]
@@lpd:	mov	eax,[edi]
	div	ebx
	mov	[edi],eax
	add	edi,4
	dec	ecx
	jnz	@@lpd
	pop	edi
	pop	ecx
	jmp	@@d0
;u��znut� koncov�ch nul a normalizace
@@de:	pop	edi
@@trim:	call	norm
	pop	eax
	ret
@@end:	push	esi
	jmp	@@trim
READX1	endp
;-------------------------------------
FFACTI	proc 	uses esi edi ebx a0,ai
	mov	eax,[a0]
	call	@onex
	mov	esi,[ai]
	test	esi,esi
	jz	@@ret     ;0!! =1
@@lp:	push	esi
	mov	edi,[a0]
	call	mult1
	pop	esi
	mov	eax,[error]
	test	eax,eax
	jnz	@@ret
	dec	esi
	jz	@@ret
	dec	esi
	jnz	@@lp
@@ret:	ret
FFACTI	endp
;-------------------------------------
;[ecx]=eax^2
sqr	proc
	mul	eax
	test	edx,edx
	jnz	@@1
	mov	[ecx],eax
	mov	[ecx-4],edx
	mov	dword ptr [ecx-12],1
	ret
@@1:	mov	[ecx],edx
	mov	[ecx+4],eax
	mov	dword ptr [ecx-4],1
	mov	dword ptr [ecx-12],2
	ret
sqr	endp
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
@sqrti	proc	uses esi edi
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
@sqrti	endp

;odmocnina ze 64-bitov�ho ��sla
;zaokrouhluje sm�rem dol�
SQRTI	proc
	mov	eax,[esp+4]
	mov	edx,[esp+8]
	call	@sqrti
	ret	8
SQRTI	endp
;-------------------------------------
SQRTX2	proc 	uses esi edi ebx a0,a1
local	sq,s2,s3,s4,d1,d2,k,ro	
	mov	ecx,[a1]	;operand
	mov	edx,[a0]	;v�sledek
	xor	esi,esi
	cmp	[ecx-12],esi
	mov	[edx-12],esi
	jz	@@ret		; sqrt(0)=0
	cmp	[ecx-8],esi
	jz	@@1
	lea	eax,[E_1008]
	push	eax
	push	1008
	call	cerror
	add	esp,8
	jmp	@@ret
@@1:	cmp	dword ptr [ecx-12],-2
	jnz	@@f2
;zlomek
	mov	edi,edx
	mov	esi,ecx
	mov	ecx,2
@@nd:	lea	ebx,[esi+ecx*4-4]
	push	0
	push	dword ptr [ebx]
	fild	qword ptr [esp]
	fsqrt
	fistp	dword ptr [esp]
	pop	eax
	pop	edx
	lea	edx,[edi+ecx*4-4]
	mov	[edx],eax
	mul	eax
	cmp	eax,[ebx]
	jnz	@@fe
	dec	ecx
	jnz	@@nd
	and	dword ptr [edi-8],0
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
;alokuj prom�nnou sq
@@f2:	sub	esp,8
	mov	[sq],esp
	sub	esp,HED
;znam�nko
	mov	[esp+HED-8],esi	;kladn�
	mov	[edx-8],esi
;max(d�lka v�sledku+3, d�lka operandu)+3
	mov	eax,[edx-16]
	mov	[edx-12],eax
	add	eax,3
	mov	edi,[ecx-12]
	cmp	eax,edi
	jbe	@@6
	mov	edi,eax
@@6:	add	edi,3
;alokuj pomocnou prom�nnou s2
	push	eax
	call	@allocx
	mov	[s2],eax
	pop	eax
;alokuj pomocnou prom�nnou s4
	call	@allocx
	mov	[s4],eax
;alokuj pomocnou prom�nnou s3
	mov	eax,edi
	call	@allocx
	mov	[s3],eax
;alokuj zbytek
	mov	eax,edi
	call	@allocx
	mov	edi,eax
	mov	esi,[a1]
;na�ti nejvy��� ��d
	xor	edx,edx
	mov	ebx,[s2]
	mov	[ebx-4],edx
	test	dword ptr [esi-4],1	;parita exponentu
	jz	@@2
	mov	eax,[esi]
	jmp	@@3
@@2:	mov	edx,[esi]
	mov	eax,[esi+4]
;vypo�ti odmocninu z nejvy���ho ��du
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
;zv�t�i operand
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
	mov	dword ptr [esi-4],1
;zapi� prvn� ��slici do v�sledku
	mov	edx,[esi]
	mov	eax,[esi+4]
	call	@sqrti
	mov	[edi],eax
;po��tadlo cyklu
	push	dword ptr [edi-12]
;ebx=d�litel, edi=v�sledek, esi=zbytek
;od zbytku ode�ti druhou mocninu p�idan� ��slice
@@lp:	mov	eax,[edi]
	test	eax,eax
	jz	@@ad	;nulovou ��slici neode��tej od zbytku
	mov	ecx,[sq]
	call	sqr
	push	[sq]
	push	esi
	push	[s3]
	call	MINUSX
	mov	eax,[s3]
	cmp	byte ptr [eax-8],0
	jz	@@0
	cmp	dword ptr [eax-12],0
	jz	@@0
;zbytek je z�porn� ->  mus� se opravit
	dec	dword ptr [esi-4]
	dec	dword ptr [ebx-4]
	jmp	@@d
@@0:	mov	[s3],esi
	mov	esi,eax
;posu� ukazatel na v�sledek
@@ad:	mov	eax,[edi]	;pro v�sledn� zaokroulen�
	add	edi,4
;sni� po��tadlo
	pop	ecx
	dec	ecx
	js	@@z		;hotovo, podle eax zaokrouhli
	cmp	dword ptr [esi-12],0
	jz	@@e		;zbytek je 0 -> zkra� v�sledek
	push	ecx
;p�idej dvojn�sobek dal�� ��slice k s2
	mov	ecx,[ebx-12]
	shl	eax,1
	mov	[ebx+4*ecx],eax
	inc	ecx
	mov	[ebx-12],ecx
	jnc	@@5
;p�ete�en� na konci s2
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
	pop	dword ptr [ebx-12]
;zvy� exponent zbytku
@@5:	inc	dword ptr [esi-4]
;zapamatuj si nejvy��� ��d d�litele
	mov	eax,[ebx]
	mov	[d1],eax
	xor	eax,eax
	cmp	dword ptr [ebx-12],1
	jz	@@7
	mov	eax,[ebx+4]
@@7:	mov	[d2],eax
;na�ti nejvy��� ��d zbytku
	xor	edx,edx
	mov	eax,[esi]
	mov	ecx,[ebx-4]
	cmp	[esi-4],ecx
	jz	@@4
	mov	dword ptr [edi],0	;zbytek < "d�litel"
	jl	@@o
	mov	edx,eax
	xor	eax,eax
	cmp	dword ptr [esi-12],1
	jz	@@4
	mov	eax,[esi+4]
;ochrana proti p�ete�en�
@@4:	cmp	edx,[d1]
	jb	@@9
	mov	ecx,eax
	stc
	sbb	eax,eax	;0xFFFFFFFF
	add	ecx,[d1]
	jc	@@8
	jmp	@@10
;odhadni v�slednou ��slici
@@9:	div	[d1]
	mov	ecx,edx	;zbytek d�len�
@@10:	push	eax	;uchovej v�slednou cifru
	mul	[d2]
	sub	edx,ecx
	jc	@@11
	mov	ecx,[esi-4]
	inc	ecx
	sub	ecx,[ebx-4]
	cmp	dword ptr [esi-12],ecx
	jbe	@@k	;jednocifern� nebo dvoucifern� zbytek
	sub	eax,[esi+4*ecx]
	sbb	edx,0
	jc	@@11
;dokud je zbytek z�porn�, sni�uj v�slednou cifru
@@k:	mov	ecx,eax
	or	ecx,edx
	jz	@@11
	dec	dword ptr [esp]
	sub	eax,[d2]
	sbb	edx,[d1]
	jnc	@@k
@@11:	pop	eax
;vyn�sob d�litel a ode�ti od zbytku
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
	cmp	byte ptr [esi-8],0
	jz	@@o	;zbytek je kladn�
	cmp	dword ptr [esi-12],0
	jz	@@o
;oprav v�sledek, aby zbytek nebyl z�porn�
@@d:	dec	dword ptr [edi]
	push	esi
	push	ebx
	push	esi
	push	[s3]
	call	PLUSX
	mov	esi,[s3]
	pop	[s3]
	cmp	dword ptr [esi-12],0
	jz	@@o
	cmp	byte ptr [esi-8],0
	jnz	@@d
@@o:	inc	dword ptr [esi-4]
	inc	dword ptr [ebx-4]
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
;sma� pomocn� prom�nn�
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
SQRTX2	endp
;-------------------------------------
overflow	proc c
	lea	eax,[E_1011]
	push	eax
	push	1011
	call	cerror
	add	esp,8
	ret
overflow	endp
;-------------------------------------
@ADDII@8:	mov	eax,ecx
@addii	proc
	add	eax,edx
	jno	@@r
	call	overflow	
@@r:	ret
@addii	endp
;-------------------------------------

MULTX:	jmp	MULTX@12
DIVX:	jmp	DIVX@12
SQRTX:	jmp	SQRTX@8

;fastcall
public	pascal @ALLOCX@4,pascal @FREEX@4,pascal @NEWCOPYX@4
public	pascal @SETX@8,pascal @SETXN@8,pascal @ZEROX@4,pascal @ONEX@4,pascal @FRACTOX@4,pascal @NORMX@4
public	pascal @NEGX@4,pascal @ABSX@4,pascal @SIGNX@4,pascal @TRUNCX@4,pascal @INTX@4,pascal @CEILX@4,pascal @ROUNDX@4,pascal @FRACX@4,pascal @SCALEX@8,pascal @ADDII@8

;stdcall
public	COPYX,WRITEX1,READX1
public	MULTX1,MULTI,MULTIN,MULTI1,DIVX2,DIVI,MODI
public	PLUSX,MINUSX,PLUSU,MINUSU,ANDU,ORU,XORU
public	CMPX,CMPU,FFACTI,SQRTX2,SQRTI
public	ANDU@12,ORU@12,XORU@12

;cdecl
public	pascal _ALLOCN

	end
