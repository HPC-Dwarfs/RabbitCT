.intel_syntax noprefix
.text
.globl fastrabbit
.type fastrabbit, @function
fastrabbit:
## Only push/pop callee saved registers (rbp, rsp, r12-r15) we actually use: none

vmovaps zmm8, [rdx]                    # IMAGEWIDTH
vmovaps zmm9, [rcx]                    # ONE

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

#define hit     rax


######   Calculate NXToff_*, NXTscalx, NXTscaly, NXTw2_inv
        vmovaps         w, wx         
        kxnor           k2, k2
        vmovaps         v, wx          
        kxnor           k3, k3
        vmovaps         u, wx           
        kxnor           k4, k4
        vfmadd213ps     w, A2, tmp2      
        kxnor           k5, k5
        vfmadd213ps     u, A0, tmp0       
        vrcp23ps        w_inv, w           
        vfmadd213ps     v, A1, tmp1         
        vmulps          ix, u, w_inv         
        vmulps          iy, v, w_inv          
        vrndfxpntps     iix, ix, 3             
        vrndfxpntps     iiy, iy, 3              
        vmulps          w2_inv, w_inv, w_inv
        vsubps          scaly, iy, iiy          
        vfmadd213ps     iiy, width, iix
        vsubps          scalx, ix, iix
        vaddps          off_tl, width, off_bl
        vaddps          off_br, one, off_bl
        vaddps          off_tr, one, off_tl
        vcvtfxpntps2dq  ioff_bl, off_bl, 3
        vcvtfxpntps2dq  ioff_br, off_br, 3
        vcvtfxpntps2dq  ioff_tl, off_tl, 3
        vcvtfxpntps2dq  ioff_tr, off_tr, 3

xor hit, hit

#vgatherpf0hintdps [I + ioff_bl * 4]{k2}
#vgatherpf0hintdps [I + ioff_br * 4]{k3}
#vgatherpf0hintdps [I + ioff_tl * 4]{k4}
#vgatherpf0hintdps [I + ioff_tr * 4]{k5}

..L110: add hit, 1
        vgatherpf0dps   [I + ioff_bl * 4]{k2}
        jkzd            k2, ..L111
        add hit, 1
        vgatherpf0dps   [I + ioff_bl * 4]{k2}
        jknzd           k2, ..L110
..L111: 
..L210: add hit, 1
        vgatherpf0dps   [I + ioff_br * 4]{k3}
        jkzd            k3, ..L211
        add hit, 1
        vgatherpf0dps   [I + ioff_br * 4]{k3}
        jknzd           k3, ..L210
..L211: 
..L310: add hit, 1
        vgatherpf0dps   [I + ioff_tl * 4]{k4}
        jkzd            k4, ..L311
        add hit, 1
        vgatherpf0dps   [I + ioff_tl * 4]{k4}
        jknzd           k4, ..L310
..L311: 
..L410: add hit, 1
        vgatherpf0dps   [I + ioff_tr * 4]{k5}
        jkzd            k5, ..L411
        add hit, 1
        vgatherpf0dps   [I + ioff_tr * 4]{k5}
        jknzd           k5, ..L410
..L411: 


######   back up NXToff_*, NXTscalx, NXTscaly, NXTw2_inv
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
        jle ..skip

# set COUNTER to zero
xor i, i
        vprefetchenta [VOL]
        vprefetch2     [VOL + 256]
.align 32
..loop:
        vmovaps         w, wx         
        kxnor           k2, k2
        vmovaps         v, wx          
        kxnor           k3, k3
        vmovaps         u, wx           
        kxnor           k4, k4
        vfmadd213ps     w, A2, tmp2      
        kxnor           k5, k5
        vfmadd213ps     u, A0, tmp0       
        mov             r10, VOL
        vrcp23ps        w_inv, w           
        lea             r11, [r9]
        vfmadd213ps     v, A1, tmp1         
        add             i, 16
        vmulps          ix, u, w_inv         
        vmulps          iy, v, w_inv          
        vrndfxpntps     iix, ix, 3             
        vrndfxpntps     iiy, iy, 3              
        vmulps          w2_inv, w_inv, w_inv
        vsubps          scaly, iy, iiy          
        vfmadd213ps     iiy, width, iix
        vsubps          scalx, ix, iix
        vaddps          off_tl, width, off_bl
        vaddps          off_br, one, off_bl
        vaddps          off_tr, one, off_tl
        vcvtfxpntps2dq  ioff_bl, off_bl, 3
        vcvtfxpntps2dq  ioff_br, off_br, 3
        vcvtfxpntps2dq  ioff_tl, off_tl, 3
        vcvtfxpntps2dq  ioff_tr, off_tr, 3

#vgatherpf0hintdps [I + ioff_bl * 4]{k2}
#vgatherpf0hintdps [I + ioff_br * 4]{k3}
#vgatherpf0hintdps [I + ioff_tl * 4]{k4}
#vgatherpf0hintdps [I + ioff_tr * 4]{k5}

