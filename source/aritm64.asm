; (C) 2005-2011  Petr Lastovicka
 
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License.

;compilation:  JWASM.EXE -win64 aritm64.asm
;http://www.japheth.de/JWasm.html

;the caller must allocate memory for a result

;-32 maxlen
;-24 len
;-16 sign
;-8  exponent
;lengths and exponent are in qword

option casemap:none
option procalign:16

extrn	Alloc:proc, Free:proc, cerror:proc
extrn	base:dword, baseIn:dword, error:dword, dwordDigits:qword
public	digitTab
public	overflow

RES	equ	104
HED	equ	32
;-------------------------------------
.data
E_1008	db	'Negative operand of sqrt',0
E_1010	db	'Division by zero',0
E_1011	db	'Overflow, number is too big',0
digitTab	db	'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ',0
qwordDigitsI db	0,0, 64, 40, 32, 26, 24, 22, 20, 20, 18, 18, 16, 16, 16, 16, 16
	db	14, 14, 14, 14, 14, 14, 14, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12
qwordMax	dq	0, 0, 0, 12157665459056928801, 0, 1490116119384765625, 4738381338321616896, 3909821048582988049
	dq	1152921504606846976, 12157665459056928801, 1000000000000000000, 5559917313492231481, 184884258895036416
	dq	665416609183179841, 2177953337809371136, 6568408355712890625, 0
	dq	168377826559400929, 374813367582081024, 799006685782884121, 1638400000000000000, 3243919932521508681
	dq	6221821273427820544, 11592836324538749809, 36520347436056576, 59604644775390625, 95428956661682176
	dq	150094635296999121, 232218265089212416, 353814783205469041, 531441000000000000, 787662783788549761
	dq	1152921504606846976, 1667889514952984961, 2386420683693101056, 3379220508056640625, 4738381338321616896

.code
;-------------------------------------
;[rdi]:=[rsi]
;nemÏnÌ rsi,rdi,rbx
copyx	proc	uses rsi rdi
	cmp	rsi,rdi
	jz	@@ret
	cld
	mov	rcx,[rsi-24]
	cmp	rcx,-2
	jnz	@@0
;zlomek
	mov	[rdi-24],rcx
	mov	rax,16
	sub	rdi,rax
	sub	rsi,rax
	movsq
	movsq
	movsq
	movsq
	ret
@@0:	mov	rax,[rdi-32]
	mov	rdx,rdi
	cmp	rcx,rax
	pushf
	jbe	@@1
	mov	rcx,rax	;zdroj je delöÌ neû v˝sledek
@@1:	mov	[rdi-24],rcx
	add	rcx,2	;okopÌruj takÈ znamÈnko a exponent
	mov	rax,16
	sub	rdi,rax
	sub	rsi,rax
	rep movsq
	popf
	jbe	@@ret
;zaokrouhlenÌ
	mov	rax,[rsi]	;prvnÌ ËÌslice, kter· uû se neveöla do v˝sledku
	test	rax,rax
	jns	@@ret
	mov	rdi,rdx
	push	rbx
	call	incre
	pop	rbx
@@ret:	ret
copyx	endp

COPYX	proc 	uses rsi rdi a0,a1
	mov	rdi,rcx
	mov	rsi,rdx
	call	copyx
	ret
COPYX	endp
;-------------------------------------
;zarovn· z·sobnÌk na 16 byt˘ a rezervuje na nÏm 32 byt˘
AllocA	proc
	push	rbp
	mov	rbp,rsp
	and	rsp,-16
	sub	rsp,32
	call	Alloc
	leave
	ret
AllocA	endp
;-------------------------------------
;alokuje ËÌslo s mantisou dÈlky rax
ALLOCX	proc
	push	rcx
	lea	rcx,[rcx*8+HED+RES]	;vËetnÏ hlaviËky a rezervovanÈho mÌsta
	call	AllocA
	pop	rcx
	test	rax,rax
	jz	@@ret
	add	rax,HED
	mov	[rax-32],rcx
	mov	qword ptr [rax-24],0	;inicializuj na nulu
	mov	qword ptr [rax-8],1
	mov	qword ptr [rax-16],0
@@ret:	ret
ALLOCX	endp

;promÏnn˝ poËet parametr˘
ALLOCN:
ALLOCNX	proc
	mov	[rsp+8],rcx  ;n
	mov	[rsp+16],rdx ;len
	mov	[rsp+24],r8  ;kam se zapÌöe ukazatel
	mov	[rsp+32],r9
	lea	rax,[rdx*8+HED+RES]	;vËetnÏ hlaviËky a rezervovanÈho mÌsta
	push	rax
	mul	rcx
	mov	rcx,rax
	push	rax
	call	AllocA
	pop	rdx	;celkov· dÈlka v bytech
	pop	rcx	;dÈlka jednoho ËÌsla v bytech
	test	rax,rax
	jz	@@ret
	lea	rax,[rax+rdx+HED]
@@lp:	
	sub	rax,rcx
	mov	rdx,[rsp+16]
	mov	[rax-32],rdx
	mov	qword ptr [rax-24],0	;inicializuj na nulu
	mov	qword ptr [rax-8],1
	mov	qword ptr [rax-16],0
	mov	rdx,[rsp+8]	;poËÌtadlo
	mov	rdx,[rsp+8*rdx+16]
	mov	[rdx],rax		;v˝sledn˝ ukazatel
	dec	qword ptr [rsp+8]
	jnz	@@lp
@@ret:	ret
ALLOCNX	endp
;-------------------------------------
;vytvo¯Ì kopii operandu
NEWCOPYX	proc	uses rsi rdi
	mov	rsi,rcx
	mov	rcx,[rsi-32]
	lea	rcx,[rcx*8+HED+RES]
	call	AllocA
	test	rax,rax
	jz	@@ret
;okopÌruj vËetnÏ hlaviËky
	mov	rdi,rax
	mov	rcx,[rsi-24]
	test	rcx,rcx
	jns	@@1
	neg	rcx
@@1:	add	rcx,HED/8
	mov	rdx,HED
	add	rax,rdx
	sub	rsi,rdx
	cld
	rep movsq
@@ret:	ret
NEWCOPYX	endp
;-------------------------------------
;uvolnÏnÌ pamÏti alokovanÈ funkcÌ ALLOCX nebo ALLOCNX
FREEX	proc
	test	rcx,rcx
	jz	@@ret
	sub	rcx,HED
	jmp	FreeA
@@ret:	ret
FREEX	endp

FreeA	proc
	push	rbp
	mov	rbp,rsp
	and	rsp,-16
	sub	rsp,32
	call	Free
	leave
	ret
FreeA	endp
;-------------------------------------
;bezznamÈnkov˝ integer
SETX	proc
	test	rdx,rdx
	jz	ZEROX
	mov	qword ptr [rcx-24],-2
	mov	qword ptr [rcx-16],0
	mov	qword ptr [rcx-8],1
	mov	[rcx],rdx
	mov	qword ptr [rcx+8],1
	ret
SETX	endp

;integer
SETXN	proc
	test	rdx,rdx
	jns	SETX
	neg	rdx
	mov	rax,1
	mov	qword ptr [rcx-24],-2
	mov	qword ptr [rcx-16],rax
	mov	qword ptr [rcx-8],rax
	mov	[rcx],rdx
	mov	[rcx+8],rax
	ret
SETXN	endp

@zerox	proc
	and	qword ptr [rax-24],0
	ret
@zerox	endp

ZEROX	proc
	and	qword ptr [rcx-24],0
	ret
ZEROX	endp

ONEX	proc
	mov	rdx,1
	mov	qword ptr [rcx-24],-2
	and	qword ptr [rcx-16],0
	mov	[rcx-8],rdx
	mov	[rcx],rdx
	mov	[rcx+8],rdx
	ret
ONEX	endp
;-------------------------------------

IF 0
;[rsi]-=rax;  nesmÌ dojÌt k podteËenÌ
decra:	sub	[rsi],rax
	jnc	trim
ENDIF

@@:	sub	rsi,8
;[rsi]--
decr:	sub	qword ptr [rsi],1
	jc	@b
;u¯ÌznutÌ koncov˝ch nul [rdi], nesmÌ b˝t nula nebo zlomek !
;nemÏnÌ rdi,rbx, mÏnÌ rsi
trim:	mov	rcx,[rdi-24]
	lea	rsi,[rdi+8*rcx-8]
;rsi ukazuje na poslednÌ ËÌslici
trims:	mov	rax,[rsi]
	test	rax,rax
	jnz	@ret
	sub	rsi,8
	dec	qword ptr [rdi-24]
	jnz	trims
	ret

IF 0
;dekrementace [rdi];  Ë·rka musÌ b˝t uvnit¯ mantisy; nesmÌ dojÌt k podteËenÌ
decre:	mov	rcx,[rdi-24]
	lea	rsi,[rdi+8*rcx-8]
	jmp	decr
ENDIF

;-------------------------------------
;[rdi+rcx]+=rax
incri	proc
	mov	rdx,[rdi-24]
	mov	rbx,[rdi-32]
	test	rcx,rcx
	js	@@sh		;p¯ed ËÌslem
	lea	rsi,[rdi+8*rcx]
	cmp	rcx,rdx
	jc	incra		;uvnit¯ ËÌsla
	cmp	rcx,rbx
	ja	@@ret		;za ËÌslem d·l neû o 1
	mov	[rdi-24],rcx
	jnz	@@2
;tÏsnÏ za ËÌslem - rozhodne se o zaokrouhlenÌ
	test	rax,rax
	jns	@@ret
@@2:	setc	bl
;increment leûÌ v alokovanÈm mÌstÏ za ËÌslem
	push	rdi
	mov	[rsi],rax
;vynuluj oblast mezi ËÌslem a inkrementem
	xor	rax,rax
	lea	rdi,[rsi-8]
	sub	rcx,rdx
	std
	rep stosq
	cld
	pop	rdi
	test	bl,bl
	jz	round2
	inc	qword ptr [rdi-24]
@@ret:	ret
;inkrement je p¯ed ËÌslem - ËÌslo se posune vpravo
@@sh:	neg	rcx
	add	[rdi-8],rcx	;uprav exponent
	cmp	rcx,rbx
	ja	@@d		;ËÌslo je moc malÈ -> zruöÌm ho
	sub	rbx,rcx
	cmp	rbx,rdx
	push	rdi
	push	rcx
	jae	@@1
;ztr·ta p¯esnosti
	mov	rdx,[rdi-32]
	inc	rdx
	mov	[rdi-24],rdx
	jmp	@@cp
;zvÏtöenÌ dÈlky ËÌsla
@@1:	add	[rdi-24],rcx
	mov	rbx,rdx
;posun bloku dÈlky rbx smÏrem vpravo o rcx integer˘
@@cp:	lea	rsi,[rdi+8*rbx-8]
	lea	rdi,[rsi+8*rcx]
	mov	rcx,rbx
	std
	rep movsq
	cld
	pop	rcx
	pop	rdi
;zapiö inkrement na prvnÌ pozici
	mov	[rdi],rax
;vynuluj mÌsto mezi
	xor	rax,rax
	dec	rcx
	push	rdi
	add	rdi,8
	rep stosq
	pop	rdi
;zaokrouhli
	mov	rax,[rdi-32]
	cmp	rax,[rdi-24]
	jae	@@ret
	dec	qword ptr [rdi-24]
	jmp	roundi
@@d:	mov	[rdi],rax
	mov	qword ptr [rdi-24],1
	ret
incri	endp
;-------------------------------------
IF 0
;[rdi+rcx]-=rax
;nesmÌ dojÌt k podteËenÌ !
decri	proc
	test	rax,rax
	jz	@@ret
	mov	rdx,[rdi-24]
	mov	rbx,[rdi-32]
	lea	rsi,[rdi+8*rcx]
	cmp	rcx,rdx
	jc	decra		;uvnit¯ ËÌsla
	cmp	rcx,rbx
	ja	@@ret		;za ËÌslem d·l neû o 1
	jnz	@@3
;tÏsnÏ za ËÌslem - rozhodne se o zaokrouhlenÌ
	test	rax,rax
	jns	@@ret
@@3:	push	rcx
	push	rdi
	push	rax
	call	decre
	pop	rax
	pop	rdi
	pop	rcx
	mov	[rdi-24],rcx
;decrement leûÌ v alokovanÈm mÌstÏ za ËÌslem
	push	rdi
	neg	rax
	mov	[rsi],rax
	sbb	rax,rax		;0ffffffffffffffffH
	lea	rdi,[rsi-8]
	sub	rcx,rdx
	std
	rep stosq
	cld
	pop	rdi
	jmp	roundi
@@ret:	ret
decri	endp
ENDIF
;-------------------------------------
;p¯iËtenÌ jedniËky na konec mantisy [rdi]
incre:	mov	rcx,[rdi-24]
	lea	rsi,[rdi+8*rcx-8]
	jmp	incr
;rdi, [rsi]+=rax
incra:	add	[rsi],rax
	jnc	trim
	sub	rsi,8
	cmp	rsi,rdi
	jc	carry1
;rdi, [rsi]++
@incr:	inc	qword ptr [rsi]
	jnz	trim
	sub	rsi,8
incr:	cmp	rsi,rdi
	jnc	@incr
carry1:	mov	rax,1
;[rdi]=rax:[rdi]
carry:	test	rax,rax
	jz	@ret
	mov	rcx,[rdi-24]
	lea	rsi,[rdi+8*rcx-8]
	cmp	rcx,[rdi-32]
	mov	rbx,[rsi]
	jae	@1
;zvÏtöi dÈlku
	inc	qword ptr [rdi-24]
	xor	rbx,rbx
;zvÏtöi exponent
@1:	inc	qword ptr [rdi-8]
	jo	overflow
;posun celÈ mantisy o jeden ¯·d vpravo
@2:	push	rsi
	lea	rdi,[rsi+8]
	std
	rep movsq
	cld
;p¯id·nÌ p¯enosu
	mov	[rdi],rax
	test	rbx,rbx
	pop	rsi
	js	@incr
	jmp	trim
@ret:	ret

;-------------------------------------
;zmenöÌ dÈlku [rdi] na maximum a zaokrouhlÌ podle cifry za mantisou
round:	mov	rcx,[rdi-32]
	cmp	rcx,[rdi-24]
	jb	round1
	ret

;zvÏtöÌ dÈlku [rdi] o jedna nebo zaokrouhlÌ
roundi:	mov	rcx,[rdi-32]
	cmp	rcx,[rdi-24]
	ja	inclen
round1:	mov	[rdi-24],rcx
;podÌvej se do rezerovanÈho mÌsta za mantisou
	lea	rsi,[rdi+8*rcx]
