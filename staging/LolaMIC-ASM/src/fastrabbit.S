.intel_syntax noprefix
.text
.globl fastrabbit
.type fastrabbit, @function
fastrabbit:
## Only push/pop callee saved registers (rbp, rsp, r12-r15) we actually use: none

## Arguments
## 1. rdi       I
## 2. rsi       VOL
## 3. rdx       float vector: imagewidth
## 4. rcx       float vector: one
## 5. r8        N
## 6. r9        (not used)

## ARGUMENTS
#define I       rdi
#define VOL     rsi
#define N       r8d
#define A0      zmm0
#define A1      zmm1
#define A2      zmm2
#define tmp0    zmm3
#define tmp1    zmm4
#define tmp2    zmm5
#define wx      zmm6
#define MM      zmm7

## not arguments per se
#define pad_width  zmm8
#define float_one  zmm9
vmovaps pad_width, [rdx]                    # IMAGEWIDTH
vmovaps float_one, [rcx]                    # ONE

#define i       r10d
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

#define oscalx  zmm21
#define oscaly  zmm22

# set COUNTER to zero
.align 32
xor i, i
       vprefetchenta   [VOL]
       vprefetche2     [VOL + 256]
..loop:
# Part 1
        vmovaps         w, wx         
        kxnor           k2, k2

        vmovaps         v, wx          
        kxnor           k3, k3

        vmovaps         u, wx           
        kxnor           k4, k4

        vfmadd213ps     w, A2, tmp2      
        kxnor           k5, k5

        vfmadd213ps     u, A0, tmp0       
        #mov             r10, VOL

        vrcp23ps        w_inv, w           
        lea             r11, [r10]

        vfmadd213ps     v, A1, tmp1         
        add             i, 16

        vmulps          ix, u, w_inv         

        vmulps          iy, v, w_inv          

        vrndfxpntps     iix, ix, 3             

        vrndfxpntps     iiy, iy, 3              

        vmulps          w2_inv, w_inv, w_inv

        vsubps          scaly, iy, iiy          

        vfmadd213ps     iiy, pad_width, iix

        vsubps          scalx, ix, iix

        vaddps          off_tl, pad_width, off_bl

        vaddps          off_br, float_one, off_bl

        vaddps          off_tr, float_one, off_tl

        vcvtfxpntps2dq  ioff_bl, off_bl, 3
        vcvtfxpntps2dq  ioff_br, off_br, 3
        vcvtfxpntps2dq  ioff_tl, off_tl, 3
        vcvtfxpntps2dq  ioff_tr, off_tr, 3
# Part 2
..L100: vgatherdps      val_bl{k2}, [I + ioff_bl * 4]
        jkzd            k2, ..L101
        vgatherdps      val_bl{k2}, [I + ioff_bl * 4]
        jknzd           k2, ..L100
..L101: vsubps          oscalx, float_one, scalx
..L200: vgatherdps      val_br{k3}, [I + ioff_br * 4]
        jkzd            k3, ..L201
        vgatherdps      val_br{k3}, [I + ioff_br * 4]
        jknzd           k3, ..L200
..L201: vsubps          oscaly, float_one, scaly
..L300: vgatherdps      val_tl{k4}, [I + ioff_tl * 4]
        jkzd            k4, ..L301
        vgatherdps      val_tl{k4}, [I + ioff_tl * 4]
        jknzd           k4, ..L300
..L301: vaddps          wx, wx, MM
..L400: vgatherdps      val_tr{k5}, [I + ioff_tr * 4]
        jkzd            k5, ..L401
        vgatherdps      val_tr{k5}, [I + ioff_tr * 4]
        jknzd           k5, ..L400
# Part 3
..L401: vmulps          sval_bl, val_bl, oscalx  
        cmp i, N

        vmulps          sval_tl, val_tl, oscalx   

        vfmadd213ps     val_br, scalx, sval_bl    

        vfmadd213ps     val_tr, scalx, sval_tl

        vmulps          sval_b, val_b, oscaly

        vfmadd213ps     val_t, scaly, sval_b

        vfmadd213ps     val, w2_inv, [VOL + r11 * 4]
        vprefetchenta   [VOL + r10 * 4]

        vprefetche2     [VOL + r10 * 4 + 256]
        nop
        vmovaps         [VOL + r11 * 4], val

        jl ..loop
ret
.size fastrabbit, .-fastrabbit
