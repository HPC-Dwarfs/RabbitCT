.intel_syntax noprefix
.text
.globl fastrabbit
.type fastrabbit, @function

# AVX2 (256-bit, 8-wide) backprojection kernel.
# Same pointer-array calling convention as fastRabbitSSE.s:
#   rdi = APTR   (float*[3] -> A0, A1, A2 vectors, 32 bytes each)
#   rsi = I      (const float*, padded projection image)
#   rdx = TMP    (float*[3] -> tmp0, tmp1, tmp2 vectors, 32 bytes each)
#   rcx = VOL    (float*, volume output pointer)
#   r8  = WX     (float*, 8-element wx vector)
#   r9  = MM4    (float*, 8-element step vector = VECTORSIZE*mm)
#   [rsp+8]  = ISX4  (float*, 8-element image-stride vector)
#   [rsp+16] = count (int, number of voxels to process)
#
# Register plan (persistent across iterations):
#   ymm9  = mm (step vector)
#   ymm10 = wx (current x-positions, advanced each iteration)
#   ymm12 = tmp0, ymm13 = tmp1, ymm14 = tmp2
#   ymm15 = one (1.0f broadcast)
#   r12   = ISX4 pointer (width vector in memory)
#   r13   = &A0, r14 = &A1, r15 = &A2
#   ebx   = loop counter, edx = count

fastrabbit:
push rbp
mov  rbp, rsp
push rbx
push r12
push r13
push r14
push r15

# Load A0/A1/A2 array pointers (re-read from memory each iteration)
mov  r13, [rdi]
mov  r14, [rdi+8]
mov  r15, [rdi+16]

# Load tmp vectors into persistent registers
mov  r11, [rdx]
vmovaps ymm12, [r11]           # tmp0
mov  r11, [rdx+8]
vmovaps ymm13, [r11]           # tmp1
mov  rax, [rdx+16]
vmovaps ymm14, [rax]           # tmp2

# Load wx and mm step
vmovaps ymm10, [r8]            # wx
vmovaps ymm9, [r9]             # mm = VECTORSIZE * voxelSize

# ISX4 pointer (7th arg) and count (8th arg)
mov  r12, [rbp+16]
mov  edx, [rbp+24]

# Broadcast 1.0f into ymm15
mov  eax, 0x3f800000
vmovd xmm15, eax
vbroadcastss ymm15, xmm15

xor  ebx, ebx
.align 16
1:
# Load projection matrix column vectors from memory
vmovaps ymm0, [r13]
vmovaps ymm1, [r14]
vmovaps ymm2, [r15]

# u = A0*wx + tmp0,  v = A1*wx + tmp1,  w = A2*wx + tmp2
vfmadd213ps ymm0, ymm10, ymm12
vfmadd213ps ymm1, ymm10, ymm13
vfmadd213ps ymm2, ymm10, ymm14

# w_inv = 1/w with Newton-Raphson refinement: w_inv *= (2 - w * w_inv)
vrcpps ymm3, ymm2               # w_inv ~= 1/w (12-bit approx)
vfnmadd213ps ymm2, ymm3, ymm15  # ymm2 = -(ymm3 * ymm2) + 1.0 = 1 - w*w_inv = err
vfmadd213ps ymm2, ymm3, ymm3    # ymm2 = ymm3*err + ymm3 = w_inv*(1+err) (refined)
vmulps ymm4, ymm0, ymm2         # ix = u * w_inv
vmulps ymm5, ymm1, ymm2         # iy = v * w_inv
vmulps ymm6, ymm2, ymm2         # w2_inv = w_inv^2

# Integer pixel coords and fractional bilinear weights
vroundps ymm0, ymm4, 3          # iix = trunc(ix)
vroundps ymm1, ymm5, 3          # iiy = trunc(iy)
vsubps ymm7, ymm4, ymm0         # scalx = ix - iix
vsubps ymm8, ymm5, ymm1         # scaly = iy - iiy

# Compute four bilinear sample offsets: bl, tl, br, tr
vmovaps ymm2, [r12]             # width
vfmadd213ps ymm1, ymm2, ymm0   # off_bl = iiy*width + iix
vaddps ymm2, ymm1, [r12]        # off_tl = off_bl + width
vaddps ymm3, ymm1, ymm15        # off_br = off_bl + 1
vaddps ymm4, ymm2, ymm15        # off_tr = off_tl + 1

# Convert to int32 indices for gather
vcvttps2dq ymm1, ymm1           # ioff_bl
vcvttps2dq ymm2, ymm2           # ioff_tl
vcvttps2dq ymm3, ymm3           # ioff_br
vcvttps2dq ymm4, ymm4           # ioff_tr

# AVX2 gather: vgatherdps dst, [base + idx*4], mask
# mask is consumed (zeroed) by gather; reset before each
vpcmpeqd ymm11, ymm11, ymm11
vgatherdps ymm5, [rsi + ymm1*4], ymm11    # val_bl
vpcmpeqd ymm11, ymm11, ymm11
vgatherdps ymm1, [rsi + ymm3*4], ymm11    # val_br
vpcmpeqd ymm11, ymm11, ymm11
vgatherdps ymm3, [rsi + ymm2*4], ymm11    # val_tl
vpcmpeqd ymm11, ymm11, ymm11
vgatherdps ymm0, [rsi + ymm4*4], ymm11    # val_tr

# Bilinear interpolation
vsubps ymm2, ymm15, ymm7        # oscalx = 1 - scalx
vsubps ymm4, ymm15, ymm8        # oscaly = 1 - scaly
vmulps ymm5, ymm5, ymm2         # val_bl * oscalx
vfmadd213ps ymm1, ymm7, ymm5    # val_b = val_br*scalx + val_bl*oscalx
vmulps ymm3, ymm3, ymm2         # val_tl * oscalx
vfmadd213ps ymm0, ymm7, ymm3    # val_t = val_tr*scalx + val_tl*oscalx
vmulps ymm1, ymm1, ymm4         # val_b * oscaly
vfmadd213ps ymm0, ymm8, ymm1    # val = val_t*scaly + val_b*oscaly

# Weight by 1/w^2 and accumulate into volume
vmulps ymm0, ymm0, ymm6
vaddps ymm0, ymm0, [rcx + rbx*4]
vmovaps [rcx + rbx*4], ymm0

# Advance wx and loop counter
vaddps ymm10, ymm10, ymm9
add    ebx, 8
cmp    ebx, edx
jl     1b

vzeroupper
pop r15
pop r14
pop r13
pop r12
pop rbx
mov rsp, rbp
pop rbp
ret
.size fastrabbit, .-fastrabbit
