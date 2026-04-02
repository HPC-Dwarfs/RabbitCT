/* Copyright (C) NHR@FAU, University Erlangen-Nuremberg.
 * All rights reserved. This file is part of RabbitCT.
 * Use of this source code is governed by a MIT style
 * license that can be found in the LICENSE file. */
#ifndef ALGORITHMREGISTRY_H
#define ALGORITHMREGISTRY_H

#include "types.h"

typedef struct {
    const char             *name;
    TfncLoadAlgorithm       load;
    TfncPrepareAlgorithm    prepare;
    TfncAlgorithmIteration  iterate;
    TfncFinishAlgorithm     finish;
    TfncUnloadAlgorithm     unload;
} AlgorithmEntry;

extern TfncLoadAlgorithm      s_fncLoadAlgorithm;
extern TfncPrepareAlgorithm   s_fncPrepareAlgorithm;
extern TfncAlgorithmIteration s_fncAlgorithmIteration;
extern TfncFinishAlgorithm    s_fncFinishAlgorithm;
extern TfncUnloadAlgorithm    s_fncUnloadAlgorithm;

extern int algorithmRegistry_find(const char *name);

#endif
