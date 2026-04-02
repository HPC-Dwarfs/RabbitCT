/* Copyright (C) NHR@FAU, University Erlangen-Nuremberg.
 * All rights reserved. This file is part of RabbitCT.
 * Use of this source code is governed by a MIT style
 * license that can be found in the LICENSE file. */
#ifndef MEMORYUTILS_TYPES_H
#define MEMORYUTILS_TYPES_H

#include "analyseGeometry_types.h"
#include <stdint.h>

typedef struct {
  float ***buffern;
  float **buffer;
  OutShadow extend;
  uint64_t paddedSize;
  int startOffset;
  int lineOffset;
  int lineSize;
  float **savePtr;
} ZeroPaddingType;

#endif /*MEMORYUTILS_TYPES_H*/
