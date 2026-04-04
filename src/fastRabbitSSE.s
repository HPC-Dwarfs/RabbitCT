.intel_syntax noprefix
.text
.globl fastrabbit
.type fastrabbit, @function
fastrabbit :
push rbp
mov rbp, rsp
push rbx
push r12
push r13
push r14
push r15
mov r13, [rdi]
mov r14, [rdi+8]
mov r15, [rdi+16]
mov r11, [rdx]
movaps xmm12, [r11]
mov r12, [rdx+8]
movaps xmm13, [r12]
mov rax, [rdx+16]
movaps xmm14, [rax]
movaps xmm10, [r8]
mov edx, [rbp+24]
pcmpeqw xmm15,xmm15
pslld xmm15,25
psrld xmm15,2
xor r8, r8
xor rdi, rdi
xor ebx, ebx
.align 16
1:
movaps xmm0, [r13]
movaps xmm1, [r14]
movaps xmm2, [r15]
mulps xmm2, xmm10
addps xmm2, xmm14
mulps xmm0, xmm10
addps xmm0, xmm12
mulps xmm1, xmm10
addps xmm1, xmm13
rcpps xmm11, xmm2
mulps xmm2, xmm11            # w * w_inv
movaps xmm9, xmm15           # 1.0
subps xmm9, xmm2             # err = 1 - w*w_inv
mulps xmm9, xmm11            # w_inv * err
addps xmm11, xmm9            # w_inv += w_inv*err (refined)
addps xmm10, [r9]
mulps xmm0, xmm11
mulps xmm1, xmm11
roundps xmm2, xmm0, 0x03
roundps xmm3, xmm1, 0x03
subps xmm1, xmm3
movaps xmm5, xmm15
mov rax, [rbp+16]
mulps xmm3, [rax]
movaps xmm4, [rax]
addps xmm4, xmm3
addps xmm4, xmm2
addps xmm3, xmm2
movaps xmm8, xmm15
subps xmm0, xmm2
movsldup xmm6, xmm1
cvtss2si rax, xmm3
psrldq xmm3, 0x4
movlps xmm2, [rsi + rax * 4]
subps xmm5, xmm6
cvtss2si rdi, xmm3
psrldq xmm3, 0x4
cvtss2si rax, xmm3
movhps xmm2, [rsi + rax * 4]
mulps xmm2, xmm5
cvtss2si rax, xmm4
psrldq xmm4, 0x4
movlps xmm5, [rsi + rax * 4]
cvtss2si r8, xmm4
psrldq xmm4, 0x4
cvtss2si rax, xmm4
movhps xmm5, [rsi + rax * 4]
mulps xmm5, xmm6
movshdup xmm6, xmm1
addps xmm5, xmm2
subps xmm8, xmm6
mulps xmm11, xmm11
movlps xmm7, [rsi + rdi * 4]
psrldq xmm3, 0x4
cvtss2si rax, xmm3
movhps xmm7, [rsi + rax * 4]
mulps xmm7, xmm8
movlps xmm1, [rsi + r8 * 4]
psrldq xmm4, 0x4
cvtss2si rax, xmm4
movhps xmm1, [rsi + rax * 4]
mulps xmm1, xmm6
addps xmm1, xmm7
movshdup xmm7, xmm5
movaps xmm8, xmm15
movaps xmm4, xmm1
movsldup xmm2, xmm1
blendps xmm2, xmm5, 0x5
blendps xmm4, xmm7, 0x5
mulps xmm4, xmm0
subps xmm8, xmm0
mulps xmm8, xmm2
addps xmm8, xmm4
mulps xmm8, xmm11
addps xmm8, [rcx + rbx * 4]
movaps [rcx + rbx * 4], xmm8
add ebx, 4
cmp ebx, edx
jl 1b
pop r15
pop r14
pop r13
pop r12
pop rbx
mov  rsp, rbp
pop rbp
ret
.size fastrabbit, .-fastrabbit


