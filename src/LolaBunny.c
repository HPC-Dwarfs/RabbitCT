/* Copyright (C) NHR@FAU, University Erlangen-Nuremberg.
 * All rights reserved. This file is part of RabbitCT.
 * Use of this source code is governed by a MIT style
 * license that can be found in the LICENSE file. */
#include <stdio.h>

#include "rabbitCt.h"

static RabbitCtGlobalData *s_rcgd = NULL;

static inline double p_n(const float *image, int i, int j)
{
  if (i >= 0 && i < (int)s_rcgd->imageWidth && j >= 0 && j < (int)s_rcgd->imageHeight)
    return image[j * s_rcgd->imageWidth + i];

  return 0.0;
}

static inline double p_hat_n(const float *image, double x, double y)
{
  int i        = (int)x;
  int j        = (int)y;
  double alpha = x - (int)x;
  double beta  = y - (int)y;

  return (1.0 - alpha) * (1.0 - beta) * p_n(image, i,     j    )
       +        alpha  * (1.0 - beta) * p_n(image, i + 1, j    )
       + (1.0 - alpha) *        beta  * p_n(image, i,     j + 1)
       +        alpha  *        beta  * p_n(image, i + 1, j + 1);
}

int lolaBunnyPrepare(RabbitCtGlobalData *rcgd)
{
  (void)rcgd;
  return 1;
}

int lolaBunnyFinish(RabbitCtGlobalData *rcgd)
{
  (void)rcgd;
  return 1;
}

int lolaBunnyBackprojection(RabbitCtGlobalData *r)
{
  unsigned int L   = r->problemSize;
  float        O_L = r->O_Index;
  float        R_L = r->voxelSize;
  float       *f_L = r->volumeData;

  s_rcgd = r;

  for (int p = 0; p < (int)r->numberOfProjections; p++) {
    double       *A_n   = r->projectionBuffer[p].matrix;
    const float  *image = r->projectionBuffer[p].image;

    for (unsigned int k = 0; k < L; k++) {
      double z = O_L + (double)k * R_L;
      for (unsigned int j = 0; j < L; j++) {
        double y = O_L + (double)j * R_L;
        for (unsigned int i = 0; i < L; i++) {
          double x = O_L + (double)i * R_L;

          double w_n = A_n[2] * x + A_n[5] * y + A_n[8]  * z + A_n[11];
          double u_n = (A_n[0] * x + A_n[3] * y + A_n[6]  * z + A_n[9] ) / w_n;
          double v_n = (A_n[1] * x + A_n[4] * y + A_n[7]  * z + A_n[10]) / w_n;

          f_L[k * L * L + j * L + i] +=
              (float)(1.0 / (w_n * w_n) * p_hat_n(image, u_n, v_n));
        }
      }
    }
  }

  return 1;
}
