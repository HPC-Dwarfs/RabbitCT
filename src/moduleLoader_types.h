/* Copyright (C) NHR@FAU, University Erlangen-Nuremberg.
 * All rights reserved. This file is part of RabbitCT.
 * Use of this source code is governed by a MIT style
 * license that can be found in the LICENSE file. */
#ifndef MODULELOADER_TYPES_H
#define MODULELOADER_TYPES_H

typedef int (*TfncLoadAlgorithm)(RabbitCtGlobalData *);
typedef int (*TfncPrepareAlgorithm)(RabbitCtGlobalData *);
typedef int (*TfncAlgorithmIteration)(RabbitCtGlobalData *);
typedef int (*TfncUnloadAlgorithm)(RabbitCtGlobalData *);
typedef int (*TfncFinishAlgorithm)(RabbitCtGlobalData *);

#endif
