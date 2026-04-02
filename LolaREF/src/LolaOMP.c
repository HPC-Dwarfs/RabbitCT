#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include <analyseGeometry.h>
#include <likwid.h>
#include <memoryUtils.h>
#include <rabbitCt.h>
#include <rabbitHelper_types.h>
#ifdef KERNEL_CYCLES
#include <rabbitTimer.h>
#endif

#define SIMD_BIT (512)
#define SIMD_BYTE (SIMD_BIT / 8)
#define SIMD_OPS (SIMD_BYTE / sizeof(float))

static OutShadow shadow;
static LineRange **Range;
static ZeroPadding padding;
static float *paddedImg;
static uint64_t voxels_clipping = 0, voxels_actual = 0;

int RCTLoadAlgorithm(RabbitCtGlobalData *rcgd) { return 1; }

int RCTFinishAlgorithm(RabbitCtGlobalData *rcgd) {
  const int L = rcgd->problemSize;
  uint64_t total = (uint64_t)L * (uint64_t)L * (uint64_t)L * 496UL;
  printf("RCTFinishAlgorithm(): clipping stats:\n");
  printf("Total Volume Voxels: %lu\n", total);
  printf("Clipped Volume Voxels: %lu\n", voxels_clipping);
  printf("Actual Volume Voxels: %lu\n", voxels_actual);
}

int RCTPrepareAlgorithm(RabbitCtGlobalData *rcgd) {
  /* numberOfProjections is set to N in main if -a was given */
  if (rcgd->numberOfProjections == 0) {
    fprintf(stderr, "This module needs global geometry information.\n"
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
  for (int i = 0; i < rcgd->numberOfProjections; i++) {
    if ((Range[i] = (LineRange *)malloc(rcgd->problemSize * rcgd->problemSize *
                                        sizeof(LineRange))) == NULL) {
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

  return 1;
}

int RCTUnloadAlgorithm(RabbitCtGlobalData *rcgd) { return 1; }

int RCTAlgorithmBackprojection(RabbitCtGlobalData *rcgd) {
  const int L = rcgd->problemSize;
  const int P = rcgd->numberOfProjections; // projections in this run
  const float MM = rcgd->voxelSize;
  const float O = rcgd->O_Index;
  const float imageWidth = rcgd->imageWidth;
  const float imageHeight = rcgd->imageHeight;

  float *VOL = rcgd->volumeData;

  for (int p = 0; p < P; ++p) {
    const double *const A = rcgd->projectionBuffer[p].matrix;
    const float *const I = rcgd->projectionBuffer[p].image;
    const int id = rcgd->projectionBuffer[p].id;

#pragma omp parallel for reduction(+ : voxels_clipping)                        \
    reduction(+ : voxels_actual)
    for (int z = 0; z < L; ++z) {
      for (int y = 0; y < L; ++y) {
        /* Select starting voxel and voxel count in x direction for this
         * projection image */
        int start = Range[id - 1][z * L + y].start;
        int stop = Range[id - 1][z * L + y].stop;
        if (stop - start == 0)
          continue;

        int missed_row = 0;

        for (int x = 0; x < L; ++x) {
          if (x >= start && x < stop)
            voxels_clipping++;
          /* Convert from VCS to WCS */
          float wz = z * MM + O;
          float wy = y * MM + O;
          float wx = x * MM + O;

          float u, v, w;
          u = A[0] * wx + A[3] * wy + A[6] * wz + A[9];
          v = A[1] * wx + A[4] * wy + A[7] * wz + A[10];
          w = A[2] * wx + A[5] * wy + A[8] * wz + A[11];

          float ix, iy;
          int iix, iiy;
          ix = u / w;
          iy = v / w;
          iix = (int)ix;
          iiy = (int)iy;

          // continue with next voxel if we missed the projection
          if ((iix + 1 < 0) || (iix > imageWidth - 1) || (iiy + 1 < 0) ||
              (iiy > imageHeight - 1))
            continue;

          // we hit the projection!
          voxels_actual++;
        } // x-loop
      } // y-loop
    } // z-loop
  } // projection loop
  return 1;
}
