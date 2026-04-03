#define A    ARG1
#define I    ARG2
#define TMP  ARG3 
#define VOL  ARG4
#define WX   ARG5
#define MM   ARG6
#define ISX  ARG7
#define L    ARG8

#define WXC  FPR11
#define W_IN FPR12
#define TMP0 FPR13
#define TMP1 FPR14
#define TMP2 FPR15
#define ONE  FPR16

#define COUNTER  ebx
#define INDEX  rbx
#define END  edx
#define TMPA  GPR10
#define TMPB  GPR11
#define A0  GPR12
#define A1  GPR13
#define A2  GPR14


#Arguments in rdi,rsi,rdx, rcx, r8, r9

START LOCAL

FUNC fastrabbit
{
# store array pointers in registers
mov      A0, [A]
mov      A1, [A+8]
mov      A2, [A+16]
mov      TMPA, [TMP]
movaps   TMP0, [TMPA]
mov      TMPB, [TMP+8]
movaps   TMP1, [TMPB]
mov      GPR1, [TMP+16]
movaps   TMP2, [GPR1]
movaps   WXC, [WX]
mov      END, L

# generate vector constant 1.0
pcmpeqw ONE,ONE
pslld   ONE,25
psrld   ONE,2
xor   r8, r8
xor   rdi, rdi


xor   COUNTER, COUNTER
.align 16
1:
#ifdef CODE_ANALYSER
mov ebx, 111
.byte 0x64, 0x67, 0x90
#endif
# BLOCK 1 chain -> FPR1 (u), FPR2 (v), FPR3 (w)
movaps FPR1, [A0]
movaps FPR2, [A1]
movaps FPR3, [A2]

mulps  FPR3, WXC
addps  FPR3, TMP2
mulps  FPR1, WXC
addps  FPR1, TMP0
mulps  FPR2, WXC
addps  FPR2, TMP1

#movaps   W_IN, ONE
# 1.0/W
# Use RCPPS and Newton-Raphson Iteration
#divps  W_IN, FPR3
rcpps  W_IN, FPR3
#mulps  FPR3, W_IN
#mulps  FPR3, W_IN
#addps  W_IN, W_IN
#subps  W_IN, FPR3

#increment WX
addps  WXC, [MM]

# BLOCK 2
# IX = U *  W_inv
# IY = V *  W_inv
mulps  FPR1, W_IN
mulps  FPR2, W_IN

# BLOCK 3
#iix -> FPR3, iiy -> FPR4
roundps  FPR3, FPR1, 0x03
roundps  FPR4, FPR2, 0x03

# BLOCK 4
#scalx, scaly
subps  FPR2, FPR4

movaps   FPR6, ONE
# BLOCK 5
# gather on the arrays
mov      GPR1, ISX
mulps    FPR4, [GPR1]
#valb
movaps   FPR5, [GPR1]
addps    FPR5, FPR4
addps    FPR5, FPR3
#valt
addps    FPR4, FPR3
movaps  FPR9, ONE

#scalx
subps  FPR1, FPR3

# duplicate scaly iteration 0 and 2
movsldup  FPR7, FPR2

# IN:
# FPR1 -> scalx
# FPR2 -> scaly
# FPR4 -> Index valt
# FPR5 -> Index valb
# FPR6 -> ONE
# FPR7 -> duplicate 0,2 scaly

# USE
# FPR3 -> valt term
# FPR6 -> valb term

# valt 0,2 -> FPR3
# convert index valt iteration 0
# load valtl and valtr iteration 0
cvtss2si GPR1, FPR4
psrldq    FPR4, 0x4
movlps   FPR3, [I + GPR1 * 4]

subps    FPR6, FPR7

# convert index valt iteration 1
cvtss2si  rdi, FPR4

# load valtl and valtr iteration 2
# convert index valt iteration 2
psrldq    FPR4, 0x4
cvtss2si  GPR1, FPR4
movhps   FPR3, [I + GPR1 * 4]
mulps    FPR3, FPR6

# valb 0,2 -> FPR3
# convert index valb iteration 0
# load valbl and valbr iteration 0
cvtss2si  GPR1, FPR5
psrldq     FPR5, 0x4
movlps    FPR6, [I + GPR1 * 4]

# convert index valb iteration 1
cvtss2si  r8, FPR5

# load valbl and valbr iteration 2
psrldq    FPR5, 0x4
cvtss2si  GPR1, FPR5
movhps    FPR6, [I + GPR1 * 4]

mulps     FPR6, FPR7
movshdup  FPR7, FPR2
addps     FPR6, FPR3
# duplicate scaly iteration 1 and 3
subps     FPR9, FPR7
mulps    W_IN, W_IN

# IN:
# FPR1 -> scalx
# FPR7 -> duplicate 1,3 scaly
# FPR9 -> ONE
# OUT:
# FPR6 -> val  0 2
# USE
# FPR8 -> valt term
# FPR2 -> valb term -> val 1 3

###############################################
# load valtl and valtr iteration 1
movlps   FPR8, [I + rdi * 4]

# load valtl and valtr iteration 3
psrldq    FPR4, 0x4
cvtss2si  GPR1, FPR4
movhps    FPR8, [I + GPR1 * 4]

mulps     FPR8, FPR9

# convert index valb iteration 1
movlps    FPR2, [I + r8 * 4]

# convert index valb iteration 3
psrldq    FPR5, 0x4
cvtss2si GPR1, FPR5
movhps   FPR2, [I + GPR1 * 4]

mulps    FPR2, FPR7
addps    FPR2, FPR8

# BLOCK 6  iteration 1 and 3

# IN:
# FPR1 -> scalx
# FPR6 -> val 0 2
# FPR2 -> val 1 3
movshdup  FPR8, FPR6
movaps  FPR9, ONE

# unpack vall -> FPR3
movaps    FPR5, FPR2

movsldup  FPR3, FPR2
blendps   FPR3, FPR6, 0x5

# unpack valr -> FPR5
blendps   FPR5, FPR8, 0x5

#vall -> FPR3
#valr -> FPR5

# IN: 
# FPR1 -> scalx
# FPR3 -> vall
# FPR5 -> valr
# BLOCK 7
# compute fx
mulps     FPR5, FPR1
subps     FPR9, FPR1
mulps     FPR9, FPR3
addps     FPR9, FPR5

# BLOCK 8
mulps     FPR9, W_IN
addps     FPR9, [VOL + INDEX * 4]
movaps    [VOL + INDEX * 4], FPR9

add       COUNTER, 4
cmp       COUNTER, END
jl 1b
#ifdef CODE_ANALYSER
mov ebx, 222
.byte 0x64, 0x67, 0x90
#endif
}

STOP LOCAL