round2:	mov	rax,[rsi]
	sub	rsi,8
	test	rax,rax
	jns	trims
;zaokrouhlenÌ nahoru
	jmp	incr
;pokud je jeötÏ volnÈ mÌsto, pak jen zvÏtöi dÈlku ËÌsla
inclen:	inc	qword ptr [rdi-24]
	ret
;-------------------------------------
;normalizace [rdi] - mantisa nebude zaËÌnat na nulu
;zmÏnÌ rsi,rdi
norm	proc
	mov	rcx,[rdi-24]
	test	rcx,rcx
	jz	@@ret
	xor	rdx,rdx
	cmp	[rdi],rdx
	jnz	@@ret
	mov	rax,rdi
@@lp:	add	rax,8
	dec	qword ptr [rdi-8]
	jo	@@nul
	dec	rcx
	jz	@@nul
	cmp	[rax],rdx
	jz	@@lp
@@e:	mov	rsi,rax
	mov	[rdi-24],rcx
	cld
	rep movsq
@@ret:	ret
@@nul:	mov	qword ptr [rdi-24],0
	ret
norm	endp
;-------------------------------------
;rax:=rcx:=gcd(rax,rcx), rdx:=0
gcd	proc
	jmp	gcd1
gcd0:	div	rcx
	mov	rax,rcx
	mov	rcx,rdx
gcd1:	xor	rdx,rdx
	test	rcx,rcx
	jnz	gcd0
	mov	rcx,rax
	ret
gcd	endp

;zkr·tÌ zlomek [rsi],[rdi]
reduce	proc
	mov	rax,[rsi]
	mov	rcx,[rdi]
	call	gcd
	mov	rax,[rsi]
	div	rcx
	mov	[rsi],rax
	mov	rax,[rdi]
	div	rcx
	mov	[rdi],rax
	ret
reduce	endp

fracReduce	proc
	push	rsi
	lea	rsi,[rdi+8]
	call	reduce
	pop	rsi
	jmp	fracexp
fracReduce	endp

fracexp	proc
;nastavÌ exponent a dÈlku zlomku
	mov	rcx,[rdi]
	cmp	rcx,[rdi+8]
	setae	al
	mov	[rdi-8],al
	mov	qword ptr [rdi-24],-2
	test	rcx,rcx
	jnz	@@ret
	and	qword ptr [rdi-24],0
@@ret:	ret
fracexp	endp



;-------------------------------------
;a0:=a1/ai
;a0 m˘ûe b˝t rovno a1
DIVI	proc 	uses rsi rdi rbx a0,a1,ai
	mov	[ai],r8
	mov	rsi,rdx
	mov	rdi,rcx
	mov	rbx,r8
	cmp	rbx,1
	ja	@@z
	jnz	@@e
;a0=a1*1
	call	copyx
	ret
;dÏlenÌ nulou
@@e:	lea	rdx,[E_1010]
	mov	rcx,1010
	sub	rsp,32
	call	cerror
	add	rsp,32
	ret
@@z:	cmp	qword ptr [rsi-24],-2
	jnz	@@0
;zlomek
	call	copyx
	lea	rsi,[ai]
	call	reduce
	mov	rbx,[ai]
	mov	rax,[rdi+8]
	mul	rbx
	jo	@@f1
	mov	[rdi+8],rax
	call	fracexp
	jmp	@@ret
;convert fraction to real
@@f1:	mov	rcx,rdi
	call	FRACTOX
	mov	rsi,rdi
;okopÌruj znamÈnko a exponent
@@0:	mov	rax,[rsi-8]
	mov	[rdi-8],rax
	mov	rdx,[rsi-16]
	mov	[rdi-16],rdx
;nastav dÈlku v˝sledku jako minimum z dÈlek operand˘
	mov	rcx,[rsi-24]
	mov	rax,[rdi-32]
	cmp	rcx,rax
	jbe	@@1
	mov	rcx,rax
@@1:	mov	[rdi-24],rcx
	test	rcx,rcx
	jz	@@ret     ; 0/ai=0
;naËti prvnÌ cifru dÏlence
	mov	rdx,[rsi]
	cmp	rdx,rbx
	jnc	@@2
;prvnÌ dÏlenÌ bude 128-bitovÈ a v˝sledek bude kratöÌ
	add	rsi,8
	dec	qword ptr [rdi-8]
	jno	@@6
;podteËenÌ -> v˝sledek nulov˝
	mov	qword ptr [rdi-24],0
	ret
@@6:	dec	qword ptr [rdi-24]
	dec	rcx
	jnz	@@4
;dÏlÏnec m· dÈlku 1 a je menöÌ neû dÏlitel
	mov	rsi,rdi
	jmp	@@3
@@2:	xor	rdx,rdx
;rcx=dÈlka v˝sledku, rbx=dÏlitel, rdx=hornÌ Ëty¯byte dÏlÏnce
@@4:	push	rdi


;hlavnÌ cyklus
	sub	rcx,3
	jle	@@lpi
@@lp:	mov	rax,[rsi]
	div	rbx
	mov	[rdi],rax
	mov	rax,[rsi+8]
	div	rbx
	mov	[rdi+8],rax
	mov	rax,[rsi+16]
	div	rbx
	mov	[rdi+16],rax
	mov	rax,[rsi+24]
	div	rbx
	mov	[rdi+24],rax
	add	rsi,32
	add	rdi,32
	sub	rcx,4
	jg	@@lp
@@lpi:	add	rcx,3
	jz	@@cy


@@lp1:	mov	rax,[rsi]
	add	rsi,8
	div	rbx
	mov	[rdi],rax
	add	rdi,8
	dec	rcx
	jnz	@@lp1

@@cy:	mov	rsi,rdi
	test	rdx,rdx
	pop	rdi
	jz	@@ret   ;nulov˝ zbytek
	mov	rcx,[rdi-24]
	jmp	@@3
;za vstupnÌ operand doplÚuj nuly a pokraËuj v dÏlenÌ
@@lp2:	xor	rax,rax
	div	rbx
	mov	[rsi],rax
	add	rsi,8
	inc	rcx
@@3:	cmp	rcx,[rdi-32]
	jnz	@@lp2
	sub	rsi,8
	mov	[rdi-24],rcx
;zaokrouhli podle zbytku
	shl	rdx,1
	jc	@@5
	cmp	rdx,rbx
	jnc	@@5
	call	trim
	jmp	@@ret
@@5:	call	incr
@@ret:	ret
DIVI	endp
;-------------------------------------
;konverze zlomku na desetinnÈ ËÌslo
FRACTOX:	cmp	qword ptr [rcx-24],-2
	jnz	@f
fractox:
	mov	qword ptr [rcx-24],1
	mov	qword ptr [rcx-8],1
	sub	rsp,32
	mov	r8,qword ptr [rcx+8]
	mov	rdx,rcx
	call	DIVI
	add	rsp,32
@@:	ret

;len=[rdi-32], a/b=[rsi]
;return rax
fractonewx2:
	cmp	qword ptr [rsi-24],-2
	mov	rcx,rsi
	jnz	NEWCOPYX
fractonewx:
	mov	rcx,[rdi-32]
	call	ALLOCX
	push	rax
	mov	rcx,[rsi]
	mov	[rax],rcx
	mov	rcx,[rsi+8]
	mov	[rax+8],rcx
	mov	rcx,rax
	call	fractox
	pop	rax
	ret
;-------------------------------------
;cmp [rdx],[rcx]
cmpu	proc	uses rsi rdi
;test na nulovost operand˘
	cmp	qword ptr [rdx-24],0
	jnz	@@1
	cmp	qword ptr [rcx-24],0
	jz	@@ret		;oba jsou 0
	jmp	@@gr2		;prvnÌ je 0
@@1:	cmp	qword ptr [rcx-24],1
	jc	@@gr1		;druh˝ je 0
;zlomky
	cmp	qword ptr [rdx-24],-2
	jnz	@@f1
	cmp	qword ptr [rcx-24],-2
	jnz	@@f2
;a/b ? c/d
	mov	rdi,rdx
	mov	rax,[rdx]
	mul	qword ptr [rcx+8]
	push	rdx
	push	rax
	mov	rax,[rcx]
	mul	qword ptr [rdi+8]
	pop	rcx
	pop	rdi
;rdi:rcx ? rdx:rax
	cmp	rdi,rdx
	jnz	@@ret
	cmp	rcx,rax
	jmp	@@ret
@@f1:	cmp	qword ptr [rcx-24],-2
	jnz	@@e
; x ? (a/b)
	mov	rsi,rcx
	mov	rdi,rdx
	call	fractonewx
	mov	rdx,rdi
	mov	rcx,rax
	jmp	@@r
; (a/b) ? x
@@f2:	mov	rsi,rdx
	mov	rdi,rcx
	call	fractonewx
	mov	rdx,rax
	mov	rcx,rdi
@@r:	mov	rsi,rax
	call	cmpu
	pushf
	mov	rcx,rsi
	call	FREEX
	popf
	ret
;exponenty
@@e:	mov	rax,[rdx-8]
	cmp	rax,[rcx-8]
	jg	@@gr1
	jl	@@gr2
;nejvyööÌ ¯·d
	mov	rax,[rdx]
	cmp	rax,[rcx]
	jnz	@@ret
	mov	rax,[rdx-24]
	lea	rsi,[rdx+8*rax]
	mov	rax,[rcx-24]
	lea	rdi,[rcx+8*rax]
	add	rdx,8
	add	rcx,8
;mantisy
@@lp:	cmp	rdx,rsi
	jnc	@@2
	cmp	rcx,rdi
	jnc	@@3
	mov	rax,[rdx]
	cmp	rax,[rcx]
	jnz	@@ret
	add	rdx,8
	add	rcx,8
	jmp	@@lp
;prvnÌ operand je kratöÌ nebo jsou si rovny
@@2:	cmp	rcx,rdi	;ZF=1 nebo CF=1
	ret
;druh˝ operand je kratöÌ
@@3:	cmp	rsi,rdx	;nastav ZF=0, CF=0
	ret
@@gr2:	stc
	ret
@@gr1:	clc
@@ret:	ret
cmpu	endp

CMPU	proc 	a1,a2
	call	cmpu
	jb	@@gr1
	ja	@@gr2
	xor	rax,rax
	ret
@@gr1:	mov	rax,1
	ret
@@gr2:	mov	rax,-1
	ret
CMPU	endp
;-------------------------------------
;porovn·nÌ, v˝sledek -1,0,+1
CMPX	proc 	a1,a2
	cmp	qword ptr [rdx-24],0
	jnz	@@1
	cmp	qword ptr [rcx-24],0
	jz	@@eq
;porovnajÌ se znamÈnka
@@1:	mov	rax,[rdx-16]
	cmp	rax,[rcx-16]
	ja	@@gr1
	jb	@@gr2
	test	rax,rax
	jz	cmpu1
	call	cmpu
	ja	@@gr1
	jb	@@gr2
	jmp	@@eq
cmpu1:	call	cmpu
	jb	@@gr1
	ja	@@gr2
@@eq:	xor	rax,rax
	ret
@@gr1:	mov	rax,1
	ret
@@gr2:	mov	rax,-1
	ret
CMPX	endp
;-------------------------------------
;souËet zlomk˘ [rsi],[rbx]
plusfrac	proc
;nalezenÌ spoleËnÈho jmenovatele
	mov	rax,[rsi+8]
	mov	rcx,[rbx+8]
	call	gcd
;lcm(b,d)
	mov	rax,[rsi+8]
	mul	qword ptr [rbx+8]
	cmp	rdx,rcx
	jae	@@fe
	div	rcx
	mov	[rdi+8],rax
;a*lcm(b,d)/b
	mov	rax,[rbx]
	mul	qword ptr [rdi+8]
	mov	rcx,[rbx+8]
	cmp	rdx,rcx
	jae	@@fe
	div	rcx
	mov	[rdi],rax
;c*lcm(b,d)/d
	mov	rax,[rsi]
	mul	qword ptr [rdi+8]
	mov	rcx,[rsi+8]
	cmp	rdx,rcx
	jae	@@fe
	div	rcx
	stc
@@fe:	ret
plusfrac	endp

;bezznamÈnkovÈ plus
PLUSU	proc 	uses rsi rdi rbx a0,a1,a2
local	k0,k1,k2,e0,e1,e2
	mov	[a1],rdx
	mov	[a2],r8
	mov	rbx,rdx
	mov	rdx,rcx
	mov	rcx,r8
;prohoÔ operandy, aby byl prvnÌ vÏtöÌ
	mov	rax,[rbx-8]
	cmp	rax,[rcx-8]
	jge	@@1
	mov	rax,rbx
	mov	rbx,rcx
	mov	rcx,rax
	mov	[a1],rbx
	mov	[a2],rcx
@@1:	mov	rsi,rcx
	mov	rdi,rdx
;test na nulovost 1.operandu
	cmp	qword ptr [rbx-24],0
	jnz	@@0
;okopÌruj 2.operand
	push	qword ptr [rdi-16]
	call	copyx
	pop	qword ptr [rdi-16]
	ret
;test na nulovost 2.operandu
@@0:	cmp	qword ptr [rcx-24],0
	jz	@@cp1a
	cmp	qword ptr [rcx-24],-2
	jnz	@@f1
	cmp	qword ptr [rbx-24],-2
	jnz	@@fe
;(a/b)+(c/d)
	call	plusfrac
	jnc	@@fe
	add	[rdi],rax
	jc	@@fe
	call	fracReduce
	ret
@@f1:	cmp	qword ptr [rbx-24],-2
	jnz	@@f2
;(a/b)+x
	mov	rsi,rbx
	mov	rbx,rcx
;x+(c/d)
@@fe:	call	fractonewx
	push	rax
	sub	rsp,32
	mov	r8,rax
	mov	rdx,rbx
	mov	rcx,rdi
	call	PLUSU
	add	rsp,32
	pop	rcx
	call	FREEX
	ret
;zvÏtöi p¯esnost o jedna
@@f2:	inc	qword ptr [rdx-32]
;nastav promÏnnÈ urËujÌcÌ exponent na konci operand˘
	mov	rax,[rbx-8]
	sub	rax,[rbx-24]
	mov	[k1],rax
	mov	rax,[rcx-8]
	sub	rax,[rcx-24]
	mov	[k2],rax
	mov	rax,[rbx-8]
	mov	rsi,[rdx-32]
	mov	[rdx-8],rax	;exponent v˝sledku
	mov	[rdx-24],rsi	;dÈlka v˝sledku
	sub	rax,rsi		;[k0]
	cmp	rax,[rcx-8]
	jl	@@7
