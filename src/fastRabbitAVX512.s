.intel_syntax noprefix
.text
.globl fastrabbit
.type  fastrabbit, @function

## System V AMD64 calling convention (same pointer-array layout as fastRabbitSSE.s):
##   rdi = APTR   (float*[3] -> A0, A1, A2 vectors, 64-byte each)
##   rsi = I      (const float*, padded projection image)
##   rdx = TMP    (float*[3] -> tmp0, tmp1, tmp2 vectors, 64-byte each)
##   rcx = VOL    (float*, volume chunk at z*L*L + y*L + start)
##   r8  = WX     (float*, 16-element wx vector)
##   r9  = MM4    (float*, 16-element step vector = VECTORSIZE*MM)
##   [rsp+8]  = ISX4  (float*, 16-element image-stride vector)
##   [rsp+16] = count (int, number of voxels to process, multiple of 16)

fastrabbit:
push  rbp
mov   rbp, rsp

## ---- load zmm inputs from pointer arrays --------------------------------
mov   rax, [rdi]
vmovaps zmm0, [rax]          ## A0
mov   rax, [rdi+8]
vmovaps zmm1, [rax]          ## A1
mov   rax, [rdi+16]
vmovaps zmm2, [rax]          ## A2
mov   rax, [rdx]
vmovaps zmm3, [rax]          ## tmp0
mov   rax, [rdx+8]
vmovaps zmm4, [rax]          ## tmp1
mov   rax, [rdx+16]
vmovaps zmm5, [rax]          ## tmp2
vmovaps zmm6, [r8]           ## wx
vmovaps zmm7, [r9]           ## mm  (= VECTORSIZE * voxelSize)
mov   rax, [rbp+16]          ## ISX4 pointer (7th arg)
vmovaps zmm8, [rax]          ## width (padded image line stride)
mov   r8d, [rbp+24]          ## count (8th arg) -> N

## generate one = 1.0f broadcast into zmm9
mov   eax, 0x3f800000
vpbroadcastd zmm9, eax

pop   rbp

## ---- register aliases (match LolaMIC-ASM/src/fastrabbit.S) ---------------
#define I       rdi
#define VOL     rcx
#define N       r8d
#define A0      zmm0
#define A1      zmm1
#define A2      zmm2
#define tmp0    zmm3
#define tmp1    zmm4
#define tmp2    zmm5
#define wx      zmm6
#define mm      zmm7
#define width   zmm8
#define one     zmm9

#define i       r9d
#define u       zmm10
#define v       zmm11
#define w       zmm12
#define w_inv   zmm13
#define ix      zmm14
#define iy      zmm15
#define iix     zmm16
#define iiy     zmm17
#define w2_inv  zmm18
#define scalx   zmm19
#define scaly   zmm20

#define off_bl  zmm17
#define off_br  zmm10
#define off_tl  zmm11
#define off_tr  zmm12

#define ioff_bl zmm17
#define ioff_br zmm10
#define ioff_tl zmm11
#define ioff_tr zmm12

#define val_bl  zmm13
#define val_br  zmm14
#define val_tl  zmm15
#define val_tr  zmm16

#define sval_bl zmm13
#define sval_tl zmm15

#define val_b   zmm14
#define val_t   zmm16
#define sval_b  zmm14
#define val     zmm16
#define sval    zmm16

#define oscalx  zmm21
#define oscaly  zmm22

#define NXTioff_bl zmm23
#define NXTioff_br zmm24
#define NXTioff_tl zmm25
#define NXTioff_tr zmm26
#define NXTscalx   zmm27
#define NXTscaly   zmm28
#define NXTw2_inv  zmm29

## ---- compute first iteration offsets (NXT) --------------------------------
        vmovaps         w, wx
        kxnorw          k2, k2, k2
        vmovaps         v, wx
        kxnorw          k3, k3, k3
        vmovaps         u, wx
        kxnorw          k4, k4, k4
        vfmadd213ps     w, A2, tmp2
        kxnorw          k5, k5, k5
        vfmadd213ps     u, A0, tmp0
        vrcp14ps        w_inv, w
        vfmadd213ps     v, A1, tmp1
        vmulps          ix, u, w_inv
        vmulps          iy, v, w_inv
        vrndscaleps     iix, ix, 3
        vrndscaleps     iiy, iy, 3
        vmulps          w2_inv, w_inv, w_inv
        vsubps          scaly, iy, iiy
        vfmadd213ps     iiy, width, iix
        vsubps          scalx, ix, iix
        vaddps          off_tl, width, off_bl
        vaddps          off_br, one, off_bl
        vaddps          off_tr, one, off_tl
        vcvttps2dq      ioff_bl, off_bl
        vcvttps2dq      ioff_br, off_br
        vcvttps2dq      ioff_tl, off_tl
        vcvttps2dq      ioff_tr, off_tr

