#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <omp.h>

extern "C" {
#include <likwid.h>
#include <rabbitHelper_types.h>
#include <rabbitTimer.h>
#include <rabbitCt.h>
#include <analyseGeometry.h>
#include <memoryUtils.h>

void Backprojection(
		int start,
		int stop,
		float MM,         /* voxel size */
		unsigned int imageWidth,
		unsigned int imageHeight,
		float A0,
		float A1,
		float A2,
		float tmp_u,
		float tmp_v,
		float tmp_w,
		float O_Index,
		float *VOL,       /* voxel volume data */
		const float* I          /* image data */
		);

int width();
}

static OutShadow shadow;
static LineRange **Range;
static ZeroPadding padding;
static float *paddedImg;

FNCSIGN bool RCTLoadAlgorithm(RabbitCtGlobalData * rcgd) {
	return true;
}


FNCSIGN bool RCTFinishAlgorithm(RabbitCtGlobalData * rcgd) {
	return true;
}


FNCSIGN bool RCTPrepareAlgorithm(RabbitCtGlobalData * rcgd) {
    const int P = rcgd->numberOfProjections;
    const int L = rcgd->problemSize;

    /* numberOfProjections is set to N in main if -a was given */
    if (rcgd->numberOfProjections == 0) {
        fprintf(stderr,"This module needs global geometry information.\n"
                "Please add -a switch!\n");
        exit(0);
    }

    /* Allocate P * L * L * sizeof(LineRange) bytes of memory:
     * For each projection we need L * L LineRanges */
    if ((Range = (LineRange **)malloc(P * sizeof(LineRange *))) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (int i=0; i<rcgd->numberOfProjections; i++) {
        if ((Range[i] = (LineRange *)malloc(L * L * sizeof(LineRange)))
                == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
    }

    /* Initialize memoryUtils:
     * calls rabbitNuma_init():
     *   i. sets numa_info.numberOfNodes
     *  ii. fills numa_info.nodes[].*Memory with information
     * iii. fills numa_info.nodes[].numberOfProcessors with information
     *  vi. fills numa_info.nodes[].processors[] with information */
    //memoryUtils_init();

    /* Compute the Shadow of the projection.
     *
     * Some rays through our volume might not hit the detector pane.
     * Because we vectorize the voxel updates and don't want to take
     * special care of u-v-coordinates that lie outside the projection
     * image, we zero-pad the original projection image to make sure
     * every voxel will hit valid memory.
     *
     * We might actually improve performance by calculating the shadow more
     * carefully: We currently (really?) calculate the shadow for *all*
     * voxels. However, when we create the clipping mask, we won't be using
     * *all* voxels in the volume, so the padding might be larger than needed.
     * If we calculate the shadow *after* we have created the clipping, and 
     * walk through the clipped volume *including* clipped voxels that we have
     * to include due to vectorization, it should give us a better result! */
    computeLineRanges(rcgd, Range);

    // TODO: remove
    // this is only for debugging purposes
    printf("SIMD width %d\n", width());

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

	return true;
}


FNCSIGN bool RCTUnloadAlgorithm(RabbitCtGlobalData * rcgd)
{
	return true;
}

FNCSIGN bool RCTAlgorithmBackprojection(RabbitCtGlobalData *rcgd) {
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

                    float A0 = A[0];
                    float A1 = A[1];
                    float A2 = A[2];

                    // N.B. In Backprojection() we use foreach (i = start ...
                    // stop) and access VOL[i]. That's why here we don't need
                    // offset += start;
                    unsigned int offset = z * L * L + y * L;

                    /* Precalculate parts of u, v, w that are invariant with
                     * respect to x and store in tmp[] */
                    float tmp[3];
                    tmp[0] = (float)(A[3] * wy + A[6] * wz + A[9]);
                    tmp[1] = (float)(A[4] * wy + A[7] * wz + A[10]);
                    tmp[2] = (float)(A[5] * wy + A[8] * wz + A[11]);


                    LIKWID_MARKER_START("fastrabbit");

                    Backprojection(start, stop, MM, imageWidth, imageHeight, A0,
                            A1, A2, tmp[0], tmp[1], tmp[2], rcgd->O_Index, VOL +
                            offset, I);

                    LIKWID_MARKER_STOP("fastrabbit");

                    /* Measure fastrabbit() runtime in cycles */
#ifdef KERNEL_CYCLES
                    if ((stop - start) != L)
                        continue;
                    for (int i = 0; i < 50; ++i) {
                        CyclesData cycleData;
                        rabbitTimer_startCycles(&cycleData);

                        Backprojection(start, stop, MM, imageWidth, imageHeight, A0,
                                A1, A2, tmp[0], tmp[1], tmp[2], rcgd->O_Index, VOL +
                                offset, I);

                        rabbitTimer_stopCycles(&cycleData);
                        printf("cycles for one line (%d voxels)/one voxel: "
                                "%llu / %f\n", (stop-start),
                                LLU_CAST rabbitTimer_printCycles(&cycleData),
                                (float)rabbitTimer_printCycles(&cycleData)/(stop-start));
                    }
                    fflush(stdout);
                    printf("EXITING\n");
                    exit(0);
#endif
                } // y-loop
            } // z-loop
        } // projection loop
    } // pragma omp parallel
    return true;
}