;2.operand je moc mal˝
@@cp1:	dec	qword ptr [rdx-32]
@@cp1a:	mov	rsi,rbx
	mov	rdi,rdx
	push	qword ptr [rdi-16]
	call	copyx
	pop	qword ptr [rdi-16]
	ret
@@7:	mov	[k0],rax
;spoËti adresy konc˘ operand˘
	mov	rax,[rbx-24]
	lea	rax,[8*rax+rbx-8]
	mov	[e1],rax
	mov	rax,[rcx-24]
	lea	rsi,[8*rax+rcx-8]
	mov	[e2],rsi
	mov	rax,[rdx-24]
	lea	rdi,[8*rax+rdx-8]
	mov	[e0],rdi
;o¯Ìzni operandy, kterÈ jsou moc dlouhÈ
	mov	rax,[k0]
	sub	rax,[k1]
	jle	@@2
	add	[k1],rax
	shl	rax,3
	sub	[e1],rax
@@2:	mov	rdi,[k0]
	sub	rdi,[k2]
	jle	@@3
	add	[k2],rdi
	shl	rdi,3
	sub	[e2],rdi
;zmenöi dÈlku v˝sledku, pokud jsou operandy moc kr·tkÈ
@@3:	cmp	rax,rdi
	jge	@@4
	mov	rax,rdi
@@4:	test	rax,rax
	jns	@@5
	add	[rdx-24],rax
	sub	[k0],rax
	shl	rax,3
	add	[e0],rax
;zjisti, kter˝ operand je delöÌ
@@5:	pushf
	std
	mov	rdx,rcx
	mov	rdi,[e0]
	mov	rcx,[k1]
	sub	rcx,[k2]
	jz	@@plus
	jg	@@6
;okopÌruj konec 1.operandu
	neg	rcx
	mov	rsi,[e1]
	rep movsq
	mov	[e1],rsi
	jmp	@@plus
@@6:	mov	rax,[k1]
	sub	rax,[rdx-8]
	mov	rsi,[e2]
	jle	@@cpe
;operandy se nep¯ekr˝vajÌ
	sub	rcx,rax
	rep movsq
;volnÈ mÌsto mezi operandy vyplÚ nulami
	mov	rcx,rax
	xor	rax,rax
	rep stosq
	mov	rsi,[e1]
	jmp	@@cpb
;okopÌruj konec 2.operandu
@@cpe:	rep movsq
	mov	[e2],rsi
;seËti p¯ekr˝vajÌcÌ se Ë·sti operand˘
;rdi, e1,e2
@@plus:	mov	rcx,[a2]
	sub	rcx,[e2]
	sar	rcx,3
	dec	rcx
	mov	rsi,[e1]
	mov	rbx,[e2]
	lea	rdi,[rdi+8*rcx]
	lea	rsi,[rsi+8*rcx]
	lea	rbx,[rbx+8*rcx]
	neg	rcx
	clc
	jz	@@cpb


	lahf
	sub	rcx,3
	jle	@@lpi
@@lp:	sahf
	mov	rax,[rsi+8*rcx+24]
	mov	rdx,[rsi+8*rcx+16]
	adc	rax,[rbx+8*rcx+24]
	mov	r8,[rsi+8*rcx+8]
	adc	rdx,[rbx+8*rcx+16]
	mov	r9,[rsi+8*rcx]
	adc	r8,[rbx+8*rcx+8]
	adc	r9,[rbx+8*rcx]
	mov	[rdi+8*rcx+24],rax
	mov	[rdi+8*rcx+16],rdx
	mov	[rdi+8*rcx+8],r8
	mov	[rdi+8*rcx],r9
	lahf
	sub	rcx,4
	jg	@@lp
@@lpi:	sahf
	inc	rcx
	inc	rcx
	inc	rcx
	jz	@@cpb


@@lp2:	mov	rax,[rsi+8*rcx]
	adc	rax,[rbx+8*rcx]
	mov	[rdi+8*rcx],rax
	dec	rcx
	jnz	@@lp2

;okopÌruj zaË·tek 1.operandu z rsi na rdi
@@cpb:	setc	bl
	push	rdi
	mov	rax,[a1]
	mov	rcx,rsi
	sub	rcx,rax
	sar	rcx,3
	inc	rcx	;rcx= (rsi+8-[a1])/8
	rep movsq
	pop	rsi
	add	rdi,8
;rdi=[a0], rsi=pozice p¯enosu
;zpracuj p¯eteËenÌ
	test	bl,bl
	jz	@@end
	call	incr
@@end:	popf
	dec	qword ptr [rdi-32]
	call	round
	ret
PLUSU	endp
;-------------------------------------
negf	proc
;rdi,rsi ukazujÌ na poslednÌ qword
;rcx je dÈlka
;vracÌ CF
	neg	rcx
	lea	rdi,[rdi+8*rcx]
	lea	rsi,[rsi+8*rcx]
	neg	rcx
	xor	rbx,rbx
@@lpe:	mov	rax,rbx
	sbb	rax,[rsi+8*rcx]
	mov	[rdi+8*rcx],rax
	dec	rcx
	jnz	@@lpe
	ret
negf	endp
;-------------------------------------
;bezznamÈnkovÈ minus
MINUSU	proc 	uses rsi rdi rbx a0,a1,a2
local	k0,k1,k2,e0,e1,e2
	mov	[a0],rcx
	mov	[a1],rdx
	mov	[a2],r8
	mov	rbx,rdx
	mov	rdx,rcx
	mov	rcx,r8
	mov	rdi,rdx
	mov	rsi,rcx
;test na nulovost 2.operandu
@@0:	cmp	qword ptr [rcx-24],0
	jz	@@cp1a
;test na zlomek
	cmp	qword ptr [rcx-24],-2
	jnz	@@f1
	cmp	qword ptr [rbx-24],-2
	jnz	@@fe
;(a/b)-(c/d)
	call	plusfrac
	jnc	@@fe
	sub	[rdi],rax
	call	fracReduce
	ret
@@f1:	cmp	qword ptr [rbx-24],-2
	jnz	@@f2
;(a/b)-x
	push	rsi
	mov	rsi,rbx
	call	fractonewx
	pop	r8
	mov	rsi,rax
	mov	rdx,rax
	jmp	@@fm
;x-(c/d)
@@fe:	call	fractonewx
	mov	rsi,rax
	mov	r8,rax
	mov	rdx,rbx
@@fm:	mov	rcx,rdi
	sub	rsp,32
	call	MINUSU
	add	rsp,32
	mov	rcx,rsi
	call	FREEX
	ret
;zvÏtöi p¯esnost o jedna
@@f2:	inc	qword ptr [rdx-32]
;nastav promÏnnÈ urËujÌcÌ exponent na konci operand˘
	mov	rax,[rbx-8]
	sub	rax,[rbx-24]
	mov	[k1],rax
	mov	rax,[rcx-8]
	sub	rax,[rcx-24]
	mov	[k2],rax
	mov	rax,[rbx-8]
	mov	rsi,[rdx-32]
	mov	[rdx-8],rax	;exponent v˝sledku
	mov	[rdx-24],rsi	;dÈlka v˝sledku
	sub	rax,rsi		;[k0]
	cmp	rax,[rcx-8]
	jl	@@7
;2.operand je moc mal˝
@@cp1:	dec	qword ptr [rdx-32]
@@cp1a:	mov	rsi,rbx
	mov	rdi,rdx
	push	qword ptr [rdi-16]
	call	copyx
	pop	qword ptr [rdi-16]
	ret
@@7:	mov	[k0],rax
;spoËti adresy konc˘ operand˘
	mov	rax,[rbx-24]
	lea	rax,[8*rax+rbx-8]
	mov	[e1],rax
	mov	rax,[rcx-24]
	lea	rsi,[8*rax+rcx-8]
	mov	[e2],rsi
	mov	rax,[rdx-24]
	lea	rdi,[8*rax+rdx-8]
	mov	[e0],rdi
;o¯Ìzni operandy, kterÈ jsou moc dlouhÈ
	mov	rax,[k0]
	sub	rax,[k1]
	jle	@@2
	add	[k1],rax
	shl	rax,3
	sub	[e1],rax
@@2:	mov	rdi,[k0]
	sub	rdi,[k2]
	jle	@@3
	add	[k2],rdi
	shl	rdi,3
	sub	[e2],rdi
;zmenöi dÈlku v˝sledku, pokud jsou operandy moc kr·tkÈ
@@3:	cmp	rax,rdi
	jge	@@4
	mov	rax,rdi
@@4:	test	rax,rax
	jns	@@5
	add	[rdx-24],rax
	sub	[k0],rax
	shl	rax,3
	add	[e0],rax
;zjisti, kter˝ operand je delöÌ
@@5:	pushf
	std
	mov	rdx,rcx
	mov	rdi,[e0]
	mov	rcx,[k1]
	sub	rcx,[k2]
	jz	@@minus
	jg	@@6
;okopÌruj konec 1.operandu
	neg	rcx
	mov	rsi,[e1]
	rep movsq
	mov	[e1],rsi
	clc
	jmp	@@minus
@@6:	mov	rax,[k1]
	sub	rax,[rdx-8]
	mov	rsi,[e2]
	jle	@@nege
;operandy se nep¯ekr˝vajÌ
	sub	rcx,rax
	push	rax
	call	negf
	pop	rcx
;volnÈ mÌsto mezi operandy vyplÚ nulami nebo jedniËkami
	sbb	rax,rax
	rep stosq	;nemÏnÌ CF
	mov	rsi,[e1]
	jmp	@@cpb
;neguj konec 2.operandu
@@nege:	call	negf
	mov	[e2],rsi
;odeËti p¯ekr˝vajÌcÌ se Ë·sti operand˘
;rdi, e1,e2, cf
@@minus:	lahf
	mov	rcx,[a2]
	sub	rcx,[e2]
	sar	rcx,3
	dec	rcx
	mov	rsi,[e1]
	mov	rbx,[e2]
	lea	rdi,[rdi+8*rcx]
	lea	rsi,[rsi+8*rcx]
	lea	rbx,[rbx+8*rcx]
	neg	rcx
	sahf
	inc	rcx
	dec	rcx
	jz	@@cpb

	lahf
	sub	rcx,3
	jle	@@lpi
@@lp:	sahf
	mov	rax,[rsi+8*rcx+24]
	mov	rdx,[rsi+8*rcx+16]
	sbb	rax,[rbx+8*rcx+24]
	mov	r8,[rsi+8*rcx+8]
	sbb	rdx,[rbx+8*rcx+16]
	mov	r9,[rsi+8*rcx]
	sbb	r8,[rbx+8*rcx+8]
	sbb	r9,[rbx+8*rcx]
	mov	[rdi+8*rcx+24],rax
	mov	[rdi+8*rcx+16],rdx
	mov	[rdi+8*rcx+8],r8
	mov	[rdi+8*rcx],r9
	lahf
	sub	rcx,4
	jg	@@lp
@@lpi:	sahf
	inc	rcx
	inc	rcx
	inc	rcx
	jz	@@cpb

@@lp2:	mov	rax,[rsi+8*rcx]
	sbb	rax,[rbx+8*rcx]
	mov	[rdi+8*rcx],rax
	dec	rcx
	jnz	@@lp2

;okopÌruj zaË·tek 1.operandu z rsi na rdi
@@cpb:	setc	bl
	push	rdi
	mov	rax,[a1]
	mov	rcx,rsi
	sub	rcx,rax
	sar	rcx,3
	inc	rcx	;rcx= (rsi+8-[a1])/8
	rep movsq
	pop	rsi
	add	rdi,8
;zpracuj p¯eteËenÌ
	test	bl,bl
	jz	@@end
	call	decr
@@end:	call	norm
	popf
	mov	rdi,[a0]
	dec	qword ptr [rdi-32]
	call	round
	ret
MINUSU	endp
;-------------------------------------
PLUSX	proc 	a0,a1,a2
	sub	rsp,32
	mov	rax,[rdx-16]
	mov	[rcx-16],rax	;znamÈnko prvnÌho operandu
	cmp	rax,[r8-16]
	jz	@@plusu
;r˘zn· znamÈnka
	mov	[a0],rcx
	mov	[a1],rdx
	mov	[a2],r8
	mov	rcx,r8
	call	cmpu
	mov	rcx,[a0]
	jnc	@@minusu
	mov	r8,[a1]
	mov	rdx,[a2]
	mov	rax,[rdx-16]	;znamÈnko druhÈho operandu
	mov	[rcx-16],rax
	jmp	@@1
@@minusu:	mov	r8,[a2]
	mov	rdx,[a1]
@@1:	call	MINUSU
	add	rsp,32
	ret
@@plusu:	call	PLUSU
	add	rsp,32
	ret
PLUSX	endp
;-------------------------------------
MINUSX	proc 	a0,a1,a2
	sub	rsp,32
	mov	rax,[rdx-16]
	mov	[rcx-16],rax	;znamÈnko prvnÌho operandu
	cmp	rax,[r8-16]
	jnz	@@plusu
;stejn· znamÈnka
	mov	[a0],rcx
	mov	[a1],rdx
	mov	[a2],r8
	mov	rcx,r8
	call	cmpu
	mov	rcx,[a0]
	jnc	@@minusu
	mov	r8,[a1]
	mov	rdx,[a2]
	mov	rax,[rdx-16]	;znamÈnko druhÈho operandu
	xor	al,1
	mov	[rcx-16],rax
	jmp	@@1
@@minusu:	mov	r8,[a2]
	mov	rdx,[a1]
@@1:	call	MINUSU
	add	rsp,32
	ret
@@plusu:	call	PLUSU
	add	rsp,32
	ret
MINUSX	endp
;-------------------------------------
;bezznamÈnkovÈ or nebo xor
ORXORU0	proc 	uses rsi rdi rbx a0,a1,a2
local	k0,k1,k2,e0,e1,e2
local	isxor:byte	
	mov	[isxor],al
	mov	[a0],rcx
	mov	[a1],rdx
	mov	[a2],r8
	mov	rbx,rdx
	mov	rdx,rcx
	mov	rcx,r8
;prohoÔ operandy, aby byl prvnÌ vÏtöÌ
	mov	rax,[rbx-8]
	cmp	rax,[rcx-8]
	jge	@@1
	mov	rax,rbx
	mov	rbx,rcx
	mov	rcx,rax
	mov	[a1],rbx
	mov	[a2],rcx
;test na nulovost 1.operandu
@@1:	cmp	qword ptr [rbx-24],0
	jnz	@@0
;okopÌruj 2.operand
	mov	rsi,rcx
	mov	rdi,rdx
	push	qword ptr [rdi-16]
	call	copyx
	pop	qword ptr [rdi-16]
	ret
;test na nulovost 2.operandu
@@0:	cmp	qword ptr [rcx-24],0
	jz	@@cp1
