/* Copyright (C) NHR@FAU, University Erlangen-Nuremberg.
 * All rights reserved. This file is part of RabbitCT.
 * Use of this source code is governed by a MIT style
 * license that can be found in the LICENSE file. */
#ifndef ANALYSE_GEOMETRY_H
#define ANALYSE_GEOMETRY_H

#include "analyseGeometry_types.h"
#include "rabbitCt.h"
#define LR_PREFIX "RabbitInput/LineRange"

extern void computeShadowOfProjection(RabbitCtGlobalData *data, OutShadow *shadow);
extern void computeLineRanges(RabbitCtGlobalData *data, LineRange **range);

#endif /*ANALYSE_GEOMETRY_H*/
