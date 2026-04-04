/* Copyright (C) NHR@FAU, University Erlangen-Nuremberg.
 * All rights reserved. This file is part of RabbitCT.
 * Use of this source code is governed by a MIT style
 * license that can be found in the LICENSE file. */
#include <assert.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include <analyseGeometry.h>
#include <memoryUtils.h>
#include <rabbitCt.h>
#include <rabbitHelper_types.h>

extern void fastrabbit(
    float **, const float *, float **, float *, float *, float *, float *, int);

static int Init = 0;
static ZeroPaddingType Padding;
static LineRangeType **Range;

int lolaAsmPrepare(RabbitCtGlobalData *rcgd)
{
  OutShadowType shadow;

  if (rcgd->numberOfProjections == 0) {
    fprintf(stderr,
        "This module needs global geometry information.\nPlease add -a switch!\n");
    exit(0);
  }

  Range = (LineRangeType **)malloc(rcgd->numberOfProjections * sizeof(LineRangeType *));

  for (int i = 0; i < rcgd->numberOfProjections; i++) {
    Range[i] = (LineRangeType *)malloc(
        rcgd->problemSize * rcgd->problemSize * sizeof(LineRangeType));
  }

  memoryUtilsInit();
  computeShadowOfProjection(rcgd, &shadow);
  computeLineRanges(rcgd, Range);

  Padding.extend.Umin = abs(shadow.Umin);
  Padding.extend.Umax = abs(shadow.Umax) - rcgd->imageWidth;
  Padding.extend.Vmin = abs(shadow.Vmin);
  Padding.extend.Vmax = abs(shadow.Vmax) - rcgd->imageHeight;

  printf("Padding\n");
  printf("bottom:%d \n", Padding.extend.Vmin);
  printf("top: %d \n", Padding.extend.Vmax);
  printf("left: %d \n", Padding.extend.Umin);
  printf("right: %d \n", Padding.extend.Umax);

  memoryUtilsZeroPadInit(rcgd, &Padding);

#ifdef SIMD_NAME
  printf("SIMD instruction set: %s (vector width: %d)\n", SIMD_NAME, VECTORSIZE);
#endif

  return 1;
}

int lolaAsmFinish(RabbitCtGlobalData *rcgd)
{
  return 1;
}

#define BLOCKING_FACTOR 8

int lolaAsmBackprojection(RabbitCtGlobalData *rcgd)
{
  const int l      = rcgd->problemSize;
  float *const vol = rcgd->volumeData;
  const float mm   = rcgd->voxelSize;

  if (!Init) {
    printf("Allocating Padded Images\n");
    memoryUtilsZeroPadAllocate(rcgd, &Padding);
    Init = 1;
  }

#pragma omp parallel
  {
    Projection *projectionBuffer =
        (Projection *)malloc(rcgd->numberOfProjections * sizeof(Projection));
    int imageWidth = Padding.lineOffset;
    memoryUtilsZeroPadEnterExp(rcgd, &Padding, projectionBuffer);
#pragma omp barrier

#pragma omp for schedule(runtime)
    for (int z = 0; z < l; z++) {
      unsigned int offset;
      float tmp[3];

      float wz = z * mm + rcgd->O_Index;

      for (int y = 0; y < l; y += BLOCKING_FACTOR) {
        for (int proj = 0; proj < rcgd->numberOfProjections; proj++) {
          for (int yh = 0; yh < BLOCKING_FACTOR; yh++) {
            int yn                = (y + yh);
            float wy              = yn * mm + rcgd->O_Index;
            const double *const a = projectionBuffer[proj].matrix;
            const float *const i  = projectionBuffer[proj].image;
            int start = Range[projectionBuffer[proj].id - 1][z * l + yn].start;
            int count = Range[projectionBuffer[proj].id - 1][z * l + yn].stop -
                        Range[projectionBuffer[proj].id - 1][z * l + yn].start;

            offset    = z * l * l + yn * l + start;
            float wx  = start * mm + rcgd->O_Index;

            if (start < 0) {
              continue;
            }

            tmp[0] = (float)(a[3] * wy + a[6] * wz + a[9]);
            tmp[1] = (float)(a[4] * wy + a[7] * wz + a[10]);
            tmp[2] = (float)(a[5] * wy + a[8] * wz + a[11]);

            __attribute__((aligned(ARRAY_ALIGNMENT))) float WX[VECTORSIZE];
            __attribute__((aligned(ARRAY_ALIGNMENT))) float ISX4[VECTORSIZE];
            __attribute__((aligned(ARRAY_ALIGNMENT))) float MM4[VECTORSIZE];
            __attribute__((aligned(ARRAY_ALIGNMENT))) float A0[VECTORSIZE];
            __attribute__((aligned(ARRAY_ALIGNMENT))) float A1[VECTORSIZE];
            __attribute__((aligned(ARRAY_ALIGNMENT))) float A2[VECTORSIZE];
            __attribute__((aligned(ARRAY_ALIGNMENT))) float TMP0[VECTORSIZE];
            __attribute__((aligned(ARRAY_ALIGNMENT))) float TMP1[VECTORSIZE];
            __attribute__((aligned(ARRAY_ALIGNMENT))) float TMP2[VECTORSIZE];

            float *APTR[3];
            float *TMP[3];

            APTR[0] = A0;
            APTR[1] = A1;
            APTR[2] = A2;
            TMP[0]  = TMP0;
            TMP[1]  = TMP1;
            TMP[2]  = TMP2;

            /* Initialize Vector register data */
            for (int j = 0; j < VECTORSIZE; j++)
              WX[j] = wx + j * mm;

            for (int j = 0; j < VECTORSIZE; j++) {
              ISX4[j] = (float)imageWidth;
              MM4[j]  = (float)VECTORSIZE * mm;
              A0[j]   = a[0];
              A1[j]   = a[1];
              A2[j]   = a[2];
              TMP0[j] = tmp[0];
              TMP1[j] = tmp[1];
              TMP2[j] = tmp[2];
            }

            fastrabbit(APTR, /* projection matrix component ptrs */
                i, /* projection image  */
                TMP, /* temp component ptrs */
                vol + offset, /* Volume image ptr */
                WX, /* float ptr */
                MM4, /* float ptr */
                ISX4, /* integer constant */
                count); /* stopping criterion */
          }
        }
      }
    }
    free(projectionBuffer);
  }

  return 1;
}