;nastav promÏnnÈ urËujÌcÌ exponent na konci operand˘
	mov	rax,[rbx-8]
	sub	rax,[rbx-24]
	mov	[k1],rax
	mov	rax,[rcx-8]
	sub	rax,[rcx-24]
	mov	[k2],rax
	mov	rax,[rbx-8]
	mov	rsi,[rdx-32]
	mov	[rdx-8],rax	;exponent v˝sledku
	mov	[rdx-24],rsi	;dÈlka v˝sledku
	sub	rax,rsi		;[k0]
	cmp	rax,[rcx-8]
	jl	@@7
;2.operand je moc mal˝
@@cp1:	mov	rsi,rbx
	mov	rdi,rdx
	push	qword ptr [rdi-16]
	call	copyx
	pop	qword ptr [rdi-16]
	ret
@@7:	mov	[k0],rax
;spoËti adresy konc˘ operand˘
	mov	rax,[rbx-24]
	lea	rax,[8*rax+rbx-8]
	mov	[e1],rax
	mov	rax,[rcx-24]
	lea	rsi,[8*rax+rcx-8]
	mov	[e2],rsi
	mov	rax,[rdx-24]
	lea	rdi,[8*rax+rdx-8]
	mov	[e0],rdi
;o¯Ìzni operandy, kterÈ jsou moc dlouhÈ
	mov	rax,[k0]
	sub	rax,[k1]
	jle	@@2
	add	[k1],rax
	shl	rax,3
	sub	[e1],rax
@@2:	mov	rdi,[k0]
	sub	rdi,[k2]
	jle	@@3
	add	[k2],rdi
	shl	rdi,3
	sub	[e2],rdi
;zmenöi dÈlku v˝sledku, pokud jsou operandy moc kr·tkÈ
@@3:	cmp	rax,rdi
	jge	@@4
	mov	rax,rdi
@@4:	test	rax,rax
	jns	@@5
	add	[rdx-24],rax
	sub	[k0],rax
	shl	rax,3
	add	[e0],rax
;zjisti, kter˝ operand je delöÌ
@@5:	pushf
	std
	mov	rdx,rcx
	mov	rdi,[e0]
	mov	rcx,[k1]
	sub	rcx,[k2]
	jz	@@plus
	jg	@@6
;okopÌruj konec 1.operandu
	neg	rcx
	mov	rsi,[e1]
	rep movsq
	mov	[e1],rsi
	jmp	@@plus
@@6:	mov	rax,[k1]
	sub	rax,[rdx-8]
	mov	rsi,[e2]
	jle	@@cpe
;operandy se nep¯ekr˝vajÌ
	sub	rcx,rax
	rep movsq
;volnÈ mÌsto mezi operandy vyplÚ nulami
	mov	rcx,rax
	xor	rax,rax
	rep stosq
	mov	rsi,[e1]
	jmp	@@cpb
;okopÌruj konec 2.operandu
@@cpe:	rep movsq
	mov	[e2],rsi
;zpracuj p¯ekr˝vajÌcÌ se Ë·sti operand˘
;rdi, e1,e2
@@plus:	mov	rcx,[a2]
	sub	rcx,[e2]
	sar	rcx,3
	dec	rcx
	mov	rsi,[e1]
	mov	rbx,[e2]
	lea	rdi,[rdi+8*rcx]
	lea	rsi,[rsi+8*rcx]
	lea	rbx,[rbx+8*rcx]
	neg	rcx
	jz	@@cpb

	cmp	byte ptr [isxor],0
	jnz	@@xor
@@or:	mov	rax,[rsi+8*rcx]
	or	rax,[rbx+8*rcx]
	mov	[rdi+8*rcx],rax
	dec	rcx
	jnz	@@or
	jmp	@@cpb
@@xor:	mov	rax,[rsi+8*rcx]
	xor	rax,[rbx+8*rcx]
	mov	[rdi+8*rcx],rax
	dec	rcx
	jnz	@@xor
;okopÌruj zaË·tek 1.operandu z rsi na rdi
@@cpb:	mov	rax,[a1]
	mov	rcx,rsi
	sub	rcx,rax
	sar	rcx,3
	inc	rcx	;rcx= (rsi+8-[a1])/8
	rep movsq
	popf            ;obnov direction flag
	mov	rdi,[a0]
	call	norm
	ret
ORXORU0	endp
;-------------------------------------
ANDU0	proc 	uses rsi rdi rbx a0,a1,a2
	mov	[a0],rcx
	mov	[a1],rdx
	mov	[a2],r8
	mov	rbx,rdx
	mov	rdx,rcx
	mov	rcx,r8
;zjisti rozsah v˝sledku
	mov	rdi,[rbx-8]
	mov	rsi,[rcx-8]
	mov	rax,rdi
	cmp	rax,rsi
	jl	@@1
	mov	rax,rsi
@@1:	mov	[rdx-8],rax    ;exponent v˝sledku
	mov	rax,[rbx-24]
	test	rax,rax
	jz	@@zero
	sub	rdi,rax
	mov	rax,[rcx-24]
	test	rax,rax
	jz	@@zero
	sub	rsi,rax
	cmp	rdi,rsi
	jl	@@2
	mov	rsi,rdi
;spoËti dÈlku v˝sledku
@@2:	mov	rdi,[rdx-8]
	sub	rdi,rsi
	jg	@@3
;operandy se nep¯ekr˝vajÌ
@@zero:	and	qword ptr [rdx-24],0
	ret
@@3:	mov	rax,[rdx-32]
	cmp	rax,rdi
	jae	@@4
;v˝sledek je kratöÌ neû operandy
	mov	rdi,rax
@@4:	mov	[rdx-24],rdi
;spoËti adresy zaË·tk˘
	mov	rsi,[rdx-8]
	mov	rax,[rbx-8]
	sub	rax,rsi
	lea	rbx,[rbx+8*rax]
	mov	rax,[rcx-8]
	sub	rax,rsi
	lea	rcx,[rcx+8*rax]
	dec	rdi
;hlavnÌ cyklus
@@and:	mov	rax,[rbx+8*rdi]
	and	rax,[rcx+8*rdi]
	mov	[rdx+8*rdi],rax
	dec	rdi
	jns	@@and
;normalizace
	mov	rdi,rdx
	call	norm
	ret
ANDU0	endp
;-------------------------------------
;rax=function
defrac	proc 	a0,a1,a2
	cmp	qword ptr [rdx-24],-2
	jz	@@fr
	cmp	qword ptr [r8-24],-2
	jz	@@fr
	sub	rsp,32	
	call	rax
	add	rsp,32
	ret
@@fr:	push	rsi
	push	rdi
	sub	rsp,32	
	push	rax
	mov	rdi,rcx
	mov	[a1],rdx
	mov	rsi,r8
	call	fractonewx2
	mov	rsi,[a1]
	mov	[a2],rax
	call	fractonewx2
	mov	rsi,rax
	mov	r8,rax
	mov	rdx,[a2]
	mov	rcx,rdi
	pop	rax
	call	rax
	mov	rcx,rsi
	call	FREEX
	mov	rcx,[a2]
	call	FREEX
	add	rsp,32
	pop	rdi
	pop	rsi
	ret
defrac	endp

ORU0:	mov	al,0
	jmp	ORXORU0
XORU0:	mov	al,1
	jmp	ORXORU0
ORU@12:
ORU:	lea	rax,[ORU0]
	jmp	defrac
XORU@12:
XORU:	lea	rax,[XORU0]
	jmp	defrac
ANDU@12:
ANDU:	lea	rax,[ANDU0]
	jmp	defrac
;-------------------------------------
NEGX	proc
	xor	byte ptr [rcx-16],1
	ret
NEGX	endp

ABSX	proc
	and	byte ptr [rcx-16],0
	ret
ABSX	endp

SIGNX	proc
	cmp	qword ptr [rcx-24],0
	jz	@@ret
	mov	qword ptr [rcx-24],-2
	mov	qword ptr [rcx-8],1
	mov	qword ptr [rcx],1
	mov	qword ptr [rcx+8],1
@@ret:	ret
SIGNX	endp

SCALEX	proc
	test	rdx,rdx
	jz	@@ret
	push	rdx
	push	rcx
	call	FRACTOX
	pop	rcx
	pop	rdx
	add	qword ptr [rcx-8],rdx
	jo	overflow
@@ret:	ret
SCALEX	endp
;-------------------------------------
;o¯ÌznutÌ desetinnÈ Ë·sti
TRUNCX	proc
	mov	rdx,[rcx-8]
	test	rdx,rdx
	js	ZEROX
	cmp	rdx,[rcx-24]
	jae	@@ret
	cmp	qword ptr [rcx-24],-2
	jz	truncf
	mov	[rcx-24],rdx
@@ret:	ret
TRUNCX	endp

;zaokrouhlenÌ zlomku, vracÌ edx=zbytek, nemÏnÌ rcx
truncf:	mov	rax,[rcx]
	xor	rdx,rdx
	div	qword ptr [rcx+8]
	mov	[rcx],rax
	mov	qword ptr [rcx+8],1
	mov	qword ptr [rcx-8],1
	test	rax,rax
	jnz	@f
	mov	qword ptr [rcx-24],rax
@@:	ret

;o¯ÌznutÌ celÈ Ë·sti
FRACX	proc
	cmp	qword ptr [rcx-24],-2
	jnz	@@1
;zlomek
	mov	rax,[rcx]
	xor	rdx,rdx
	div	qword ptr [rcx+8]
	test	rdx,rdx
	jz	ZEROX
	mov	[rcx],rdx
	jmp	fractox
@@1:	mov	rdx,[rcx-8]
	test	rdx,rdx
	js	@@ret
	jz	@@ret
	mov	rax,[rcx-24]
	sub	rax,rdx
	jbe	ZEROX
	mov	[rcx-24],rax
	sub	[rcx-8],rdx  ; =0
	push	rdi
	push	rsi
	mov	rdi,rcx
	lea	rsi,[rcx+8*rdx]
	xchg	rcx,rax
	rep movsq
	mov	rdi,rax
	call	norm
	pop	rsi
	pop	rdi
@@ret:	ret
FRACX	endp
;-------------------------------------
;zaokrouhlenÌ na celÈ ËÌslo
ROUNDX	proc
	mov	rdx,[rcx-8]
	test	rdx,rdx
	js	ZEROX  ;z·porn˝ exponent -> v˝sledek nula
	cmp	rdx,[rcx-24]
	jl	@@1
;integer or fraction
	cmp	qword ptr [rcx-24],-2
	jnz	@@ret
	push	qword ptr [rcx+8]
	call	truncf
	pop	rax
	shl	rdx,1   ;remainder
	jc	@@i
	cmp	rax,rdx
	ja	@@ret
@@i:	inc	qword ptr [rcx]
	cmp	qword ptr [rcx-24],0
	jnz	@@ret
	mov	qword ptr [rcx-24],-2
@@ret:	ret
@@1:	push	rsi
	push	rdi
	mov	[rcx-24],rdx   ;zkraù dÈlku
	mov	rdi,rcx
	mov	rax,[rdi+8*rdx]
	test	rax,rax
	jns	@@e
;zaokrouhli
	lea	rsi,[rdi+8*rdx-8]
	call	incr
@@e:	pop	rdi
	pop	rsi
	ret
ROUNDX	endp
;-------------------------------------
MINUS1	proc
	mov	rdx,1
	mov	[rcx+8],rdx
	mov	[rcx],rdx
	mov	[rcx-8],rdx
	mov	[rcx-16],rdx
	mov	qword ptr [rcx-24],-2
	ret
MINUS1	endp
;-------------------------------------
;zaokrouhlenÌ smÏrem dol˘
INTX	proc
	cmp	byte ptr [rcx-16],0
	jz	TRUNCX
;operand je z·porn˝
	cmp	qword ptr [rcx-24],0
	jg	@@1
	jz	@@ret
;fraction
	call	truncf
	test	rdx,rdx
	jz	@@ret
	inc	qword ptr [rcx]
	cmp	qword ptr [rcx-24],0
	jnz	@@ret
	mov	qword ptr [rcx-24],-2
@@ret:	ret
@@1:	mov	rdx,[rcx-8]
	test	rdx,rdx
	js	MINUS1
	jz	MINUS1
	push	rdi
	push	rsi
	mov	rdi,rcx
	call	trim
	mov	rdx,[rdi-8]
	cmp	rdx,[rdi-24]
	jge	@@e
	mov	[rdi-24],rdx
	lea	rsi,[rdi+8*rdx-8]
	call	incr
@@e:	pop	rsi
	pop	rdi
	ret
INTX	endp

;zaokrouhlenÌ smÏrem nahoru
CEILX	proc
	xor	byte ptr [rcx-16],1
	push	rcx
	call	INTX
	pop	rcx
	xor	byte ptr [rcx-16],1
	ret
CEILX	endp
;-------------------------------------
;[rdi]*= rsi
mult1	proc
	cmp	rsi,1
	ja	@@z
	jz	@@ret
	and	qword ptr [rdi-24],0
	ret
@@z:	cmp	qword ptr [rdi-24],-2
	jnz	@@0
;zlomek
	lea	rdi,[rdi+8]
	push	rsi
	mov	rsi,rsp
	call	reduce
	pop	rsi
	lea	rdi,[rdi-8]
	mov	rax,[rdi]
	mul	rsi
	jo	@@f1
	mov	[rdi],rax
	call	fracexp
	jmp	@@ret
;convert fraction to real
@@f1:	mov	rcx,rdi
	call	FRACTOX
@@0:	xor	rbx,rbx   ;p¯enos
	mov	rcx,[rdi-24]
	dec	rcx
	js	@@ret
@@lp:	mov	rax,[rdi+8*rcx]
	mul	rsi
	add	rax,rbx
	mov	rbx,rdx
	mov	[rdi+8*rcx+8],rax
	adc	rbx,0
	dec	rcx
	jns	@@lp
	mov	[rdi],rbx
	test	rbx,rbx
	jnz	@@1
;zruö nulov˝ p¯enos
	mov	rcx,[rdi-24]
	lea	rsi,[rdi+8]
	cld
	rep movsq
@@ret:	ret
@@1:	inc	qword ptr [rdi-8]
	jno	roundi
	jmp	overflow
mult1	endp

;a0*=ai
MULTI1	proc 	uses rsi rdi rbx a0,ai
	mov	rdi,rcx
	mov	rsi,rdx
	call	mult1
	ret
MULTI1	endp
;-------------------------------------
;a0:=a1*ai
;a0 m˘ûe b˝t rovno a1
MULTI	proc 	uses rsi rdi rbx a0,a1,ai
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rcx,[rsi-24]
	cmp	rcx,-2
	jnz	@@0
