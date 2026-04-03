#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <xmmintrin.h>

#include <rabbitCt.h>
#include <rabbitHelper_types.h>
#include <analyseGeometry.h>
#include <memoryUtils.h>
#include <likwid.h>
#ifdef KERNEL_CYCLES
#include <rabbitTimer.h>
#endif

#define     SIMD_BIT  (128)
#define     SIMD_BYTE (SIMD_BIT/8)    
#define     SIMD_OPS  (SIMD_BYTE / sizeof(float))

#ifdef __INTEL_COMPILER
__declspec(align(SIMD_BYTE)) float WIDTH_VEC[SIMD_OPS];
#else
#error Compiler not supported!
#endif

extern void fastrabbit(float **,        // APTR, pointers to A[i] vectors
        const float *,                  // I, projection image
        float **,                       // TMP, pointers to tmp[i]
        float *,                        // VOL+offset
        float *,                        // WX_VEC
        float *,                        // MM_VEC
        float *,                        // WIDTH_VEC
        int,                            // count
        __m128, __m128, __m128,         // A[0], A[1], A[2]
        __m128, __m128, __m128,         // tmp[0], tmp[1], tmp[2]
        __m128,                         // wx
        __m128                          // mm
        ); 

OutShadow shadow;
static ZeroPadding padding;
static LineRange **Range;
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

    int imageWidth = padding.lineOffset;
    /* Initialize Vector register data */
#pragma unroll
    for (int i = 0; i < SIMD_OPS; ++i) {
        WIDTH_VEC[i] = (float)imageWidth;
    }

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
#pragma omp for schedule(static)
            for (int i = 0; i<rcgd->imageHeight; i++)
                memcpy(cursor + (i * padding.lineOffset), I_orig + (i * rcgd->imageWidth), rcgd->imageWidth * sizeof(float));
#pragma omp barrier
            const float* const I = cursor;
            
            __m128 _mminit = _mm_set_ps(3*MM, 2*MM, MM, 0.0f);

#pragma omp for schedule(runtime) collapse(2)
            for (int z = 0; z < L; ++z) {
                for (int y = 0; y < L; ++y) {
                    /* Select starting voxel and voxel count in x direction for this
                     * projection image */

                    int start = Range[id - 1][z * L + y].start;
                    int count = Range[id - 1][z * L + y].stop - start;
                    /* if (id-1==0) */
                    /*     printf("start: %d stop: %d (z: %d y: %d)\n", start, Range[id - 1][z * L + y].stop, z, y); */
                    if (count == 0)
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
                    unsigned int offset = z * L * L + y * L + start;

                    /* Precalculate parts of u, v, w that are invariant with
                     * respect to x and store in tmp[] */
                    float tmp[3];
                    tmp[0] = (float)(A[3] * wy + A[6] * wz + A[9]);
                    tmp[1] = (float)(A[4] * wy + A[7] * wz + A[10]);
                    tmp[2] = (float)(A[5] * wy + A[8] * wz + A[11]);

                    /* Fill arguments */
                    __m128 _wxinit = _mm_set1_ps(wx);
                    __m128 _wx = _mm_add_ps(_wxinit, _mminit);
                    __m128 _A0 = _mm_set1_ps((float)A[0]);
                    __m128 _A1 = _mm_set1_ps((float)A[1]);
                    __m128 _A2 = _mm_set1_ps((float)A[2]);
                    __m128 _mm = _mm_set1_ps(4.0f * MM);
                    __m128 _tmp0 = _mm_set1_ps(tmp[0]);
                    __m128 _tmp1 = _mm_set1_ps(tmp[1]);
                    __m128 _tmp2 = _mm_set1_ps(tmp[2]);


                    /* We only have 16 ymm registers available. 8 are being used to
                     * pass arguments. Because we need more "room" than 8 ymm
                     * registers to perform the line update, we need to back up
                     * some of the 8 arguments onto the stack. That's why we
                     * allocate the aligned variables here. NB: We also have to
                     * pass the addresses of this "backup space" to the function.
                     * */
#ifdef __INTEL_COMPILER
                    __declspec(align(SIMD_BYTE)) float WX_VEC[SIMD_OPS];
                    __declspec(align(SIMD_BYTE)) float MM_VEC[SIMD_OPS];
                    __declspec(align(SIMD_BYTE)) float A0_VEC[SIMD_OPS];
                    __declspec(align(SIMD_BYTE)) float A1_VEC[SIMD_OPS];
                    __declspec(align(SIMD_BYTE)) float A2_VEC[SIMD_OPS];
                    __declspec(align(SIMD_BYTE)) float TMP0_VEC[SIMD_OPS];
                    __declspec(align(SIMD_BYTE)) float TMP1_VEC[SIMD_OPS];
                    __declspec(align(SIMD_BYTE)) float TMP2_VEC[SIMD_OPS];
#else
#error Compiler not supported!
#endif

                    /* Contains three pointers.
                     * Each one points to VECTORSIZE elements
                     * of A[0], A[1], and A[2]. */
                    float *APTR[3];
                    APTR[0] = A0_VEC;   // later filled with A[0], A[0], ...
                    APTR[1] = A1_VEC;   // later filled with A[1], A[1], ...
                    APTR[2] = A2_VEC;   // later filled with A[2], A[2], ...

                    /* Contains three pointers.
                     * Each one points to VECTORSIZE elements
                     * of tmp[0], tmp[1], and tmp[2]. */
                    float *TMP[3];
                    TMP[0] = TMP0_VEC;// later filled with tmp[0],tmp[0],...
                    TMP[1] = TMP1_VEC;// later filled with tmp[1],tmp[1],...
                    TMP[2] = TMP2_VEC;// later filled with tmp[2],tmp[2],...

                    LIKWID_MARKER_START("fastrabbit");
                    fastrabbit(APTR,        // pointers to A[i] vectors
                            I,              // projection image
                            TMP,            // temp component ptrs
                            VOL + offset,   // Volume image ptr
                            WX_VEC,         // float ptr
                            MM_VEC,         // float ptr
                            WIDTH_VEC,      // integer constant
                            count,          // stopping criterion
                            _A0, _A1, _A2,  // a[i]
                            _tmp0, _tmp1, _tmp2,    // tmp_ix, tmp_iy, tmp_iz
                            _wx,            // wx
                            _mm             // increment
                            );         // stopping criterion
                    LIKWID_MARKER_STOP("fastrabbit");

                } // y-loop
            } // z-loop
        } // p-loop
    } // pragma omp parallel
    return 1;
}
