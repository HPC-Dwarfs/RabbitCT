// AARCH64 NEON (128-bit, 4-wide) backprojection kernel.
// Port of fastRabbitSSE.S to ARM NEON with optimizations.
//
// AAPCS64 calling convention (all 8 arguments in registers):
//   x0 = APTR   (float*[3] -> A0, A1, A2 vectors, 16 bytes each)
//   x1 = I      (const float*, padded projection image)
//   x2 = TMP    (float*[3] -> tmp0, tmp1, tmp2 vectors, 16 bytes each)
//   x3 = VOL    (float*, volume output pointer)
//   x4 = WX     (float*, 4-element wx vector)
//   x5 = MM4    (float*, 4-element step vector = VECTORSIZE*mm)
//   x6 = ISX4   (float*, 4-element image-stride vector)
//   x7 = count  (int, number of voxels to process, multiple of 4)
//
// Register plan (persistent across iterations):
//   v20 = tmp0, v21 = tmp1, v22 = tmp2
//   v23 = wx (current x-positions, advanced each iteration)
//   v24 = mm4 (step vector, constant)
//   v25 = 1.0f broadcast
//   x19 = &A0, x20 = &A1, x21 = &A2
//   x22 = &ISX4
//   x1  = I (image base, preserved)
//   x3  = VOL (post-incremented each iteration)
//   w7  = count (decremented as loop counter)

#ifdef __APPLE__
    .globl _fastrabbit
#else
    .text
    .globl fastrabbit
    .type fastrabbit, %function
#endif