;zlomek
	mov	rbx,r8
	call	copyx
	mov	rsi,rbx
	call	mult1
	ret
;okopÌruj znamÈnko a exponent
@@0:	mov	rax,[rsi-8]
	mov	[rdi-8],rax
	mov	rdx,[rsi-16]
	mov	[rdi-16],rdx
;nastav dÈlku v˝sledku
	mov	rax,[rdi-32]
	cmp	rcx,rax
	jbe	@@1
	mov	rcx,rax
@@1:	mov	[rdi-24],rcx
	add	rdi,8
	dec	rcx
	js	@@ret    ;operand je 0
	xor	rbx,rbx  ;p¯enos


	lahf
	sub	rcx,3
	js	@@lpi
;hlavnÌ cyklus
@@lp:	sahf
	mov	rax,[rsi+8*rcx+24]
	mul	r8
	add	rax,rbx
	mov	[rdi+8*rcx+24],rax
	mov	r9,rdx
	adc	r9,0
	mov	rax,[rsi+8*rcx+16]
	mul	r8
	add	rax,r9
	mov	[rdi+8*rcx+16],rax
	mov	rbx,rdx
	adc	rbx,0
	mov	rax,[rsi+8*rcx+8]
	mul	r8
	add	rax,rbx
	mov	[rdi+8*rcx+8],rax
	mov	r9,rdx
	adc	r9,0
	mov	rax,[rsi+8*rcx]
	mul	r8
	add	rax,r9
	mov	[rdi+8*rcx],rax
	mov	rbx,rdx
	adc	rbx,0
	lahf
	sub	rcx,4
	jns	@@lp
@@lpi:	sahf
	inc	rcx
	inc	rcx
	inc	rcx
	js	@@cy

@@lp2:	mov	rax,[rsi+8*rcx]
	mul	r8
	add	rax,rbx
	mov	[rdi+8*rcx],rax
	mov	rbx,rdx
	adc	rbx,0
	dec	rcx
	jns	@@lp2
;zapiö p¯eteËenÌ
@@cy:	sub	rdi,8
	mov	[rdi],rbx
	test	rbx,rbx
	jnz	@@2
;zruö nulov˝ p¯enos
	mov	rcx,[rdi-24]
	lea	rsi,[rdi+8]
	cld
	rep movsq
	ret
@@2:	inc	qword ptr [rdi-8]
	jo	overflow
	call	roundi
@@ret:	ret
MULTI	endp

MULTIN	proc 	a0,a1,ai
	test	r8,r8
	jns	@f
	neg	r8
	mov	[a0],rcx
	call	MULTI
	mov	rcx,[a0]
	xor	qword ptr [rcx-16],1
	ret
@@:	call	MULTI
	ret
MULTIN	endp
;-------------------------------------
DIVX	proc 	uses rsi rdi rbx a0,a1,a2
local	d1,d2,d,t1,t2	
	xor	rsi,rsi
	mov	[a0],rcx
	mov	[a1],rdx
	mov	[a2],r8
	mov	rbx,rdx	;dÏlenec
	mov	rdx,rcx	;v˝sledek
	mov	rcx,r8	;dÏlitel
	cmp	qword ptr [rbx-24],-2
	jnz	@@f1
	cmp	qword ptr [rcx-24],-2
	jnz	@@f0
;(a/b)/(c/d)
	mov	rax,[rcx-16]
	xor	rax,[rbx-16]
	mov	[rdx-16],rax
	push	qword ptr [rbx]   ;a
	push	qword ptr [rcx+8] ;d
	push	qword ptr [rbx+8] ;b
	push	qword ptr [rcx]   ;c
	lea	rsi,[rsp]     ;c
	lea	rdi,[rsp+24]  ;a
	call	reduce
	lea	rsi,[rsp+8]   ;b
	lea	rdi,[rsp+16]   ;d
	call	reduce
	mov	rdi,[a0]
	pop	rax
	pop	rcx
	mul	rcx	;b*c
	jo	@@fe1
	mov	[rdi+8],rax
	pop	rax
	pop	rcx
	mul	rcx	;a*d
	jo	@@fe2
	mov	[rdi],rax
	call	fracexp
	ret
;zlomek p¯etekl
@@fe1:	add	rsp,16
@@fe2:	mov	rbx,[a1]
	mov	rcx,[a2]
	mov	rdx,rdi
	jmp	@@f3
;(a/b)/x
@@f0:	sub	rsp,32+HED+16
;a uloû na z·sobnÌk
	lea	rax,[rsp+32+HED]
	mov	qword ptr [rax-32],1
	mov	qword ptr [rax-24],1
	mov	qword ptr [rax-8],1
	mov	rdx,[rbx-16]
	mov	[rax-16],rdx
	mov	rdx,[rbx]
	mov	[rax],rdx
;a/x
	mov	r8,rcx
	mov	rdx,rax
	mov	rcx,[a0]
	call	DIVX
;(a/x)/b
	mov	r8,qword ptr [rbx+8]
	mov	rdx,[a0]
	mov	rcx,rdx
	call	DIVI
	add	rsp,32+HED+16
	ret
@@f1:	cmp	qword ptr [rcx-24],-2
	jnz	@@f2
;x/(a/b)
@@f3:	push	qword ptr [rcx+8]
	push	qword ptr [rcx]
;p¯ekopÌruj dÏlenec do v˝sledku
	mov	rdi,rdx
	mov	rsi,rbx
	call	copyx
;znamÈnko
	mov	rcx,[a2]
	mov	rax,[rcx-16]
	xor	[rdi-16],rax
	pop	r8
	sub	rsp,32
	mov	rdx,rdi
	mov	rcx,rdi
	call	DIVI	;x/a
	add	rsp,32
	pop	rsi
	call	mult1	;(x/a)*b
	ret
;operandy nejsou zlomky
@@f2:	cmp	qword ptr [rcx-24],0
	jnz	@@tz
;division by zero
	lea	rdx,[E_1010]
	mov	rcx,1010
	sub	rsp,32
	call	cerror
	add	rsp,32
	jmp	@@ret
@@tz:	cmp	[rbx-24],rsi
	mov	[rdx-24],rsi
	jz	@@ret		; dÏlÏnec je nula
	cmp	qword ptr [rcx-24],1
	ja	@@1
;jednocifern˝ dÏlitel
	sub	rsp,32
	mov	r8,qword ptr [rcx]
	mov	rcx,rdx
	mov	rdx,rbx
	call	DIVI
	add	rsp,32
	mov	rcx,[a2]
	mov	rdx,[a0]
	mov	rax,[rcx-16]	;znamÈnko dÏlitele
	xor	[rdx-16],rax
	mov	rax,[rcx-8]	;exponent dÏlitele
	dec	rax
	sub	[rdx-8],rax
	jo	overflow
	ret
;znamÈnko
@@1:	mov	rax,[rcx-16]
	push	rax
	xor	rax,[rbx-16]
	mov	[rdx-16],rax
	mov	[rcx-16],rsi	;kladn˝ dÏlitel
;exponent
	mov	rdi,[rcx-8]
	mov	rax,[rbx-8]
	push	rdi
	inc	rax
	sub	rax,rdi
	mov	[rcx-8],rsi	;dÏlitel bez exponentu
	mov	[rdx-8],rax
	jno	@@2
	call	overflow
	jmp	@@r
@@2:	mov	rax,[rbx-24]
	mov	rdi,[rcx-24]
	cmp	rax,rdi
	jbe	@@6
	mov	rdi,rax
@@6:	add	rdi,2
;alokuj pomocnou promÏnnou t2, o 1 delöÌ neû dÏlitel
	mov	rcx,[rcx-24]
	inc	rcx
	call	ALLOCX
	mov	[t2],rax
;alokuj pomocnou promÏnnou t1, o 2 delöÌ neû dÏlenec nebo dÏlitel
	mov	rcx,rdi
	call	ALLOCX
	mov	[t1],rax
;alokuj zbytek
	mov	rcx,rdi
	call	ALLOCX
;rdi:=zbytek, rsi:=dÏlÏnec, rbx:=dÏlitel
	mov	rsi,rbx
	mov	rdi,rax
	mov	rbx,[a2]
;vyn·sob dÏlenec i dÏlitel, aby dÏlitel zaËÌnal velkou cifrou
	xor	rax,rax
	mov	rdx,1
	mov	rcx,[rbx] ;prvnÌ cifra dÏlitele
	inc	rcx
	jz	@@c
	div	rcx
	cmp	rax,1
	jz	@@c
	push	rax
	mov	rcx,[rbx-24]
	call	ALLOCX
	mov	[d],rax
	pop	rcx
	push	rcx
	push	rbx
	mov	rbx,rax
	mov	r8,rcx
	mov	rdx,rsi
	mov	rcx,rdi
	sub	rsp,32
	call	MULTI
	add	rsp,32
	mov	rcx,rbx
	pop	rdx
	pop	r8
	sub	rsp,32
	call	MULTI
	add	rsp,32
	jmp	@@12
;p¯ekopÌruj do zbytku dÏlenec
@@c:	call	copyx
	mov	[d],0
;exponent zbytku
@@12:	mov	rax,[rdi-8]
	sub	rax,[rsi-8]
	mov	[rdi-8],rax
	mov	qword ptr [rdi-16],0
;rdi:=v˝sledek, rsi:=zbytek
	mov	rsi,rdi
	mov	rdi,[a0]
;porovn·nÌ dÏlence a dÏlitele
	mov	rcx,rsi
	mov	rdx,rbx
	call	cmpu
	jna	@@3
	inc	qword ptr [rsi-8]
	dec	qword ptr [rdi-8]
;nastav dÈlku v˝sledku
@@3:	mov	rcx,[rdi-32]
	mov	[rdi-24],rcx
	push	rcx	;poËÌtadlo for cyklu
;[d1]:= prvnÌ cifra dÏlitele
	mov	rax,[rbx]
	mov	[d1],rax
;[d2]:= druh· cifra dÏlitele
	mov	rax,[rbx+8]
	mov	[d2],rax
;hlavnÌ cyklus, rsi=zbytek, rbx=dÏlitel
;rdi=pr·vÏ poËÌtan· cifra v˝sledku
@@lp:	xor	rdx,rdx
	cmp	[rsi-24],rdx
	jz	@@e	;zbytek je 0
;naËti nejvyööÌ ¯·d dÏlence do rdx:rax
	mov	rax,[rsi]
	cmp	[rsi-8],rdx
	jz	@@4
	mov	[rdi],rdx ;0
	jl	@@w	;zbytek < dÏlitel
	mov	rdx,rax
	xor	rax,rax
	cmp	qword ptr [rsi-24],1
	jz	@@4	;jednocifern˝ zbytek
	mov	rax,[rsi+8]
@@4:	cmp	rdx,[d1]
	jb	@@9
;v˝sledek dÏlenÌ je vÏtöÌ neû 64 bit˘
	mov	rcx,rax
	stc
	sbb	rax,rax	;0xFFFFFFFFFFFFFFFF
	add	rcx,[d1]
	jc	@@8
	jmp	@@10
;odhadni v˝slednou cifru
@@9:	div	[d1]
	mov	rcx,rdx	;zbytek dÏlenÌ
@@10:	push	rax	;uchovej v˝slednou cifru
	mul	[d2]
	sub	rdx,rcx
	jc	@@11
	mov	rcx,[rsi-8]
	inc	rcx
	cmp	qword ptr [rsi-24],rcx
	jbe	@@f	;jednocifern˝ nebo dvoucifern˝ zbytek
	sub	rax,[rsi+8*rcx]
	sbb	rdx,0
	jc	@@11
;dokud je zbytek z·porn˝, sniûuj v˝slednou cifru
@@f:	mov	rcx,rax
	or	rcx,rdx
	jz	@@11
	dec	qword ptr [rsp]
	sub	rax,[d2]
	sbb	rdx,[d1]
	jnc	@@f
@@11:	pop	rax
;od zbytku odeËti dÏlitel * rax
@@8:	mov	[rdi],rax
	sub	rsp,32
	mov	r8,rax
	mov	rdx,rbx
	mov	rcx,[t2]
	call	MULTI
	mov	r8,[t2]
	mov	rdx,rsi
	mov	rcx,[t1]
	call	MINUSX
	add	rsp,32
	mov	rcx,rsi
	mov	rsi,[t1]
	mov	[t1],rcx
;oprav v˝sledek, aby zbytek nebyl z·porn˝
	cmp	byte ptr [rsi-16],0
	jz	@@w	;zbytek je kladn˝
	cmp	qword ptr [rsi-24],0
	jz	@@w	;zbytek je nulov˝
	sub	rsp,32
@@d:	dec	qword ptr [rdi]
	mov	r8,rbx
	mov	rdx,rsi
	mov	rcx,[t1]
	call	PLUSX
	mov	rcx,rsi
	mov	rsi,[t1]
	mov	[t1],rcx
;pro jistotu jeötÏ zkontroluj znamÈnko zbytku
	cmp	qword ptr [rsi-24],0
	jz	@f	;zbytek je nulov˝
	cmp	byte ptr [rsi-16],0
	jnz	@@d	;tento skok se asi nikdy neprovede
@@:	add	rsp,32
;posun na dalöÌ cifru v˝sledku
@@w:	add	rdi,8
	inc	qword ptr [rsi-8]
	pop	rcx
	dec	rcx
	push	rcx
	jnz	@@lp
;oprav dÈlku v˝sledku (p¯i nulovÈm zbytku)
@@e:	pop	rcx
	mov	rax,[a0]
	sub	[rax-24],rcx
;smaû pomocnÈ promÏnnÈ
	mov	rcx,rsi
	call	FREEX
	mov	rcx,[t1]
	call	FREEX
	mov	rcx,[t2]
	call	FREEX
	mov	rcx,[d]
	call	FREEX
;obnov znamÈnko a exponent dÏlitele
@@r:	mov	rbx,[a2]
	pop	qword ptr [rbx-8]
	pop	qword ptr [rbx-16]
@@ret:	ret
DIVX	endp
;-------------------------------------
MULTX1	proc 	uses rsi rdi rbx a0,a1,a2
local	e2
	mov	[a0],rcx
	mov	[a1],rdx
	mov	[a2],r8
	mov	rbx,rdx
	mov	rdx,rcx
	mov	rcx,r8
	xor	rsi,rsi
	mov	[rdx-24],rsi
	cmp	[rbx-24],rsi
	jz	@@ret
	cmp	[rcx-24],rsi
	jz	@@ret
	cmp	qword ptr [rbx-24],-2
	jnz	@@f1
