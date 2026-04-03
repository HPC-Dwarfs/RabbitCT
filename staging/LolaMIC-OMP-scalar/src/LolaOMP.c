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

                    unsigned int offset = z * L * L + y * L;

                    /* Precalculate parts of u, v, w that are invariant with
                     * respect to x and store in tmp[] */
                    float tmp[3];
                    tmp[0] = (float)(A[3] * wy + A[6] * wz + A[9]);
                    tmp[1] = (float)(A[4] * wy + A[7] * wz + A[10]);
                    tmp[2] = (float)(A[5] * wy + A[8] * wz + A[11]);

                    LIKWID_MARKER_START("fastrabbit");

                    for (int x = start; x < stop; ++x) {
                        float u = tmp[0] + wx * A[0];
                        float v = tmp[1] + wx * A[1];
                        float w = tmp[2] + wx * A[2];
                        float rcp_w = 1.0f/w;
                        float rcp2_w = rcp_w * rcp_w;

                        float ix = u*rcp_w;
                        float iy = v*rcp_w;

                        int iix = (int)ix;
                        int iiy = (int)iy;

                        float scalx = ix - iix;
                        float scaly = iy - iiy;

                        wx += MM;

                        float valbl = I[iiy * padding.lineOffset + iix];
                        float valbr = I[iiy * padding.lineOffset + iix+1];
                        float valtl = I[(iiy+1) * padding.lineOffset + iix];
                        float valtr = I[(iiy+1) * padding.lineOffset + iix+1];

                        float valb = (1-scalx) * valbl + scalx * valbr;
                        float valt = (1-scalx) * valtl + scalx * valtr;
                        float val = (1-scaly) * valb + scaly * valt;
                        float wval = val * rcp2_w;

                        VOL[offset + x] += wval;
                    }

                    /* Measure fastrabbit() runtime in cycles */
#ifdef KERNEL_CYCLES
                    // only measure a complete line
                    if (stop - start != L)
                        continue;
                    for (int i = 0; i < 50; ++i) {
                        CyclesData cycleData;
                        rabbitTimer_startCycles(&cycleData);


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
