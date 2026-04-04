/* Copyright (C) NHR@FAU, University Erlangen-Nuremberg.
 * All rights reserved. This file is part of RabbitCT.
 * Use of this source code is governed by a MIT style
 * license that can be found in the LICENSE file. */
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

#include "fastRabbit_ispc.h"

static OutShadowType Shadow;
static LineRangeType **Range;
static ZeroPaddingType Padding;
static float *PaddedImg;

int lolaIspcPrepare(RabbitCtGlobalData *rcgd)
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

  computeLineRanges(rcgd, Range);

  printf("ISPC SIMD width: %d\n", width());

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
  for (int i = 0; i < xSize * ySize; ++i)
    PaddedImg[i] = 0.0f;

#pragma omp parallel
  {
    LIKWID_MARKER_THREADINIT;
  }

  return 1;
}

int lolaIspcFinish(RabbitCtGlobalData *rcgd)
{
  return 1;
}

int lolaIspcBackprojection(RabbitCtGlobalData *rcgd)
{
  const int l    = rcgd->problemSize;
  const int numP = rcgd->numberOfProjections;
  const float mm = rcgd->voxelSize;
  const float imageWidth = Padding.lineOffset;

  float *vol = rcgd->volumeData;

#pragma omp parallel
  {
    for (int p = 0; p < numP; ++p) {
      const double *const a    = rcgd->projectionBuffer[p].matrix;
      const float *const iOrig = rcgd->projectionBuffer[p].image;
      const int id             = rcgd->projectionBuffer[p].id;

      /* copy projection image into zero padded buffer */
      float *cursor = PaddedImg + Padding.startOffset;
#pragma omp for schedule(dynamic)
      for (int i = 0; i < rcgd->imageHeight; i++)
        memcpy(cursor + (i * Padding.lineOffset),
            iOrig + (i * rcgd->imageWidth),
            rcgd->imageWidth * sizeof(float));
#pragma omp barrier
      const float *const img = cursor;

#pragma omp for schedule(runtime) collapse(2)
      for (int z = 0; z < l; ++z) {
        for (int y = 0; y < l; ++y) {
          int start = Range[id - 1][z * l + y].start;
          int stop  = Range[id - 1][z * l + y].stop;
          if (stop - start == 0)
            continue;

          float wz = z * mm + rcgd->O_Index;
          float wy = y * mm + rcgd->O_Index;

          float a0 = a[0];
          float a1 = a[1];
          float a2 = a[2];

          unsigned int offset = z * l * l + y * l;

          /* Precalculate parts of u, v, w that are invariant with
           * respect to x */
          float tmpU = (float)(a[3] * wy + a[6] * wz + a[9]);
          float tmpV = (float)(a[4] * wy + a[7] * wz + a[10]);
          float tmpW = (float)(a[5] * wy + a[8] * wz + a[11]);

          LIKWID_MARKER_START("fastrabbit");

          Backprojection(start, stop, mm, imageWidth,
              a0, a1, a2, tmpU, tmpV, tmpW, rcgd->O_Index,
              vol + offset, img);

          LIKWID_MARKER_STOP("fastrabbit");

#ifdef KERNEL_CYCLES
          if ((stop - start) != l)
            continue;
          for (int i = 0; i < 50; ++i) {
            CyclesData cycleData;
            rabbitTimer_startCycles(&cycleData);

            Backprojection(start, stop, mm, imageWidth,
                a0, a1, a2, tmpU, tmpV, tmpW, rcgd->O_Index,
                vol + offset, img);

            rabbitTimer_stopCycles(&cycleData);
            printf("cycles for one line (%d voxels)/one voxel: "
                "%llu / %f\n", (stop - start),
                LLU_CAST rabbitTimer_printCycles(&cycleData),
                (float)rabbitTimer_printCycles(&cycleData) / (stop - start));
          }
          fflush(stdout);
          printf("EXITING\n");
          exit(0);
#endif
        } /* y-loop */
      } /* z-loop */
    } /* projection loop */
  } /* pragma omp parallel */
  return 1;
}