;fraction
@@f0:	sub	rsp,32+HED+16
	lea	rax,[rsp+32+HED]
	mov	qword ptr [rax-32],2
	mov	qword ptr [rax-24],-2
	mov	rdi,[rbx-16]
	mov	[rax-16],rdi
	mov	rdi,[rbx]
	mov	[rax+8],rdi
	mov	rdi,[rbx+8]
	mov	[rax],rdi
	mov	r8,rax
	mov	rax,rdx
	mov	rdx,rcx
	mov	rcx,rax
	call	DIVX
	add	rsp,32+HED+16
	ret
@@f1:	cmp	qword ptr [rcx-24],-2
	jnz	@@f2
	mov	rax,rcx
	mov	rcx,rbx
	mov	rbx,rax
	jmp	@@f0
;spoËti znamÈnko
@@f2:	mov	rax,[rbx-16]
	xor	rax,[rcx-16]
	mov	[rdx-16],rax
;spoËti v˝sledn˝ exponent
	mov	rax,[rbx-8]
	dec	rax
	add	rax,[rcx-8]
	jno	@@1
	call	overflow
	jmp	@@ret
@@1:	mov	[rdx-8],rax	;exponent
;zjisti dÈlku v˝sledku
	mov	rax,[rdx-32]
	add	rax,10          ;zvÏtöi dÈlku kv˘li p¯esnosti
	mov	[rdx-32],rax
	mov	[rdx-24],rax
	mov	rsi,[rbx-24]
	add	rsi,[rcx-24]
	dec	rsi
	cmp	rsi,rax
	jnb	@@2
;v˝sledek nem˘ûe b˝t delöÌ neû souËet dÈlek operand˘
	mov	[rdx-24],rsi
@@2:	mov	rax,[rcx-24]
	lea	rax,[rcx+8*rax]
	mov	[e2],rax
;vynuluj v˝sledek
	mov	rcx,[rdx-24]
	mov	rdi,rdx
	xor	rax,rax
	cld
	rep stosq
;rsi:= poË·teËnÌ pozice v 1.operandu
	mov	rcx,[a2]
	mov	rsi,[rdx-24]
	mov	rax,[rbx-24]
	lea	rdi,[rdx+8*rsi-8]
	cmp	rax,rsi
	lea	rsi,[rbx+8*rsi-8]
	jnc	@@7
	lea	rsi,[rbx+8*rax-8]
;rbx:= poË·teËnÌ pozice v 2.operandu
@@7:	mov	rax,[rdx-24]
	sub	rax,[rbx-24]
	lea	rbx,[rcx+8*rax]
	cmp	rbx,rcx
	jnc	@@5
	mov	rbx,rcx
;vnÏjöÌ cyklus p¯es 1.operand
@@5:	push	rbx
	push	rsi
	push	rdi
	xor	rcx,rcx		;p¯enos
;vnit¯nÌ cyklus p¯es 2.operand
	mov	r8,[rsi]
	mov	r9,[a2]
@@lp:	mov	rax,[rbx]
	mul	r8
	add	rax,rcx
	mov	rcx,rdx
	adc	rcx,0
	add	[rdi],rax
	adc	rcx,0
	cmp	rbx,r9
	jbe	@@8
	
	mov	rax,[rbx-8]
	mul	r8
	add	rax,rcx
	mov	rcx,rdx
	adc	rcx,0
	add	[rdi-8],rax
	adc	rcx,0
	sub	rbx,16
	sub	rdi,16
	cmp	rbx,r9
	jnc	@@lp
@@lpe:

	cmp	rdi,[a0]
	jc	@@e
;zapiö p¯enos
	add	[rdi],rcx
	pop	rdi
	pop	rsi
	pop	rbx
;posuÚ se na dalöÌ ¯·d
	sub	rsi,8
	add	rbx,8
	cmp	rbx,[e2]
	jnz	@@5
@@6:	sub	rdi,8
	sub	rbx,8
	jmp	@@5
@@8:	sub	rbx,8
	sub	rdi,8
	jmp	@@lpe
;konec
@@e:	pop	rdi
	pop	rsi
	pop	rbx
	mov	rdi,[a0]
	mov	rax,rcx
	push	rdi
	call	carry
	pop	rdi
	sub	qword ptr [rdi-32],10
	call	round
@@ret:	ret
MULTX1	endp
;-------------------------------------
;rekurzivnÌ algoritmus
; x = x0 + x1*B
; y = y0 + y1*B
; xy = (B^2 + B)x1*y1 + B(x1-x0)*(y0-y1) + (B + 1)x0*y0
; sloûitost n^(ln3/ln2) = n^1.58496
MULTX	proc 	uses rsi rdi rbx z,x,y
local	x0,y0,t1,t2,t3,n1,n2,n01,n02,xsgn,ysgn
MULLIM	equ	50
;zkontroluj, jestli x,y nejsou moc kr·tkÈ
	mov	[z],rcx
	mov	[x],rdx
	mov	[y],r8
	mov	rcx,[rdx-24]
	cmp	rcx,MULLIM
	mov	[n01],rcx
	jl	@@1
	mov	rdx,[r8-24]
	cmp	rdx,MULLIM
	mov	[n02],rdx
	jl	@@1
;zkraù x,y, pokud jsou delöÌ neû v˝sledek
	mov	rax,[z]
	mov	rsi,[rax-32]
	inc	rsi
	cmp	rsi,rcx
	jae	@@5
	mov	rcx,rsi
@@5:	mov	[n1],rcx
	cmp	rsi,rdx
	jae	@@6
	mov	rdx,rsi
@@6:	mov	[n2],rdx
	mov	rbx,rdx
	add	rbx,rcx
;rcx=n1, rdx=n2, rbx=n1+n2, rsi=[z-32]+1
;zkontroluj, jestli jsou x,y zhruba stejnÏ dlouhÈ
	shl	rcx,1
	lea	rax,[rdx+rdx*2]
	cmp	rcx,rax
	jg	@@1
	shl	rdx,1
	mov	rcx,[n1]
	lea	rcx,[rcx+rcx*2]
	cmp	rdx,rcx
	jle	@@2
;norm·lnÌ nerekurzivnÌ n·sobenÌ
@@1:	sub	rsp,32
	mov	r8,[y]
	mov	rdx,[x]
	mov	rcx,[z]
	call	MULTX1
	add	rsp,32
	ret
@@2:	cmp	rsi,rbx
	jb	@@a
	mov	rsi,rbx
;alokuj pomocnÈ promÏnnÈ
@@a:	
	lea	rax,[t3]
	push	rax
	lea	rax,[t2]
	push	rax
	lea	rax,[t1]
	push	rax
	lea	r9,[y0]
	lea	r8,[x0]
	mov	rdx,rsi
	mov	rcx,5
	sub	rsp,32
	call	ALLOCNX
	add	rsp,56
;inicializuj x,y,x0,y0
	inc	rbx
	shr	rbx,2
	mov	rsi,[x]
	mov	rdi,[y]
;uchovej star· znamÈnka a nov· dej kladn·
	mov	rax,[rsi-16]
	mov	[xsgn],rax
	mov	rax,[rdi-16]
	mov	[ysgn],rax
	and	qword ptr [rdi-16],0
	and	qword ptr [rsi-16],0
;dÈlky
	mov	rax,[y0]
	mov	[rax-24],rbx
	mov	rax,[x0]
	mov	[rax-24],rbx
	mov	rax,[n1]
	sub	rax,rbx
	mov	[rsi-24],rax
	mov	rax,[n2]
	sub	rax,rbx
	mov	[rdi-24],rax
;exponenty
;x0[-1]:=x[-1]-n1+b;  y0[-1]:=y[-1]-n2+b
	mov	rax,[rsi-8]
	sub	rax,[n1]
	add	rax,rbx
	mov	rcx,[x0]
	mov	[rcx-8],rax
	mov	rax,[rdi-8]
	sub	rax,[n2]
	add	rax,rbx
	mov	rcx,[y0]
	mov	[rcx-8],rax
;x1[-1]:=x[-1]-b;  y1[-1]:=y[-1]-b;
	sub	[rsi-8],rbx
	cmp	rsi,rdi
	jz	@@3
	sub	[rdi-8],rbx
;do x0 okopÌruj spodnÌ polovinu x
@@3:	mov	rax,[n1]
	lea	rsi,[rsi+rax*8]
	mov	rcx,rbx
	shl	rcx,3
	sub	rsi,rcx
	mov	rdi,[x0]
	mov	rcx,rbx
	cld
	rep movsq
;normalizace
	mov	rdi,[x0]
	call	norm
;do y0 okopÌruj spodnÌ polovinu y
	mov	rdx,[y]
	mov	rax,[n2]
	lea	rsi,[rdx+rax*8]
	mov	rcx,rbx
	shl	rcx,3
	sub	rsi,rcx
	mov	rdi,[y0]
	mov	rcx,rbx
	rep movsq
;normalizace
	mov	rdi,[y0]
	call	norm
;rekurze a seËtenÌ dÌlËÌch v˝sledk˘
	mov	rsi,[x]
	mov	rdi,[y]
;t3:=x1-x0
	sub	rsp,32
	mov	r8,[x0]
	mov	rdx,rsi
	mov	rcx,[t3]
	call	MINUSX
;t1:=y0-y1
	mov	r8,rdi
	mov	rdx,[y0]
	mov	rcx,[t1]
	call	MINUSX
;t2:=(x1-x0)*(y0-y1)
	mov	r8,[t1]
	mov	rdx,[t3]
	mov	rcx,[t2]
	call	MULTX
;t3:=x0*y0
	mov	r8,[y0]
	mov	rdx,[x0]
	mov	rcx,[t3]
	call	MULTX
;t1:= B*(x1-x0)*(y0-y1) + x0*y0
	mov	rdx,[t2]
	mov	r8,[t3]
	add	[rdx-8],rbx
	mov	rcx,[t1]
	call	PLUSX
;t2:= B*(x1-x0)*(y0-y1) + (B+1)*x0*y0
	mov	r8,[t3]
	add	[r8-8],rbx
	mov	rdx,[t1]
	mov	rcx,[t2]
	call	PLUSX
;t3:=x1*y1
	mov	r8,rdi
	mov	rdx,rsi
	mov	rcx,[t3]
	call	MULTX
;t1:= B*x1*y1 + B*(x1-x0)*(y0-y1) + (B+1)*x0*y0
	mov	r8,[t3]
	add	[r8-8],rbx
	mov	rdx,[t2]
	mov	rcx,[t1]
	call	PLUSX
;z:= (B^2+B)*x1*y1 + B*(x1-x0)*(y0-y1) + (B+1)*x0*y0
	mov	r8,[t3]
	add	[r8-8],rbx
	mov	rdx,[t1]
	mov	rcx,[z]
	call	PLUSX
	add	rsp,32
;znamÈnko
	mov	rax,[xsgn]
	mov	[rsi-16],rax
	mov	rdx,[ysgn]
	mov	[rdi-16],rdx
	xor	rax,rdx
	mov	rcx,[z]
	mov	[rcx-16],rax
;obnov parametry x,y
	mov	rcx,[n01]
	mov	[rsi-24],rcx
	mov	rdx,[n02]
	mov	[rdi-24],rdx
	add	[rsi-8],rbx
	cmp	rsi,rdi
	jz	@@f
	add	[rdi-8],rbx
;uvolni pomocnÈ promÏnnÈ
@@f:	mov	rcx,[x0]
	call	FREEX
	ret
MULTX	endp
;-------------------------------------
;vracÌ zbytek dÏlenÌ 64-bitov˝m ËÌslem
;nem· smysl pro re·ln˝ operand
MODI	proc 	uses rsi rbx a1,ai
	mov	r8,rcx
	mov	rsi,rcx
	mov	rbx,rdx
	test	rbx,rbx
	jnz	@@0
	lea	rdx,[E_1010]
	mov	rcx,1010
	sub	rsp,32
	call	cerror
	add	rsp,32
	ret
@@0:	mov	rcx,[rsi-24]
	xor	rdx,rdx
	test	rcx,rcx
	jns	@@1
;32bit
	mov	rax,[rsi]
	div	rbx
	jmp	@@r
@@1:	jz	@@r     ; 0 mod ai=0
;hlavnÌ cyklus
@@lp:	mov	rax,[rsi]
	add	rsi,8
	div	rbx
	dec	rcx
	jnz	@@lp
	test	rdx,rdx
	jz	@@r   ;nulov˝ zbytek
	mov	rcx,[r8-8]
	sub	rcx,[r8-24]
	jle	@@r
;za vstupnÌ operand doplÚuj nuly a pokraËuj v dÏlenÌ
@@lp2:	xor	rax,rax
	div	rbx
	dec	rcx
	jnz	@@lp2
@@r:	mov	rax,rdx
	ret
MODI	endp
;-------------------------------------
;vypÌöe ËÌslo do bufferu
;neumÌ zobrazit exponent -> Ë·rka musÌ b˝t uvnit¯ mantisy !!!
WRITEX1	proc 	uses rsi rdi rbx buf,a1
	mov	[buf],rcx
	mov	[a1],rdx
	mov	rsi,rdx
	mov	rax,[rdx-24]
	mov	rdx,rcx
	test	rax,rax
	jnz	@@1
;nula
	mov	byte ptr [rdx],48
	inc	rdx
	mov	byte ptr [rdx],0
	ret
;znamÈnko
@@1:	cmp	qword ptr [rsi-16],0
	jz	@@5
	mov	byte ptr [rdx],45  ;'-'
	inc	rdx
	mov	[buf],rdx
@@5:	mov	byte ptr [rdx],0
;okopÌruj operand
	mov	rcx,[a1]
	call	NEWCOPYX
	push	rax
	mov	rdi,rax
	mov	rax,[rdi-8]
	test	rax,rax
	js	@@f  ;Ë·rka je vlevo od mantisy
	cmp	rax,[rdi-32]
	jg	@@f  ;Ë·rka je vpravo od mantisy
	mov	rdx,[rdi-24]
	sub	rax,rdx
	jle	@@3
;vynuluj Ë·st za mantisou
	mov	rcx,rax
	push	rdi
	lea	rdi,[rdi+8*rdx]
	xor	rax,rax
	rep stosq
	pop	rdi
;alokuj buffer pro meziv˝sledek vlevo od teËky
@@3:	mov	rcx,[rdi-8]
	inc	rcx
	shl	rcx,3
	lea	rax,[rcx+2*rcx] ; *24
	cmp	[base],8
	jnc	@@a
	lea	rax,[rcx*8] ; *64
@@a:	push	rax
	mov	rcx,rax
	call	AllocA
	mov	rsi,rax
	mov	[rsp],rax
;rsi=buf, rdi=x, obÏ hodnoty jsou i na z·sobnÌku
	mov	rcx,[rdi-8]
	test	rcx,rcx
	jnz	@@high
