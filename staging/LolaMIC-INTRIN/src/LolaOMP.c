#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <immintrin.h>

#include <rabbitCt.h>
#include <rabbitHelper_types.h>
#include <analyseGeometry.h>
#include <memoryUtils.h>
#include <likwid.h>
#ifdef KERNEL_CYCLES
#include <rabbitTimer.h>
#endif

#define     SIMD_BIT  (512)
#define     SIMD_BYTE (SIMD_BIT/8)    
#define     SIMD_OPS  (SIMD_BYTE / sizeof(float))


static OutShadow shadow;
static LineRange **Range;
static ZeroPadding padding;
static float *paddedImg;

int RCTLoadAlgorithm(RabbitCtGlobalData * rcgd) {
    return 1;
}

int RCTFinishAlgorithm(RabbitCtGlobalData * rcgd) {
    return 1;
}

int RCTPrepareAlgorithm(RabbitCtGlobalData * rcgd) {
    /* numberOfProjections is set to N in main if -a was given */
    if (rcgd->numberOfProjections == 0) {
        fprintf(stderr,"This module needs global geometry information.\n"
                "Please add -a switch!\n");
        exit(EXIT_FAILURE);
    }

    /* Allocate N * L * L * sizeof(LineRange) bytes of memory:
     * For each projection we need L * L LineRanges */
    if ((Range = (LineRange **)malloc(rcgd->numberOfProjections *
            sizeof(LineRange *))) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (int i=0; i<rcgd->numberOfProjections; i++) {
        if ((Range[i] = (LineRange *)malloc(rcgd->problemSize *
                        rcgd->problemSize * sizeof(LineRange))) == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
    }

    /* Calculate LineRanges
     *
     * Some rays through our volume might not hit the detector pane.
     * For each projection image, we calculate a x-voxelline-wise clipping
     * mask for our volume.
     */
    computeLineRanges(rcgd, Range);

    /* Compute the Shadow of the projection.
     *
     * Some rays through our volume might not hit the detector pane.
     * Because we vectorize the voxel updates and don't want to take
     * special care of u-v-coordinates that lie outside the projection
     * image, we zero-pad the original projection image to make sure
     * every voxel will hit valid memory.
     */
    computeShadowOfProjection(rcgd, &shadow);
    int padXl = abs(shadow.Umin);
    int padXr = abs(shadow.Umax) - rcgd->imageWidth;
    int padYb = abs(shadow.Vmin);
    int padYt = abs(shadow.Vmax) - rcgd->imageHeight;
    int XSize = padXl + rcgd->imageWidth + padXr;
    int YSize = padYb + rcgd->imageHeight + padYt;
    padding.paddedSize = XSize * YSize ;
    padding.startOffset = padYb * XSize + padXl;
    padding.lineOffset  = XSize;
    if ((paddedImg = (float *)malloc(XSize * YSize * sizeof(float))) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < XSize * YSize; ++i)
        paddedImg[i] = 0.0f;

    /* The thread init for the marker must only be called once by each thread,
     * which is why we do it here. (RCTAlgorithmBackprojection() is called
     * multiple times, so we can't do it there.) */
#pragma omp parallel
    {
        LIKWID_MARKER_THREADINIT;
    }

    return 1;
}


int RCTUnloadAlgorithm(RabbitCtGlobalData * rcgd) {
    return 1;
}

int RCTAlgorithmBackprojection(RabbitCtGlobalData * rcgd) {
    const int L = rcgd->problemSize;
    const int P = rcgd->numberOfProjections; // projections in this run
    const float MM = rcgd->voxelSize;
    const float O = rcgd->O_Index;
    //const float imageWidth = rcgd->imageWidth;
    const float imageWidth = padding.lineOffset;
    const float imageHeight = rcgd->imageHeight;

    float *VOL  = rcgd->volumeData;

#pragma omp parallel
    {
        for (int p = 0; p < P; ++p) {
            const double* const A = rcgd->projectionBuffer[p].matrix;
            const float*  const I_orig = rcgd->projectionBuffer[p].image;
            const int id = rcgd->projectionBuffer[p].id;

            // copy projection image into zero padded buffer
            float *cursor = paddedImg + padding.startOffset;
#pragma omp for schedule(dynamic)
                for (int i = 0; i<rcgd->imageHeight; i++)
                    memcpy(cursor + (i * padding.lineOffset), I_orig + (i * rcgd->imageWidth), rcgd->imageWidth * sizeof(float));
#pragma omp barrier
            const float* const I = cursor;

#pragma omp for schedule(runtime) collapse(2)
            for (int z = 0; z < L; ++z) {
                for (int y = 0; y < L; ++y) {
                    /* Select starting voxel and voxel count in x direction for this
                     * projection image */
                    int start = Range[id - 1][z * L + y].start;
                    int stop = Range[id - 1][z * L + y].stop;
                    if (stop - start == 0)
                        continue;

                    /* Convert from VCS to WCS */
                    float wz = z * MM + O;
                    float wy = y * MM + O;
                    float wx = start * MM + O;

                    /* fastrabbit() receives a ptr to VOL+offset. In fastrabbit()
                     * this pointer is then known as VOL. The additional offset as
                     * fastrabbit() iterates over voxel-vectors is calculated as
                     * VOL + INDEX * 4.
                     *
                     * N.B. INDEX is the addressing part of the register, which is
                     * otherwise used as the loop counter COUNTER. */
                    unsigned int offset = z * L * L + y * L;

                    /* Precalculate parts of u, v, w that are invariant with
                     * respect to x and store in tmp[] */
                    float tmp[3];
                    tmp[0] = (float)(A[3] * wy + A[6] * wz + A[9]);
                    tmp[1] = (float)(A[4] * wy + A[7] * wz + A[10]);
                    tmp[2] = (float)(A[5] * wy + A[8] * wz + A[11]);

                    LIKWID_MARKER_START("fastrabbit");

                    __m512 _wx = _mm512_set_ps((float)wx + 15 * MM, (float)wx + 14 * MM,
                            (float)wx + 13 * MM, (float)wx + 12 * MM,
                            (float)wx + 11 * MM, (float)wx + 10 * MM,
                            (float)wx + 9 * MM, (float)wx + 8 * MM,
                            (float)wx + 7 * MM, (float)wx + 6 * MM,
                            (float)wx + 5 * MM, (float)wx + 4 * MM,
                            (float)wx + 3 * MM, (float)wx + 2 * MM,
                            (float)wx + 1 * MM, (float)wx);
                    __m512 _MM = _mm512_set1_ps(16.0f * MM);
                    __m512 _one = _mm512_set1_ps(1.0f);
                    __m512 _zero = _mm512_setzero_ps();

                    __m512 _tmp_u = _mm512_set1_ps(tmp[0]);
                    __m512 _tmp_v = _mm512_set1_ps(tmp[1]);
                    __m512 _tmp_w = _mm512_set1_ps(tmp[2]);
                    __m512 _A0 = _mm512_set1_ps((float)A[0]);
                    __m512 _A1 = _mm512_set1_ps((float)A[1]);
                    __m512 _A2 = _mm512_set1_ps((float)A[2]);

                    __m512 _width = _mm512_set1_ps(imageWidth);
                    __m512 _height = _mm512_set1_ps(imageHeight);

                    __m512 _valtl, _valtr, _valbl, _valbr;

                    for (int x = start; x < stop; x += SIMD_OPS) {
                        // calc u, v, w, 1/w, 1/w^2
                        __m512 _u = _mm512_fmadd_ps(_wx, _A0, _tmp_u);
                        __m512 _v = _mm512_fmadd_ps(_wx, _A1, _tmp_v);
                        __m512 _w = _mm512_fmadd_ps(_wx, _A2, _tmp_w);
                        __m512 _rcp_w = _mm512_rcp23_ps(_w);
                        __m512 _rcp2_w = _mm512_mul_ps(_rcp_w, _rcp_w);

                        // incr wx for next iteration
                        _wx = _mm512_add_ps(_wx, _MM);

                        // calc ix iy, iix iiy, scalx scaly
                        __m512 _ix = _mm512_mul_ps(_u, _rcp_w);
                        __m512 _iy = _mm512_mul_ps(_v, _rcp_w);
                        __m512 _iix = _mm512_round_ps(_ix, _MM_FROUND_TO_ZERO, _MM_EXPADJ_NONE);
                        __m512 _iiy = _mm512_round_ps(_iy, _MM_FROUND_TO_ZERO, _MM_EXPADJ_NONE);
                        __m512 _scalx = _mm512_sub_ps(_ix, _iix);
                        __m512 _scaly = _mm512_sub_ps(_iy, _iiy);
                        __m512 _1mscalx = _mm512_sub_ps(_one, _scalx);
                        __m512 _1mscaly = _mm512_sub_ps(_one, _scaly);

                        // calc offsets & convert to int

                        __m512 _bl_foffset = _mm512_fmadd_ps(_iiy, _width, _iix);
                        __m512 _br_foffset = _mm512_add_ps(_bl_foffset, _one);
                        __m512 _tl_foffset = _mm512_add_ps(_bl_foffset, _width);
                        __m512 _tr_foffset = _mm512_add_ps(_tl_foffset, _one);

                        __m512i _bl_offset = _mm512_cvtfxpnt_round_adjustps_epi32(_bl_foffset, _MM_FROUND_TO_ZERO, _MM_EXPADJ_NONE);
                        __m512i _br_offset = _mm512_cvtfxpnt_round_adjustps_epi32(_br_foffset, _MM_FROUND_TO_ZERO, _MM_EXPADJ_NONE);
                        __m512i _tl_offset = _mm512_cvtfxpnt_round_adjustps_epi32(_tl_foffset, _MM_FROUND_TO_ZERO, _MM_EXPADJ_NONE);
                        __m512i _tr_offset = _mm512_cvtfxpnt_round_adjustps_epi32(_tr_foffset, _MM_FROUND_TO_ZERO, _MM_EXPADJ_NONE);

                        // gather
                        __m512 _valtl, _valtr, _valbl, _valbr;
                        _valtl = _mm512_i32gather_ps(_tl_offset, I, 4);
                        _valtr = _mm512_i32gather_ps(_tr_offset, I, 4);
                        _valbl = _mm512_i32gather_ps(_bl_offset, I, 4);
                        _valbr = _mm512_i32gather_ps(_br_offset, I, 4);

#if 0
                        // FIXME use FMA
                        _valtl = _mm512_mul_ps(_valtl, _1mscalx);
                        _valtr = _mm512_mul_ps(_valtr, _scalx);
                        _valbl = _mm512_mul_ps(_valbl, _1mscalx);
                        _valbr = _mm512_mul_ps(_valbr, _scalx);
                        
                        __m512 _valt, _valb;
                        _valt = _mm512_add_ps(_valtl, _valtr);
                        _valb = _mm512_add_ps(_valbl, _valbr);

                        // TODO: check if this is correct 
                        _valt = _mm512_mul_ps(_valt, _scaly);
                        _valb = _mm512_mul_ps(_valb, _1mscaly);
#endif

                        _valtl = _mm512_mul_ps(_valtl, _1mscalx);
                        _valbl = _mm512_mul_ps(_valbl, _1mscalx);
                        __m512 _valt, _valb;
                        _valt = _mm512_fmadd_ps(_valtr, _scalx, _valtl);
                        _valb = _mm512_fmadd_ps(_valbr, _scalx, _valbl);
                        _valt = _mm512_mul_ps(_valt, _scaly);

                        __m512 _val;
                        _val = _mm512_fmadd_ps(_valb, _1mscaly, _valt);

                        //_val = _mm512_add_ps(_valb, _valt);
#if 0
                        _val = _mm512_mul_ps(_val, _rcp2_w);

                        __m512 _sum;
                        _sum = _mm512_load_ps(&VOL[offset + x]);
                        _sum = _mm512_add_ps(_sum, _val);
#endif
                        __m512 _sum, _vol;
                        _vol = _mm512_load_ps(&VOL[offset + x]);
                        _sum = _mm512_fmadd_ps(_val, _rcp2_w, _vol);

                        _mm512_store_ps(&VOL[offset + x], _sum);
                    }

                    /* Measure fastrabbit() runtime in cycles */
#ifdef KERNEL_CYCLES
                    // only measure a complete line
                    if (stop - start != L)
                        continue;
                    for (int i = 0; i < 50; ++i) {
                        CyclesData cycleData;
                        rabbitTimer_startCycles(&cycleData);

                        for (int x = start; x < stop; x += SIMD_OPS) {
                            // calc u, v, w, 1/w, 1/w^2
                            __m512 _u = _mm512_summadd_ps(_wx, _A0, _tmp_u);
                            __m512 _v = _mm512_summadd_ps(_wx, _A1, _tmp_v);
                            __m512 _w = _mm512_summadd_ps(_wx, _A2, _tmp_w);
                            __m512 _rcp_w = _mm512_rcp23_ps(_w);
                            _wx = _mm512_add_ps(_wx, _MM);
                            __m512 _rcp2_w = _mm512_mul_ps(_rcpw, _rcpw);

                            // calc ix iy, iix iiy, scalx scaly
                            __m512 _ix = _mm512_mul_ps(_u, _rcpw);
                            __m512 _iy = _mm512_mul_ps(_v, _rcpw);
                            __m512 _iix = _mm512_round_ps(_ix, _MM_sumROUND_TO_ZERO, _MM_EXPADJ_NONE);
                            __m512 _iiy = _mm512_round_ps(_iy, _MM_sumROUND_TO_ZERO, _MM_EXPADJ_NONE);
                            /* __m512 _iix1 = _mm512_add_ps(_iix, _one); */
                            /* __m512 _iiy1 = _mm512_add_ps(_iiy, _one); */

                            __m512 _scalx = _mm512_sub_ps(_ix, _iix);
                            __m512 _scaly = _mm512_sub_ps(_iy, _iiy);
                            __m512 _1mscalx = _mm512_sub_ps(_one, _scalx);
                            __m512 _1mscaly = _mm512_sub_ps(_one, _scaly);

                            // calc offsets & convert to int
                            __m512 _valbl_sumoffset = _mm512_summadd_ps(_iiy, _width, _iix);
                            __m512 _valbr_sumoffset = _mm512_add_ps(_valbl_sumoffset, _one);
                            __m512 _valtl_sumoffset = _mm512_add_ps(_valbl_sumoffset, _width);
                            __m512 _valtr_sumoffset = _mm512_add_ps(_valtl_sumoffset, _one);

                            __m512i _valbl_offset, _valbr_offset, _valtl_offset, _valtr_offset;
                            _valbl_offset = _mm512_cvtfxpnt_round_adjustps_epi32(_valbl_sumoffset, _MM_sumROUND_TO_ZERO, _MM_EXPADJ_NONE);
                            _valbr_offset = _mm512_cvtfxpnt_round_adjustps_epi32(_valbr_sumoffset, _MM_sumROUND_TO_ZERO, _MM_EXPADJ_NONE);
                            _valtl_offset = _mm512_cvtfxpnt_round_adjustps_epi32(_valtl_sumoffset, _MM_sumROUND_TO_ZERO, _MM_EXPADJ_NONE);
                            _valtr_offset = _mm512_cvtfxpnt_round_adjustps_epi32(_valtr_sumoffset, _MM_sumROUND_TO_ZERO, _MM_EXPADJ_NONE);

                            // gather
                            __m512 _valtl, _valtr, _valbl, _valbr;
                            _valtl = _mm512_i32gather_ps(_valtl_offset, I, 4);
                            _valtr = _mm512_i32gather_ps(_valtr_offset, I, 4);
                            _valbl = _mm512_i32gather_ps(_valbl_offset, I, 4);
                            _valbr = _mm512_i32gather_ps(_valbr_offset, I, 4);

/* #if 0 */
                            // FIXME use FMA
                            _valtl = _mm512_mul_ps(_valtl, _1mscalx);
                            _valtr = _mm512_mul_ps(_valtr, _scalx);
                            _valbl = _mm512_mul_ps(_valbl, _1mscalx);
                            _valbr = _mm512_mul_ps(_valbr, _scalx);

                            __m512 _valt, _valb;
                            _valt = _mm512_add_ps(_valtl, _valtr);
                            _valb = _mm512_add_ps(_valbl, _valbr);

                            // TODO: check if this is correct 
                            _valt = _mm512_mul_ps(_valt, _scaly);
                            _valb = _mm512_mul_ps(_valb, _1mscaly);
/* #endif */
                            /* _valtl = _mm512_mul_ps(_valtl, _1mscalx); */
                            /* _valbl = _mm512_mul_ps(_valbl, _1mscalx); */
                            /* __m512 _valt, _valb; */
                            /* _valt = _mm512_summadd_ps(_valtr, _scalx, _valtl); */
                            /* _valb = _mm512_summadd_ps(_valbr, _scalx, _valbl); */

                            __m512 _val;
                            _val = _mm512_add_ps(_valb, _valt);
                            _val = _mm512_mul_ps(_val, _rcpw2);

                            __m512 _sum;
                            _sum = _mm512_load_ps(&VOL[offset + x]);
                            _sum = _mm512_add_ps(_sum, _val);

                            _mm512_store_ps(&VOL[offset + x], _sum);
                        }

                        rabbitTimer_stopCycles(&cycleData);
                        printf("cycles for one line (%d voxels)/one voxel: "
                                "%llu / %f\n", count,
                                LLU_CAST rabbitTimer_printCycles(&cycleData),
                                (float)rabbitTimer_printCycles(&cycleData)/count);
                    }
                    printf("stats for projection p = %d at voxel coordinates z"
                            "= %d y = %d\n", id-1, z, y);
                    fflush(stdout);
                    printf("EXITING\n");
                    exit(0);
#endif
                } // y-loop
            } // z-loop
        } // projection loop
    } // pragma omp parallel
    return 1;
}
