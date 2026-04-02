/* Copyright (C) NHR@FAU, University Erlangen-Nuremberg.
 * All rights reserved. This file is part of RabbitCT.
 * Use of this source code is governed by a MIT style
 * license that can be found in the LICENSE file. */

#include "memoryUtils_types.h"
#ifdef _OPENMP
#include <omp.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include "analyseGeometry.h"
#include "rabbitCt.h"
#ifdef KERNEL_CYCLES
#include <rabbitTimer.h>
#endif

#define SIMD_BIT (512)
#define SIMD_BYTE (SIMD_BIT / 8)
#define SIMD_OPS (SIMD_BYTE / sizeof(float))

static OutShadow Shadow;
static LineRange **Range;
static ZeroPaddingType Padding;
static float *PaddedImg;
static uint64_t VoxelsClipping = 0, VoxelsActual = 0;

void lolaRefFinish(RabbitCtGlobalData *rcgd)
{
  const int l    = rcgd->problemSize;
  uint64_t total = (uint64_t)l * (uint64_t)l * (uint64_t)l * 496UL;
  printf("RCTFinishAlgorithm(): clipping stats:\n");
  printf("Total Volume Voxels: %llu\n", total);
  printf("Clipped Volume Voxels: %llu\n", VoxelsClipping);
  printf("Actual Volume Voxels: %llu\n", VoxelsActual);
}

int lolaRefPrepare(RabbitCtGlobalData *rcgd)
{
  /* numberOfProjections is set to N in main if -a was given */
  if (rcgd->numberOfProjections == 0) {
    fprintf(stderr,
        "This module needs global geometry information.\n"
        "Please add -a switch!\n");
    exit(EXIT_FAILURE);
  }

  /* Allocate N * L * L * sizeof(LineRange) bytes of memory:
   * For each projection we need L * L LineRanges */
  if ((Range = (LineRange **)malloc(rcgd->numberOfProjections * sizeof(LineRange *))) ==
      NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < rcgd->numberOfProjections; i++) {
    if ((Range[i] = (LineRange *)malloc(
             rcgd->problemSize * rcgd->problemSize * sizeof(LineRange))) == NULL) {
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

int lolaRefBackprojection(RabbitCtGlobalData *rcgd)
{
  const int l             = rcgd->problemSize;
  const int np            = rcgd->numberOfProjections; // projections in this run
  const float mm          = rcgd->voxelSize;
  const float o           = rcgd->O_Index;
  const float imageWidth  = rcgd->imageWidth;
  const float imageHeight = rcgd->imageHeight;

  float *vol              = rcgd->volumeData;

  for (int p = 0; p < np; ++p) {
    const double *const a = rcgd->projectionBuffer[p].matrix;
    const float *const i  = rcgd->projectionBuffer[p].image;
    const int id          = rcgd->projectionBuffer[p].id;

#pragma omp parallel for reduction(+ : voxels_clipping) reduction(+ : voxels_actual)
    for (int z = 0; z < l; ++z) {
      for (int y = 0; y < l; ++y) {
        /* Select starting voxel and voxel count in x direction for this
         * projection image */
        int start = Range[id - 1][z * l + y].start;
        int stop  = Range[id - 1][z * l + y].stop;
        if (stop - start == 0)
          continue;

        int missedRow = 0;

        for (int x = 0; x < l; ++x) {
          if (x >= start && x < stop)
            VoxelsClipping++;
          /* Convert from VCS to WCS */
          float wz = z * mm + o;
          float wy = y * mm + o;
          float wx = x * mm + o;

          float u, v, w;
          u = a[0] * wx + a[3] * wy + a[6] * wz + a[9];
          v = a[1] * wx + a[4] * wy + a[7] * wz + a[10];
          w = a[2] * wx + a[5] * wy + a[8] * wz + a[11];

          float ix, iy;
          int iix, iiy;
          ix  = u / w;
          iy  = v / w;
          iix = (int)ix;
          iiy = (int)iy;

          // continue with next voxel if we missed the projection
          if ((iix + 1 < 0) || (iix > imageWidth - 1) || (iiy + 1 < 0) ||
              (iiy > imageHeight - 1))
            continue;

          // we hit the projection!
          VoxelsActual++;
        } // x-loop
      } // y-loop
    } // z-loop
  } // projection loop
  return 1;
}