;p¯ed teËkou je nula
	mov	rax,[buf]
	mov	byte ptr [rax],48  ;'0'
	inc	rax
	jmp	@@tecka

;Ë·st vlevo od teËky
@@high:	xor	rdx,rdx
	mov	edx,[base]
	mov	rbx,[qwordMax+rdx*8]
;cyklus od teËky smÏrem vlevo, rcx je poËÌtadlo
@@h1:	push	rcx
	push	rdi
	test	rbx,rbx
	jnz	@@h2
;base je n·sobek 2
	mov	rdx,[rdi+8*rcx-8]
	jmp	@@h3
;vydÏl ËÌslo p¯ed teËkou mocninou z·kladu
@@h2:	xor	rdx,rdx
@@lpd:	mov	rax,[rdi]
	div	rbx
	mov	[rdi],rax
	add	rdi,8
	dec	rcx
	jnz	@@lpd
;rdx=skupina ËÌslic
@@h3:	xor	rdi,rdi
	mov	edi,[base]
	mov	rax,rdx
	mov	cl,[qwordDigitsI+rdi]
@@2:	xor	rdx,rdx
	div	rdi
;zbytek dÏlenÌ je v˝sledn· ËÌslice
@@d:	mov	dl,[digitTab+rdx]
	mov	[rsi],dl
	inc	rsi
	dec	cl
	jnz	@@2
;p¯esun na dalöÌ osmibyte
	pop	rdi
	pop	rcx
	test	rbx,rbx
	jz	@@dc
	mov	rax,[rdi]
	test	rax,rax
	jnz	@@h1
	add	rdi,8
@@dc:	dec	rcx
	jnz	@@h1
;u¯Ìzni poË·teËnÌ nuly
@@tr:	dec	rsi
	cmp	byte ptr [rsi],48
	jz	@@tr
;obraù v˝sledek
	mov	rax,[buf]
@@rev:	mov	cl,[rsi]
	mov	[rax],cl
	inc	rax
	dec	rsi
	cmp	rsi,[rsp]
	jae	@@rev
;rax=ukazatel za celou Ë·st v˝sledku
@@tecka:
	mov	rdi,[rsp+8]
	mov	rdx,[rdi-8]
	mov	rcx,[rdi-24]
	mov	rbx,[edi-32]
	lea	rdi,[rdi+8*rdx]
	sub	rcx,rdx
	jle	@@end  ;celÈ ËÌslo
;rdi=desetinn· Ë·st okopÌrovanÈho vstupu,  rcx=jejÌ dÈlka
;vypiö teËku
	mov	byte ptr [rax],46  ;'.'
	inc	rax
	mov	rsi,rax
;odhadni poËet cifer
	push	rbx
	xor	rax,rax
	mov	eax,[base]
	fild	qword ptr [rsp]
	fld	qword ptr [dwordDigits+8*rax]
	fmul
	fistp	qword ptr [rsp]
	pop	rdx
	sub	rdx,5
	dec	rcx
;Ë·st vpravo od teËky
;rdx=poËet cifer, rsi=v˝stup, rdi=ukazatel na desetinnou Ë·st, rcx=dÈlka-1
@@low:	push	rdx
	push	rdi
	push	rcx
	xor	rax,rax
	mov	eax,[base]
	mov	rax,[qwordMax+rax*8]
	test	rax,rax
	jnz	@@low1
	mov	rbx,[rdi]
	add	qword ptr [rsp+8],8
	dec	qword ptr [rsp]
	jmp	@@low2
@@low1:	push	rsi
;n·sobenÌ mocninou z·kladu
	mov	rsi,rax
	xor	rbx,rbx  ;carry
@@lpm:	mov	rax,[rdi+8*rcx]
	mul	rsi
	add	rax,rbx
	mov	[rdi+8*rcx],rax
	mov	rbx,rdx
	adc	rbx,0
	dec	rcx
	jns	@@lpm
	pop	rsi
;rbx=skupina ËÌslic
@@low2:	mov	rax,rbx
	xor	rdi,rdi
	mov	edi,[base]
	movzx	rcx,[qwordDigitsI+rdi]
	mov	rbx,rcx
@@d2:	xor	rdx,rdx
	div	rdi
;zbytek dÏlenÌ je v˝sledn· ËÌslice
	mov	dl,[digitTab+rdx]
	dec	rcx
	mov	[rsi+rcx],dl
	jnz	@@d2
	add	rsi,rbx
	pop	rcx
	pop	rdi
	pop	rdx
;poËÌtadlo cyklu
	sub	rdx,rbx
	ja	@@tc
	add	rsi,rdx
	jmp	@@lend
@@tc:	test	rcx,rcx
	js	@@lend
	mov	eax,[error]
	test	eax,eax
	jnz	@@lend
;u¯Ìzni koncovÈ nuly vstupnÌho operandu
	mov	rax,[rdi+8*rcx]
	test	rax,rax
	jnz	@@low
	dec	rcx
	jns	@@low
@@lend:	mov	rax,rsi
;zapiö koncovou nulu
@@end:	mov	byte ptr [rax],0
;smaû pomocnÈ buffery
	pop	rcx
	call	FreeA
@@f:	pop	rcx
	call	FREEX
	ret
WRITEX1	endp
;-------------------------------------
;znak [rsi] p¯evede na ËÌslo rbx
;p¯i chybÏ vracÌ CF=1
rdDigit	proc
	xor	rbx,rbx
	mov	bl,byte ptr [rsi]
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
;naËte kr·tkÈ desetinnÈ ËÌslo a p¯evede ho na zlomek
;vracÌ CY=1, pokud je ËÌslo moc velkÈ, jinak [rax]=konec vstupu
;[rdi]=v˝sledek, [rsi]=vstup
;mÏnÌ rbx
rdFrac	proc	uses rsi
	cmp	qword ptr [rdi-32],2
	jc	@@ret
	mov	rcx,-1  ;dÈlka desetinnÈ Ë·sti
;spoËti Ëitatel
	xor	rax,rax
@@lpn:	test 	rcx,rcx
	jge	@@1
	cmp	byte ptr [rsi],46  ;'.'
	jnz	@@3
	inc	rsi
@@1:	inc	rcx
@@3:	call	rdDigit
	jc	@@4
	inc	rsi
	xor	rdx,rdx
	mov	edx,[baseIn]
	mul	rdx
	test	rdx,rdx
	jnz	@@e
	add	rax,rbx
	jnc	@@lpn
	jmp	@@e
@@4:	mov	[rdi],rax
;spoËti jmenovatel
	mov	rax,1
	xor	rbx,rbx
	mov	ebx,[baseIn]
@@lpd:	dec	rcx
	js	@@2
	mul	rbx
	test	rdx,rdx
	jz	@@lpd
@@e:	stc
	ret
@@2:	mov	[rdi+8],rax
	call	fracReduce
	mov	rax,rsi
	clc
@@ret:	ret
rdFrac	endp
;-------------------------------------
READX1	proc	uses rsi rdi rbx a1,buf
	mov	[a1],rcx
	mov	rdi,rcx
	mov	rsi,rdx
;vyplÚ dÈlku a vynuluj exponent
	mov	rax,[rdi-32]
	xor	rcx,rcx
	mov	[rdi-24],rax
	mov	[rdi-8],rcx
	cmp	[rsi],cl
	jnz	@@1
;pr·zdn˝ ¯etÏzec => nula
	mov	[rdi-24],rcx
	mov	rax,rsi
@@ret:	ret
;znamÈnko
@@1:	cmp	byte ptr [rsi],45	; -
	mov	[rdi-16],rcx
	jnz	@@5
	inc	rsi
	inc	byte ptr [rdi-16]
@@5:	cmp	byte ptr [rsi],43	; '+'
	jnz	@@f
	inc	rsi
@@f:	call	rdFrac
	jnc	@@ret
;Ë·st nalevo od teËky
	xor	rcx,rcx
;naËti dalöÌ ËÌslici do rbx
@@m0:	call	rdDigit
	jc	@@me    ;konec ËÌsla
	inc	rsi
	test	rcx,rcx
	jz	@@m1
;n·sobenÌ z·kladem a p¯iËtenÌ rbx
;ËÌslo od uloûeno obr·cenÏ (nejniûöÌho ¯·du k nejvyööÌmu), proto nelze pouûÌt MULTI
	push	rdi
	push	rsi
	push	rcx
	lea	rdi,[rdi+8*rcx]
	neg	rcx
	xor	rsi,rsi
	mov	esi,[baseIn]
@@lpm:	mov	rax,[rdi+8*rcx]
	mul	rsi
	add	rax,rbx
	mov	[rdi+8*rcx],rax
	mov	rbx,rdx
	adc	rbx,0
	inc	rcx
	jnz	@@lpm
	pop	rcx
	pop	rsi
	pop	rdi
	test	rbx,rbx
	jz	@@m0
;zapiö p¯eteËenÌ a zvÏtöi velikost v˝sledku
@@m1:	mov	[rdi+8*rcx],rbx
	inc	rcx
	inc	qword ptr [rdi-8]
	cmp	rcx,[rdi-24]
	jbe	@@m0
;ztr·ta p¯esnosti, vstupnÌ ¯etÏzec je moc dlouh˝
	dec	rcx
	push	rdi
	push	rsi
	push	rcx
	lea	rsi,[rdi+8]
	cld
	rep movsq
	pop	rcx
	pop	rsi
	pop	rdi
	jmp	@@m0
@@me:	push	rsi
;obr·cenÌ v˝sledku
	lea	rsi,[rdi+8*rcx-8]
	jmp	@@rev1
@@rev:	mov	rax,[rsi]
	mov	rbx,[rdi]
	mov	[rdi],rax
	mov	[rsi],rbx
	add	rdi,8
	sub	rsi,8
@@rev1:	cmp	rdi,rsi
	jb	@@rev
	pop	rsi
;teËka
	mov	rdi,[a1]
	mov	al,[rsi]
	mov	[rdi-24],rcx
	cmp	al,46	; '.'
	jnz	@@end  ;celÈ ËÌslo
;dojeÔ na konec, a potom Ëti ËÌslo pozp·tku
@@sk:	inc	rsi
	call	rdDigit
	jnc	@@sk
 	push	rsi  ;n·vratov· hodnota
;desetinn· ËÌsla majÌ maxim·lnÌ dÈlku
	mov	rdx,[rdi-32]
	xor	rax,rax
	mov	[rdi-24],rdx
	push	rdi  ;ukazatel na v˝sledek
;vynuluj desetinnou Ë·st v˝sledku
	sub	rdx,rcx
	test	rdx,rdx
	jz	@@de
	lea	rdi,[rdi+8*rcx]
	mov	rcx,rdx
	push	rdi
	cld
	rep stosq
;rdi:=ukazatel na desetinnou Ë·st, rcx:=jejÌ dÈlka, rsi:=konec vstupu
	pop	rdi
	mov	rcx,rdx
	mov	rsi,[rsp+8]
;naËti dalöÌ ËÌslici do rdx
@@d0:	dec	rsi
	call	rdDigit
	jc	@@de
	mov	rdx,rbx
;dÏlenÌ z·kladem
	push	rcx
	push	rdi
	xor	rbx,rbx
	mov	ebx,[baseIn]
@@lpd:	mov	rax,[rdi]
	div	rbx
	mov	[rdi],rax
	add	rdi,8
	dec	rcx
	jnz	@@lpd
	pop	rdi
	pop	rcx
	jmp	@@d0
;u¯ÌznutÌ koncov˝ch nul a normalizace
@@de:	pop	rdi
@@trim:	call	trim
	call	norm
	pop	rax	;ukazatel na znak za ËÌslem
	ret
@@end:	push	rsi 
	jmp	@@trim
READX1	endp
;-------------------------------------
FACTORIALI	proc 	uses rsi rdi rbx a0,ai
	mov	[a0],rcx
	mov	[ai],rdx
	call	ONEX
	mov	rsi,[ai]
	test	rsi,rsi
	jz	@@ret     ;0! =1
@@lp:	push	rsi
	mov	rdi,[a0]
	call	mult1
	pop	rsi
	mov	eax,[error]
	test	eax,eax
	jnz	@@ret
	dec	rsi
	jnz	@@lp
@@ret:	ret
FACTORIALI	endp

FFACTI	proc 	uses rsi rdi rbx a0,ai
	mov	[a0],rcx
	mov	[ai],rdx
	call	ONEX
	mov	rsi,[ai]
	test	rsi,rsi
	jz	@@ret     ;0!! =1
@@lp:	push	rsi
	mov	rdi,[a0]
	call	mult1
	pop	rsi
	mov	eax,[error]
	test	eax,eax
	jnz	@@ret
	dec	rsi
	jz	@@ret
	dec	rsi
	jnz	@@lp
@@ret:	ret
FFACTI	endp
;-------------------------------------
;[rcx]=rax^2
sqr	proc
	mul	rax
	test	rdx,rdx
	jnz	@@1
	mov	[rcx],rax
	mov	[rcx-8],rdx
	mov	qword ptr [rcx-24],1
	ret
@@1:	mov	[rcx],rdx
	mov	[rcx+8],rax
	mov	qword ptr [rcx-8],1
	mov	qword ptr [rcx-24],2
	ret