..L120: add hit, 1
        vgatherpf0dps   [I + ioff_bl * 4]{k2}
        jkzd            k2, ..L121
        add hit, 1
        vgatherpf0dps   [I + ioff_bl * 4]{k2}
        jknzd           k2, ..L120
..L121: kxnor           k2, k2
..L220: add hit, 1
        vgatherpf0dps   [I + ioff_br * 4]{k3}
        jkzd            k3, ..L221
        add hit, 1
        vgatherpf0dps   [I + ioff_br * 4]{k3}
        jknzd           k3, ..L220
..L221: kxnor           k3, k3
..L320: add hit, 1
        vgatherpf0dps   [I + ioff_tl * 4]{k4}
        jkzd            k4, ..L321
        add hit, 1
        vgatherpf0dps   [I + ioff_tl * 4]{k4}
        jknzd           k4, ..L320
..L321: kxnor           k4, k4
..L420: add hit, 1
        vgatherpf0dps   [I + ioff_tr * 4]{k5}
        jkzd            k5, ..L421
        add hit, 1
        vgatherpf0dps   [I + ioff_tr * 4]{k5}
        jknzd           k5, ..L420
..L421: kxnor           k5, k5

..L100: 
        vgatherdps      val_bl{k2}, [I + NXTioff_bl * 4]
        jkzd            k2, ..L101
       
        vgatherdps      val_bl{k2}, [I + NXTioff_bl * 4]
        jknzd           k2, ..L100
..L101: vsubps          oscalx, one, NXTscalx
..L200: 
        vgatherdps      val_br{k3}, [I + NXTioff_br * 4]
        jkzd            k3, ..L201
       
        vgatherdps      val_br{k3}, [I + NXTioff_br * 4]
        jknzd           k3, ..L200
..L201: vsubps          oscaly, one, NXTscaly
..L300:
        vgatherdps      val_tl{k4}, [I + NXTioff_tl * 4]
        jkzd            k4, ..L301
      
        vgatherdps      val_tl{k4}, [I + NXTioff_tl * 4]
        jknzd           k4, ..L300
..L301: vaddps          wx, wx, mm
..L400:
        vgatherdps      val_tr{k5}, [I + NXTioff_tr * 4]
        jkzd            k5, ..L401
        
        vgatherdps      val_tr{k5}, [I + NXTioff_tr * 4]
        jknzd           k5, ..L400
..L401: vmulps          sval_bl, val_bl, oscalx  
        cmp i, N
        vmulps          sval_tl, val_tl, oscalx   
        vfmadd213ps     val_br, NXTscalx, sval_bl    
        vfmadd213ps     val_tr, NXTscalx, sval_tl
        vmulps          sval_b, val_b, oscaly
        vfmadd213ps     val_t, NXTscaly, sval_b
        vfmadd213ps     val, NXTw2_inv, [r10 + r11 * 4]

        vprefetchenta     [r10 + r9 * 4]
        vprefetch2     [r10 + r9 * 4 + 256]
        nop
        vmovaps         [r10 + r11 * 4], sval

        vmovaps         NXTioff_bl, ioff_bl
        vmovaps         NXTioff_br, ioff_br
        vmovaps         NXTioff_tl, ioff_tl
        vmovaps         NXTioff_tr, ioff_tr
        vmovaps         NXTscalx, scalx
        vmovaps         NXTscaly, scaly
        vmovaps         NXTw2_inv, w2_inv
        jl ..loop
..skip:
        kxnor           k2, k2
        kxnor           k3, k3
        kxnor           k4, k4
        kxnor           k5, k5
..L130:
        vgatherdps      val_bl{k2}, [I + NXTioff_bl * 4]
        jkzd            k2, ..L131
      
        vgatherdps      val_bl{k2}, [I + NXTioff_bl * 4]
        jknzd           k2, ..L130
..L131: vsubps          oscalx, one, NXTscalx
..L230:
        vgatherdps      val_br{k3}, [I + NXTioff_br * 4]
        jkzd            k3, ..L231
       
        vgatherdps      val_br{k3}, [I + NXTioff_br * 4]
        jknzd           k3, ..L230
..L231: vsubps          oscaly, one, NXTscaly
..L330: 
        vgatherdps      val_tl{k4}, [I + NXTioff_tl * 4]
        jkzd            k4, ..L331
      
        vgatherdps      val_tl{k4}, [I + NXTioff_tl * 4]
        jknzd           k4, ..L330
..L331: lea             r11, [r9]
..L430: 
        vgatherdps      val_tr{k5}, [I + NXTioff_tr * 4]
        jkzd            k5, ..L431
       
        vgatherdps      val_tr{k5}, [I + NXTioff_tr * 4]
        jknzd           k5, ..L430
..L431: vmulps          sval_bl, val_bl, oscalx  
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