#ifdef __APPLE__
_fastrabbit:
#else
fastrabbit:
#endif
    // Save callee-saved registers
    stp     x19, x20, [sp, #-48]!
    stp     x21, x22, [sp, #16]
    stp     x23, x24, [sp, #32]

    // Dereference APTR pointer array
    ldr     x19, [x0]              // x19 = APTR[0] = &A0
    ldr     x20, [x0, #8]          // x20 = APTR[1] = &A1
    ldr     x21, [x0, #16]         // x21 = APTR[2] = &A2

    // Load TMP vectors into persistent registers
    ldr     x9, [x2]
    ldr     q20, [x9]              // v20 = tmp0
    ldr     x9, [x2, #8]
    ldr     q21, [x9]              // v21 = tmp1
    ldr     x9, [x2, #16]
    ldr     q22, [x9]              // v22 = tmp2

    // Load wx and mm4 step
    ldr     q23, [x4]              // v23 = wx
    ldr     q24, [x5]              // v24 = mm4 step

    // ISX4 pointer
    mov     x22, x6                // x22 = ISX4 pointer

    // Broadcast 1.0f
    fmov    v25.4s, #1.0           // v25 = {1.0, 1.0, 1.0, 1.0}

    .p2align 4
.Lloop:
    // --- Phase 1: u = A0*wx + tmp0, v = A1*wx + tmp1, w = A2*wx + tmp2 ---
    ldr     q16, [x19]             // v16 = A0
    ldr     q17, [x20]             // v17 = A1
    ldr     q18, [x21]             // v18 = A2
    mov     v0.16b, v20.16b        // v0 = tmp0
    mov     v1.16b, v21.16b        // v1 = tmp1
    mov     v2.16b, v22.16b        // v2 = tmp2
    fmla    v0.4s, v16.4s, v23.4s  // u = tmp0 + A0*wx
    fmla    v1.4s, v17.4s, v23.4s  // v = tmp1 + A1*wx
    fmla    v2.4s, v18.4s, v23.4s  // w = tmp2 + A2*wx

    // --- Phase 2: w_inv = 1/w via Newton-Raphson (2 iterations) ---
    frecpe  v3.4s, v2.4s           // v3 ~= 1/w (8-bit estimate)
    frecps  v4.4s, v3.4s, v2.4s    // v4 = 2 - v3*w
    fmul    v3.4s, v3.4s, v4.4s    // v3 = refined (~17 bits)
    frecps  v4.4s, v3.4s, v2.4s    // v4 = 2 - v3*w
    fmul    v3.4s, v3.4s, v4.4s    // v3 = w_inv (full precision)

    // --- Phase 3: pixel coordinates ---
    fmul    v0.4s, v0.4s, v3.4s    // ix = u * w_inv
    fmul    v1.4s, v1.4s, v3.4s    // iy = v * w_inv

    // w_inv^2 (compute early to hide latency during gather)
    fmul    v19.4s, v3.4s, v3.4s   // v19 = w_inv^2

    // --- Phase 4: truncate and fractional parts ---
    fcvtzs  v4.4s, v0.4s           // iix = trunc(ix) as int
    fcvtzs  v5.4s, v1.4s           // iiy = trunc(iy) as int
    scvtf   v4.4s, v4.4s           // iix back to float
    scvtf   v5.4s, v5.4s           // iiy back to float
    fsub    v6.4s, v0.4s, v4.4s    // scalx = ix - iix
    fsub    v7.4s, v1.4s, v5.4s    // scaly = iy - iiy

    // --- Phase 5: bilinear sample offsets ---
    ldr     q16, [x22]             // v16 = ISX4 (image width)
    fmla    v4.4s, v5.4s, v16.4s   // v4 = iix + iiy*width = off_bl
    fadd    v5.4s, v4.4s, v16.4s   // v5 = off_bl + width = off_tl
    fadd    v17.4s, v4.4s, v25.4s  // v17 = off_bl + 1 = off_br
    fadd    v18.4s, v5.4s, v25.4s  // v18 = off_tl + 1 = off_tr

    // Convert to integer indices
    fcvtzs  v4.4s, v4.4s           // ioff_bl
    fcvtzs  v5.4s, v5.4s           // ioff_tl
    fcvtzs  v17.4s, v17.4s         // ioff_br
    fcvtzs  v18.4s, v18.4s         // ioff_tr

    // --- Phase 6: gather pixel values (4 scalar loads per corner) ---

    // Advance wx now (independent of gather, fills pipeline)
    fadd    v23.4s, v23.4s, v24.4s // wx += mm4

    // Gather bottom-left -> v26
    mov     w9, v4.s[0]
    ldr     s26, [x1, w9, sxtw #2]
    mov     w9, v4.s[1]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v26.s[1], v16.s[0]
    mov     w9, v4.s[2]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v26.s[2], v16.s[0]
    mov     w9, v4.s[3]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v26.s[3], v16.s[0]

    // Gather top-left -> v27
    mov     w9, v5.s[0]
    ldr     s27, [x1, w9, sxtw #2]
    mov     w9, v5.s[1]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v27.s[1], v16.s[0]
    mov     w9, v5.s[2]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v27.s[2], v16.s[0]
    mov     w9, v5.s[3]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v27.s[3], v16.s[0]

    // Gather bottom-right -> v28
    mov     w9, v17.s[0]
    ldr     s28, [x1, w9, sxtw #2]
    mov     w9, v17.s[1]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v28.s[1], v16.s[0]
    mov     w9, v17.s[2]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v28.s[2], v16.s[0]
    mov     w9, v17.s[3]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v28.s[3], v16.s[0]

    // Gather top-right -> v29
    mov     w9, v18.s[0]
    ldr     s29, [x1, w9, sxtw #2]
    mov     w9, v18.s[1]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v29.s[1], v16.s[0]
    mov     w9, v18.s[2]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v29.s[2], v16.s[0]
    mov     w9, v18.s[3]
    ldr     s16, [x1, w9, sxtw #2]
    mov     v29.s[3], v16.s[0]

    // --- Phase 7: bilinear interpolation ---
    // v26=bl, v27=tl, v28=br, v29=tr, v6=scalx, v7=scaly, v25=1.0
    fsub    v16.4s, v25.4s, v6.4s   // oscalx = 1 - scalx
    fsub    v17.4s, v25.4s, v7.4s   // oscaly = 1 - scaly

    // Bottom row: val_b = bl*oscalx + br*scalx
    fmul    v26.4s, v26.4s, v16.4s  // bl * oscalx
    fmla    v26.4s, v28.4s, v6.4s   // val_b = bl*oscalx + br*scalx

    // Top row: val_t = tl*oscalx + tr*scalx
    fmul    v27.4s, v27.4s, v16.4s  // tl * oscalx
    fmla    v27.4s, v29.4s, v6.4s   // val_t = tl*oscalx + tr*scalx

    // Vertical blend: val = val_b*oscaly + val_t*scaly
    fmul    v26.4s, v26.4s, v17.4s  // val_b * oscaly
    fmla    v26.4s, v27.4s, v7.4s   // val = val_b*oscaly + val_t*scaly

    // --- Phase 8: weight by 1/w^2 and accumulate ---
    fmul    v26.4s, v26.4s, v19.4s  // val * w_inv^2

    ldr     q16, [x3]               // load current volume
    fadd    v16.4s, v16.4s, v26.4s  // accumulate
    str     q16, [x3], #16          // store and advance VOL by 16 bytes

    // --- Phase 9: loop ---
    subs    w7, w7, #4              // count -= 4
    b.gt    .Lloop

    // Epilogue: restore callee-saved registers
    ldp     x23, x24, [sp, #32]
    ldp     x21, x22, [sp, #16]
    ldp     x19, x20, [sp], #48
    ret

#ifndef __APPLE__
    .size fastrabbit, .-fastrabbit
#endif