sqr	endp
;-------------------------------------
; y=0;
; for(c=0x8000000000000000; c>0; c>>=1){
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

;rax:=sqrt(rdx:rax)
@sqrti	proc	uses rsi rdi
	mov	rcx,08000000000000000h
	xor	rdi,rdi
	xor	rsi,rsi

@@0:	ror	rcx,1
	add	rsi,rcx
	sub	rax,rdi
	sbb	rdx,rsi
	jnc	@@1
	add	rax,rdi
	adc	rdx,rsi
	sub	rsi,rcx
	sub	rsi,rcx
@@1:	add	rsi,rcx
	shr	rsi,1
	rcr	rdi,1
	ror	rcx,1
	jnc	@@0

@@3:	ror	rcx,1
	add	rdi,rcx
	sub	rax,rdi
	sbb	rdx,rsi
	jnc	@@2
	add	rax,rdi
	adc	rdx,rsi
	sub	rdi,rcx
	sub	rdi,rcx
@@2:	add	rdi,rcx
	shr	rsi,1
	rcr	rdi,1
	ror	rcx,1
	jnc	@@3
	mov	rax,rdi
	ret
@sqrti	endp

;odmocnina ze 64-bitovÈho ËÌsla
;zaokrouhluje smÏrem dol˘
SQRTI	proc
	mov	rax,rcx
	mov	rdx,0
	call	@sqrti
	ret
SQRTI	endp
;-------------------------------------
SQRTX	proc 	uses rsi rdi rbx a0,a1
local	sq,s2,s3,s4,d1,d2,k,ro	
	mov	[a0],rcx
	mov	[a1],rdx
	mov	rcx,rdx	;operand
	mov	rdx,[a0]	;v˝sledek
	xor	rsi,rsi
	cmp	[rcx-24],rsi
	mov	[rdx-24],rsi
	jz	@@ret		; sqrt(0)=0
	cmp	[rcx-16],rsi
	jz	@@1
	lea	rdx,[E_1008]
	mov	rcx,1008
	sub	rsp,32
	call	cerror
	add	rsp,32
	jmp	@@ret
@@1:	cmp	qword ptr [rcx-24],-2
	jnz	@@f2
;zlomek
	mov	rdi,rdx
	mov	rsi,rcx
	mov	rcx,2
@@nd:	mov	rbx,[rsi+rcx*8-8]
	test	rbx,rbx
	js	@@fe
	push	rbx
	fild	qword ptr [rsp]
	fsqrt
	fistp	qword ptr [rsp]
	pop	rax
	lea	rdx,[rdi+rcx*8-8]
	mov	[rdx],rax
	mul	rax
	cmp	rax,rbx
	jnz	@@fe
	dec	rcx
	jnz	@@nd
	and	qword ptr [rdi-16],0
	call	fracexp
	ret
@@fe:	call	fractonewx
	mov	rsi,rax
	sub	rsp,32
	mov	rdx,rax
	mov	rcx,rdi
	call	SQRTX
	add	rsp,32
	mov	rcx,rsi
	call	FREEX
	ret
;alokuj promÏnnou sq
@@f2:	sub	rsp,16
	mov	[sq],rsp
	sub	rsp,HED
;znamÈnko
	mov	[rsp+HED-16],rsi	;kladnÈ
	mov	[rdx-16],rsi
;max(dÈlka v˝sledku+3, dÈlka operandu)+3
	mov	rax,[rdx-32]
	mov	[rdx-24],rax
	add	rax,3
	mov	rdi,[rcx-24]
	cmp	rax,rdi
	jbe	@@6
	mov	rdi,rax
@@6:	add	rdi,3
;alokuj pomocnou promÏnnou s2
	push	rax
	mov	rcx,rax
	call	ALLOCX
	mov	[s2],rax
	pop	rcx
;alokuj pomocnou promÏnnou s4
	call	ALLOCX
	mov	[s4],rax
;alokuj pomocnou promÏnnou s3
	mov	rcx,rdi
	call	ALLOCX
	mov	[s3],rax
;alokuj zbytek
	mov	rcx,rdi
	call	ALLOCX
	mov	rdi,rax
	mov	rsi,[a1]
;naËti nejvyööÌ ¯·d
	xor	rdx,rdx
	mov	rbx,[s2]
	mov	[rbx-8],rdx
	test	qword ptr [rsi-8],1	;parita exponentu
	jz	@@2
	mov	rax,[rsi]
	jmp	@@3
@@2:	mov	rdx,[rsi]
	mov	rax,[rsi+8]
;vypoËti odmocninu z nejvyööÌho ¯·du
@@3:	call	@sqrti
	mov	rcx,rax
	mov	rax,08000000000000000h
	test	rcx,rcx
	js	@@r
	xor	rdx,rdx
	inc	rcx
	div	rcx
	cmp	rax,1
	jbe	@@c
;zvÏtöi operand
@@r:	mov	[ro],rax
	mov	rcx,[sq]
	call	sqr
	sub	rsp,32
	mov	r8,[sq]
	mov	rdx,rsi
	mov	rcx,rdi
	call	MULTX
	add	rsp,32
	jmp	@@12
@@c:	call	copyx
	mov	[ro],0
@@12:	mov	rsi,rdi
	mov	rdi,[a0]
;exponent
	mov	rax,[rsi-8]
	inc	rax
	sar	rax,1
	mov	[rdi-8],rax
	mov	qword ptr [rsi-8],1
;zapiö prvnÌ ËÌslici do v˝sledku
	mov	rdx,[rsi]
	mov	rax,[rsi+8]
	call	@sqrti
	mov	[rdi],rax
;poËÌtadlo cyklu
	push	qword ptr [rdi-24]
;rbx=dÏlitel, rdi=v˝sledek, rsi=zbytek
;od zbytku odeËti druhou mocninu p¯idanÈ ËÌslice
@@lp:	mov	rax,[rdi]
	test	rax,rax
	jz	@@ad	;nulovou ËÌslici neodeËÌtej od zbytku
	mov	rcx,[sq]
	call	sqr
	sub	rsp,32
	mov	r8,[sq]
	mov	rdx,rsi
	mov	rcx,[s3]
	call	MINUSX
	add	rsp,32
	mov	rax,[s3]
	cmp	byte ptr [rax-16],0
	jz	@@0
	cmp	qword ptr [rax-24],0
	jz	@@0
;zbytek je z·porn˝ ->  musÌ se opravit
	dec	qword ptr [rsi-8]
	dec	qword ptr [rbx-8]
	jmp	@@d
@@0:	mov	[s3],rsi
	mov	rsi,rax
;posuÚ ukazatel na v˝sledek
@@ad:	mov	rax,[rdi]	;pro v˝slednÈ zaokroulenÌ
	add	rdi,8
;sniû poËÌtadlo
	pop	rcx
	dec	rcx
	js	@@z		;hotovo, podle rax zaokrouhli
	cmp	qword ptr [rsi-24],0
	jz	@@e		;zbytek je 0 -> zkraù v˝sledek
	push	rcx
;p¯idej dvojn·sobek dalöÌ ËÌslice k s2
	mov	rcx,[rbx-24]
	shl	rax,1
	mov	[rbx+8*rcx],rax
	inc	rcx
	mov	[rbx-24],rcx
	jnc	@@5
;p¯eteËenÌ na konci s2
	push	rcx
	dec	rcx
	dec	rcx
	mov	rax,1
	push	rdi
	push	rsi
	push	rbx
	mov	rdi,rbx
	call	incri
	pop	rbx
	pop	rsi
	pop	rdi
	pop	qword ptr [rbx-24]
;zvyö exponent zbytku
@@5:	inc	qword ptr [rsi-8]
;zapamatuj si nejvyööÌ ¯·d dÏlitele
	mov	rax,[rbx]
	mov	[d1],rax
	xor	rax,rax
	cmp	qword ptr [rbx-24],1
	jz	@@7
	mov	rax,[rbx+8]
@@7:	mov	[d2],rax
;naËti nejvyööÌ ¯·d zbytku
	xor	rdx,rdx
	mov	rax,[rsi]
	mov	rcx,[rbx-8]
	cmp	[rsi-8],rcx
	jz	@@4
	mov	qword ptr [rdi],0	;zbytek < "dÏlitel"
	jl	@@o
	mov	rdx,rax
	xor	rax,rax
	cmp	qword ptr [rsi-24],1
	jz	@@4
	mov	rax,[rsi+8]
;ochrana proti p¯eteËenÌ
@@4:	cmp	rdx,[d1]
	jb	@@9
	mov	rcx,rax
	stc
	sbb	rax,rax	;0xFFFFFFFFFFFFFFFF
	add	rcx,[d1]
	jc	@@8
	jmp	@@10
;odhadni v˝slednou ËÌslici
@@9:	div	[d1]
	mov	rcx,rdx	;zbytek dÏlenÌ
@@10:	push	rax	;uchovej v˝slednou cifru
	mul	[d2]
	sub	rdx,rcx
	jc	@@11
	mov	rcx,[rsi-8]
	inc	rcx
	sub	rcx,[rbx-8]
	cmp	qword ptr [rsi-24],rcx
	jbe	@@k	;jednocifern˝ nebo dvoucifern˝ zbytek
	sub	rax,[rsi+8*rcx]
	sbb	rdx,0
	jc	@@11
;dokud je zbytek z·porn˝, sniûuj v˝slednou cifru
@@k:	mov	rcx,rax
	or	rcx,rdx
	jz	@@11
	dec	qword ptr [rsp]
	sub	rax,[d2]
	sbb	rdx,[d1]
	jnc	@@k
@@11:	pop	rax
;vyn·sob dÏlitel a odeËti od zbytku
@@8:	mov	[rdi],rax
	sub	rsp,32
	mov	r8,rax
	mov	rdx,rbx
	mov	rcx,[s4]
	call	MULTI
	mov	r8,[s4]
	mov	rdx,rsi
	mov	rcx,[s3]
	call	MINUSX
	add	rsp,32
	mov	rcx,rsi
	mov	rsi,[s3]
	mov	[s3],rcx
	cmp	byte ptr [rsi-16],0
	jz	@@o	;zbytek je kladn˝
	cmp	qword ptr [rsi-24],0
	jz	@@o
;oprav v˝sledek, aby zbytek nebyl z·porn˝
@@d:	dec	qword ptr [rdi]
	sub	rsp,32
	mov	r8,rbx
	mov	rdx,rsi
	mov	rcx,[s3]
	call	PLUSX
	add	rsp,32
	mov	rcx,rsi
	mov	rsi,[s3]
	mov	[s3],rcx
	cmp	qword ptr [rsi-24],0
	jz	@@o
	cmp	byte ptr [rsi-16],0
	jnz	@@d
@@o:	inc	qword ptr [rsi-8]
	inc	qword ptr [rbx-8]
	jmp	@@lp
@@e:	mov	rdi,[a0]
	sub	[rdi-24],rcx
	jmp	@@f
@@z:	test	rax,rax
	jns	@@f
	mov	rdi,[a0]
	push	rsi
	push	rbx
	call	roundi
	pop	rbx
	pop	rsi
;smaû pomocnÈ promÏnnÈ
@@f:	mov	rcx,rsi
	call	FREEX
	mov	rcx,rbx
	call	FREEX
	mov	rcx,[s3]
	call	FREEX
	mov	rcx,[s4]
	call	FREEX
	add	rsp,16+HED
	mov	rdi,[a0]
	mov	r8,[ro]
	test	r8,r8
	jz	@@t
	sub	rsp,32
	mov	rdx,rdi
	mov	rcx,rdi
	call	DIVI
	add	rsp,32
@@t:	call	trim
@@ret:	ret
SQRTX	endp
;-------------------------------------
PI	proc 	uses rsi rdi rbx a0
local	a,b,z,t,y,x,n	
	mov	[a0],rcx
	mov	rdi,rcx
;alokuj pomocnÈ promÏnnÈ
	lea	rcx,[a]
	push	rcx
	lea	rcx,[b]
	push	rcx
	lea	rcx,[z]
	push	rcx
	lea	rcx,[t]
	push	rcx
	lea	r9,[y]
	lea	r8,[x]
	mov	rdx,qword ptr [rdi-32]
	mov	rcx,6
	sub	rsp,32
	call	ALLOCNX
	add	rsp,64

; x=1; a=1; b=sqrt(1/2); z=1/4
	mov	rcx,[x]
	call	ONEX
	mov	rcx,[a]
	call	ONEX
	mov	rax,[y]
	mov	rdx,8000000000000000h
	mov	[rax],rdx
	mov	byte ptr [rax-24],1
	and	qword ptr [rax-8],0
	sub	rsp,32
	mov	rdx,rax
	mov	rcx,[b]
	call	SQRTX
	add	rsp,32
	mov	rax,[z]
	mov	rdx,4000000000000000h
	mov	[rax],rdx
	mov	byte ptr [rax-24],1
	and	qword ptr [rax-8],0
	mov	[n],0
;cyklus
@@lp:	inc	[n]
; y=(a+b)/2
	sub	rsp,32
	mov	r8,[b]
	mov	rdx,[a]
	mov	rcx,[t]
	call	PLUSX
	mov	r8,2
	mov	rdx,[t]
	mov	rcx,[y]
	call	DIVI
; b=sqrt(b*a)
	mov	r8,[a]
	mov	rdx,[b]
	mov	rcx,[t]
	call	MULTX
	mov	rdx,[t]
	mov	rcx,[b]
	call	SQRTX
; a=y
	push	[a]
	push	[y]
	pop	[a]
	pop	[y]
; t=x*(a-y)^2
	mov	r8,[y]
	mov	rdx,[a]
	mov	rcx,[t]
	call	MINUSX
	mov	r8,[t]
	mov	rdx,[t]
	mov	rcx,[y]
	call	MULTX
	mov	r8,[x]
	mov	rdx,[y]
	mov	rcx,[t]
	call	MULTX
; z=z-t
	mov	r8,[t]
	mov	rdx,[z]
	mov	rcx,[y]
	call	MINUSX
	push	[y]
	push	[z]
	pop	[y]
	pop	[z]
; x=x*2
	mov	rsi,2
	mov	rdi,[x]
	call	mult1
	add	rsp,32
;until t<d
	mov	eax,[error]
	test	eax,eax
	jnz	@@e
	mov	rax,[t]
	cmp	qword ptr [rax-24],0
	jz	@@e
	cmp	qword ptr [rax-24],-2
	jz	@@lp
	mov	rcx,[rax-8]
	mov	rax,[a0]
	add	rcx,[rax-32]
	jns	@@lp
; a0= (a+b)^2/4z
@@e:	sub	rsp,32
	mov	r8,[b]
	mov	rdx,[a]
	mov	rcx,[y]
	call	PLUSX
	mov	r8,[y]
	mov	rdx,[y]
	mov	rcx,[t]
	call	MULTX
	mov	rsi,4
	mov	rdi,[z]
	call	mult1
	mov	r8,[z]
	mov	rdx,[t]
	mov	rcx,[a0]
	call	DIVX
	add	rsp,32
;smaû pomocnÈ promÏnnÈ
	mov	rcx,[x]
	call	FREEX
	mov	rax,[n]
	ret
PI	endp
;-------------------------------------
overflow	proc
	lea	rdx,[E_1011]
	mov	rcx,1011
	sub	rsp,32
	call	cerror
	add	rsp,32
	ret
overflow	endp
;-------------------------------------

public	ALLOCX,FREEX,NEWCOPYX
public	SETX,SETXN,ZEROX,ONEX,FRACTOX
public	NEGX,ABSX,SIGNX,TRUNCX,INTX,CEILX,ROUNDX,FRACX,SCALEX
public	COPYX,WRITEX1,READX1
public	MULTX,MULTX1,MULTI,MULTIN,MULTI1,DIVX,DIVI,MODI
public	PLUSX,MINUSX,PLUSU,MINUSU,ANDU,ORU,XORU
public	CMPX,CMPU,FACTORIALI,FFACTI,SQRTX,SQRTI,PI,ALLOCNX
public	ALLOCN,ANDU@12,ORU@12,XORU@12

	end
