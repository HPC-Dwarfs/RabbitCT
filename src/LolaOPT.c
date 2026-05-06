#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "likwid-marker.h"
#include <analyseGeometry.h>
#include <memoryUtils.h>
#include <rabbitCt.h>
#include <rabbitHelper_types.h>
#ifdef KERNEL_CYCLES
#include <rabbitTimer.h>
#endif

static OutShadowType Shadow;
static LineRangeType **Range;
static ZeroPaddingType Padding;
static float *PaddedImg;

int lolaOptPrepare(RabbitCtGlobalData *rcgd)
{
  /* numberOfProjections is set to N in main if -a was given */
  if (rcgd->numberOfProjections == 0) {
    fprintf(stderr,
        "This module needs global geometry information.\n"
        "Please add -a switch!\n");
    exit(EXIT_FAILURE);
  }

  /* Allocate N * L * L * sizeof(LineRangeType) bytes of memory:
     * For each projection we need L * L LineRanges */
  if ((Range = (LineRangeType **)malloc(
           rcgd->numberOfProjections * sizeof(LineRangeType *))) == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < rcgd->numberOfProjections; i++) {
    if ((Range[i] = (LineRangeType *)malloc(
             rcgd->problemSize * rcgd->problemSize * sizeof(LineRangeType))) == NULL) {
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
  computeShadowOfProjection(rcgd, &Shadow);
  int padXl           = abs(Shadow.Umin);
  int padXr           = abs(Shadow.Umax) - rcgd->imageWidth;
  int padYb           = abs(Shadow.Vmin);
  int padYt           = abs(Shadow.Vmax) - rcgd->imageHeight;
  int xSize           = padXl + rcgd->imageWidth + padXr;
  int ySize           = padYb + rcgd->imageHeight + padYt;
  Padding.paddedSize  = xSize * ySize;
  Padding.startOffset = padYb * xSize + padXl;
  Padding.lineOffset  = xSize;
  if ((PaddedImg = (float *)malloc(xSize * ySize * sizeof(float))) == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < xSize * ySize; ++i) {
    PaddedImg[i] = 0.0f;
  }

  return 1;
}

int lolaOptFinish(RabbitCtGlobalData *rcgd)
{
  return 1;
}

int lolaOptBackprojection(RabbitCtGlobalData *rcgd)
{
  const int l    = rcgd->problemSize;
  const int numP = rcgd->numberOfProjections; // projections in this run
  const float mm = rcgd->voxelSize;
  const float o  = rcgd->O_Index;
  //const float imageWidth = rcgd->imageWidth;
  const float imageWidth  = Padding.lineOffset;
  const float imageHeight = rcgd->imageHeight;

  float *vol              = rcgd->volumeData;

#pragma omp parallel
  {
    for (int p = 0; p < numP; ++p) {
      const double *const a    = rcgd->projectionBuffer[p].matrix;
      const float *const iOrig = rcgd->projectionBuffer[p].image;
      const int id             = rcgd->projectionBuffer[p].id;

      // copy projection image into zero padded buffer
      float *cursor = PaddedImg + Padding.startOffset;
#pragma omp for schedule(dynamic)
      for (int i = 0; i < rcgd->imageHeight; i++)
        memcpy(cursor + (i * Padding.lineOffset),
            iOrig + (i * rcgd->imageWidth),
            rcgd->imageWidth * sizeof(float));
#pragma omp barrier
      const float *const i = cursor;

#pragma omp for schedule(runtime) collapse(2)
      for (int z = 0; z < l; ++z) {
        for (int y = 0; y < l; ++y) {
          /* Select starting voxel and voxel count in x direction for this
                     * projection image */
          int start = Range[id - 1][z * l + y].start;
          int stop  = Range[id - 1][z * l + y].stop;
          if (stop - start == 0)
            continue;

          /* Convert from VCS to WCS */
          float wz            = z * mm + o;
          float wy            = y * mm + o;
          float wx            = start * mm + o;

          unsigned int offset = z * l * l + y * l;

          /* Precalculate parts of u, v, w that are invariant with
                     * respect to x and store in tmp[] */
          float tmp[3];
          tmp[0] = (float)(a[3] * wy + a[6] * wz + a[9]);
          tmp[1] = (float)(a[4] * wy + a[7] * wz + a[10]);
          tmp[2] = (float)(a[5] * wy + a[8] * wz + a[11]);

          for (int x = start; x < stop; ++x) {
            float u     = tmp[0] + wx * a[0];
            float v     = tmp[1] + wx * a[1];
            float w     = tmp[2] + wx * a[2];
            float rcpW  = 1.0f / w;
            float rcp2W = rcpW * rcpW;

            float ix    = u * rcpW;
            float iy    = v * rcpW;

            int iix     = (int)ix;
            int iiy     = (int)iy;

            float scalx = ix - iix;
            float scaly = iy - iiy;

            wx += mm;

            float valbl = i[iiy * Padding.lineOffset + iix];
            float valbr = i[iiy * Padding.lineOffset + iix + 1];
            float valtl = i[(iiy + 1) * Padding.lineOffset + iix];
            float valtr = i[(iiy + 1) * Padding.lineOffset + iix + 1];

            float valb  = (1 - scalx) * valbl + scalx * valbr;
            float valt  = (1 - scalx) * valtl + scalx * valtr;
            float val   = (1 - scaly) * valb + scaly * valt;
            float wval  = val * rcp2W;

            vol[offset + x] += wval;
          }
        } // y-loop
      } // z-loop
    } // projection loop
  } // pragma omp parallel
  return 1;
}