## ---- save as NXT, advance wx ----------------------------------------------
        vmovaps         NXTioff_bl, ioff_bl
        vmovaps         NXTioff_br, ioff_br
        vmovaps         NXTioff_tl, ioff_tl
        vmovaps         NXTioff_tr, ioff_tr
        vmovaps         NXTscalx, scalx
        vmovaps         NXTscaly, scaly
        vmovaps         NXTw2_inv, w2_inv
        vaddps          wx, wx, mm

        mov             r10, VOL
        xor i, i
        sub N, 16
        test N, N
        jle .Lskip

xor i, i
        prefetcht0      [rcx]
        prefetcht2      [rcx + 256]
.align 32
.Lloop:
        vmovaps         w, wx
        kxnorw          k2, k2, k2
        vmovaps         v, wx
        kxnorw          k3, k3, k3
        vmovaps         u, wx
        kxnorw          k4, k4, k4
        vfmadd213ps     w, A2, tmp2
        kxnorw          k5, k5, k5
        vfmadd213ps     u, A0, tmp0
        mov             r10, VOL
        vrcp14ps        w_inv, w
        lea             r11, [r9]
        vfmadd213ps     v, A1, tmp1
        add             i, 16
        vmulps          ix, u, w_inv
        vmulps          iy, v, w_inv
        vrndscaleps     iix, ix, 3
        vrndscaleps     iiy, iy, 3
        vmulps          w2_inv, w_inv, w_inv
        vsubps          scaly, iy, iiy
        vfmadd213ps     iiy, width, iix
        vsubps          scalx, ix, iix
        vaddps          off_tl, width, off_bl
        vaddps          off_br, one, off_bl
        vaddps          off_tr, one, off_tl
        vcvttps2dq      ioff_bl, off_bl
        vcvttps2dq      ioff_br, off_br
        vcvttps2dq      ioff_tl, off_tl
        vcvttps2dq      ioff_tr, off_tr

.LL100: vgatherdps      val_bl{k2}, [I + NXTioff_bl * 4]
        kortestw        k2, k2
        jnz             .LL100
        vsubps          oscalx, one, NXTscalx
.LL200: vgatherdps      val_br{k3}, [I + NXTioff_br * 4]
        kortestw        k3, k3
        jnz             .LL200
        vsubps          oscaly, one, NXTscaly
.LL300: vgatherdps      val_tl{k4}, [I + NXTioff_tl * 4]
        kortestw        k4, k4
        jnz             .LL300
        vaddps          wx, wx, mm
.LL400: vgatherdps      val_tr{k5}, [I + NXTioff_tr * 4]
        kortestw        k5, k5
        jnz             .LL400
        vmulps          sval_bl, val_bl, oscalx
        cmp i, N
        vmulps          sval_tl, val_tl, oscalx
        vfmadd213ps     val_br, NXTscalx, sval_bl
        vfmadd213ps     val_tr, NXTscalx, sval_tl
        vmulps          sval_b, val_b, oscaly
        vfmadd213ps     val_t, NXTscaly, sval_b
        vfmadd213ps     val, NXTw2_inv, [r10 + r11 * 4]
        prefetcht0      [r10 + r9 * 4]
        prefetcht2      [r10 + r9 * 4 + 256]
        nop
        vmovaps         [r10 + r11 * 4], sval

        vmovaps         NXTioff_bl, ioff_bl
        vmovaps         NXTioff_br, ioff_br
        vmovaps         NXTioff_tl, ioff_tl
        vmovaps         NXTioff_tr, ioff_tr
        vmovaps         NXTscalx, scalx
        vmovaps         NXTscaly, scaly
        vmovaps         NXTw2_inv, w2_inv
        jl              .Lloop

.Lskip:
        kxnorw          k2, k2, k2
        kxnorw          k3, k3, k3
        kxnorw          k4, k4, k4
        kxnorw          k5, k5, k5
.LL130: vgatherdps      val_bl{k2}, [I + NXTioff_bl * 4]
        kortestw        k2, k2
        jnz             .LL130
        vsubps          oscalx, one, NXTscalx
.LL230: vgatherdps      val_br{k3}, [I + NXTioff_br * 4]
        kortestw        k3, k3
        jnz             .LL230
        vsubps          oscaly, one, NXTscaly
.LL330: vgatherdps      val_tl{k4}, [I + NXTioff_tl * 4]
        kortestw        k4, k4
        jnz             .LL330
        lea             r11, [r9]
.LL430: vgatherdps      val_tr{k5}, [I + NXTioff_tr * 4]
        kortestw        k5, k5
        jnz             .LL430
        vmulps          sval_bl, val_bl, oscalx
        vmulps          sval_tl, val_tl, oscalx
        vfmadd213ps     val_br, NXTscalx, sval_bl
        vfmadd213ps     val_tr, NXTscalx, sval_tl
        vmulps          sval_b, val_b, oscaly
        vfmadd213ps     val_t, NXTscaly, sval_b
        vfmadd213ps     val, NXTw2_inv, [r10 + r11 * 4]
        nop
        vmovaps         [r10 + r11 * 4], sval
ret
.size fastrabbit, .-fastrabbit
