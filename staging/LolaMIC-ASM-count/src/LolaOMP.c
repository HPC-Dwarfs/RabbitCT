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

/* Allocate variables with correct alignment on stack. */
#ifdef __INTEL_COMPILER
__declspec(align(SIMD_BYTE)) float WIDTH[SIMD_OPS];
__declspec(align(SIMD_BYTE)) float FLOATONE[SIMD_OPS];
#else // __INTEL_COMPILER
#ifdef __GNUC__
#error GCC as of now only supports 16-byte stack alignment!
#else // __GNUC__
#error Compiler not supported!
#endif // __GNUC__
#endif // __INTEL_COMPILER

extern int fastrabbit(const float *,   // I
        float *,                        // VOL
        float *,                        // WIDTH
        float *,                        // ONE
        int,                            // count
        __m512, __m512, __m512,         // A[0], A[1], A[2]
        __m512, __m512, __m512,         // tmp[0], tmp[1], tmp[2]
        __m512,                         // wx
        __m512);                        // mm

static OutShadow shadow;
static LineRange **Range;
static ZeroPadding padding;
static float *paddedImg;
static uint64_t gathers = 0;
static uint64_t voxels = 0;

int RCTLoadAlgorithm(RabbitCtGlobalData * rcgd) {
    return 1;
}

int RCTFinishAlgorithm(RabbitCtGlobalData * rcgd) {
    printf("processed voxels: %llu\n", voxels);
    printf("max. gather invokations: %llu\n", voxels*4);
    printf("gather invokations: %llu\n", gathers);
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

    /* Initialize vector register data */
#pragma unroll
    for (int i = 0; i < SIMD_OPS; ++i) {
        WIDTH[i] = padding.lineOffset;
        FLOATONE[i] = 1.0f;
    }

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

    float *VOL  = rcgd->volumeData;

#pragma omp parallel reduction(+:voxels) reduction(+:gathers)
    {
        for (int p = 0; p < P; ++p) {
            const double* const A = rcgd->projectionBuffer[p].matrix;
            const float*  const I_orig = rcgd->projectionBuffer[p].image;
            const int id = rcgd->projectionBuffer[p].id;
            __m512 _mminit = _mm512_set_ps(15 * MM, 14 * MM, 13 * MM, 12 * MM, 11 *
                    MM, 10 * MM, 9 * MM, 8 * MM, 7 * MM, 6 * MM, 5 * MM, 4 *
                    MM, 3 * MM, 2 * MM, 1 * MM, 0.0f);

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
                    int count = Range[id - 1][z * L + y].stop - start;
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
                    __m512 _wxinit = _mm512_set1_ps(wx);
                    __m512 _wx = _mm512_add_ps(_wxinit, _mminit);
                    __m512 _A0 = _mm512_set1_ps((float)A[0]);
                    __m512 _A1 = _mm512_set1_ps((float)A[1]);
                    __m512 _A2 = _mm512_set1_ps((float)A[2]);
                    __m512 _mm = _mm512_set1_ps(16.0f * MM);
                    __m512 _tmp0 = _mm512_set1_ps(tmp[0]);
                    __m512 _tmp1 = _mm512_set1_ps(tmp[1]);
                    __m512 _tmp2 = _mm512_set1_ps(tmp[2]);

                    int rabbit_gathers;

                    LIKWID_MARKER_START("fastrabbit");
                    rabbit_gathers = fastrabbit(I, VOL + offset, WIDTH, FLOATONE,
                            count, _A0, _A1, _A2, _tmp0, _tmp1, _tmp2, _wx,
                            _mm);
                    LIKWID_MARKER_STOP("fastrabbit");
/*  */
/*                     if (count == L) { */
/*                         static int min = 999999; */
/*                         if (rabbit_gathers < min) { */
/*                             min = rabbit_gathers; */
/*                             printf("p %d z %d y %d: %d\n", id-1, z, y, rabbit_gathers); */
/*                         } */
/*                     } */

                    if (id-1 == 0 && z == 237 && y == 18) {
                        printf("line length: %d\n", count);
                        printf("rabbit_gathers: %d\n", rabbit_gathers);
                    }

                    voxels += count;
                    gathers += rabbit_gathers;

                    /* Measure fastrabbit() runtime in cycles */
#ifdef KERNEL_CYCLES
                    if (count != L)
                        continue;
                    for (int i = 0; i < 50; ++i) {
                        CyclesData cycleData;
                        rabbitTimer_startCycles(&cycleData);

                        fastrabbit(I, VOL + offset, WIDTH, FLOATONE, count,
                                _A0, _A1, _A2, _tmp0, _tmp1, _tmp2, _wx, _mm);

                        rabbitTimer_stopCycles(&cycleData);
                        printf("cycles for one line (%d voxels)/one loop iteration (16 voxels)/one voxel: "
                                "%llu / %f / %f\n", count,
                                LLU_CAST rabbitTimer_printCycles(&cycleData),
                                (float)rabbitTimer_printCycles(&cycleData)/count*16,
                                (float)rabbitTimer_printCycles(&cycleData)/count);
                    }
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
