OPTION	DOTNAME
.text$	SEGMENT ALIGN(256) 'CODE'

PUBLIC	OPENSSL_ia32_cpuid

ALIGN	16
OPENSSL_ia32_cpuid	PROC PUBLIC
	mov	QWORD PTR[8+rsp],rdi	;WIN64 prologue
	mov	QWORD PTR[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_OPENSSL_ia32_cpuid::
	mov	rdi,rcx




	mov	rdi,rcx
	mov	r8,rbx

	xor	eax,eax
	mov	DWORD PTR[8+rdi],eax
	cpuid
	mov	r11d,eax

	xor	eax,eax
	cmp	ebx,0756e6547h
	setne	al
	mov	r9d,eax
	cmp	edx,049656e69h
	setne	al
	or	r9d,eax
	cmp	ecx,06c65746eh
	setne	al
	or	r9d,eax
	jz	$L$intel

	cmp	ebx,068747541h
	setne	al
	mov	r10d,eax
	cmp	edx,069746E65h
	setne	al
	or	r10d,eax
	cmp	ecx,0444D4163h
	setne	al
	or	r10d,eax
	jnz	$L$intel




	mov	eax,080000000h
	cpuid


	cmp	eax,080000001h
	jb	$L$intel
	mov	r10d,eax
	mov	eax,080000001h
	cpuid


	or	r9d,ecx
	and	r9d,000000801h

	cmp	r10d,080000008h
	jb	$L$intel

	mov	eax,080000008h
	cpuid

	movzx	r10,cl
	inc	r10

	mov	eax,1
	cpuid

	bt	edx,28
	jnc	$L$generic
	shr	ebx,16
	cmp	bl,r10b
	ja	$L$generic
	and	edx,0efffffffh
	jmp	$L$generic

$L$intel::
	cmp	r11d,4
	mov	r10d,-1
	jb	$L$nocacheinfo

	mov	eax,4
	mov	ecx,0
	cpuid
	mov	r10d,eax
	shr	r10d,14
	and	r10d,0fffh

	cmp	r11d,7
	jb	$L$nocacheinfo

	mov	eax,7
	xor	ecx,ecx
	cpuid
	mov	DWORD PTR[8+rdi],ebx

$L$nocacheinfo::
	mov	eax,1
	cpuid

	and	edx,0bfefffffh
	cmp	r9d,0
	jne	$L$notintel
	or	edx,040000000h
	and	ah,15
	cmp	ah,15
	jne	$L$notintel
	or	edx,000100000h
$L$notintel::
	bt	edx,28
	jnc	$L$generic
	and	edx,0efffffffh
	cmp	r10d,0
	je	$L$generic

	or	edx,010000000h
	shr	ebx,16
	cmp	bl,1
	ja	$L$generic
	and	edx,0efffffffh
$L$generic::
	and	r9d,000000800h
	and	ecx,0fffff7ffh
	or	r9d,ecx

	mov	r10d,edx
	bt	r9d,27
	jnc	$L$clear_avx
	xor	ecx,ecx
DB	00fh,001h,0d0h
	and	eax,6
	cmp	eax,6
	je	$L$done
$L$clear_avx::
	mov	eax,0efffe7ffh
	and	r9d,eax
	and	DWORD PTR[8+rdi],0ffffffdfh
$L$done::
	mov	DWORD PTR[4+rdi],r9d
	mov	DWORD PTR[rdi],r10d
	mov	rbx,r8
	mov	rdi,QWORD PTR[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD PTR[16+rsp]
	DB	0F3h,0C3h		;repret
$L$SEH_end_OPENSSL_ia32_cpuid::
OPENSSL_ia32_cpuid	ENDP


.text$	ENDS
END
