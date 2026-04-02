/* Copyright (C) NHR@FAU, University Erlangen-Nuremberg.
 * All rights reserved. This file is part of RabbitCT.
 * Use of this source code is governed by a MIT style
 * license that can be found in the LICENSE file. */
#include <stdio.h>
#include <string.h>

#include "algorithmRegistry.h"
#include "types.h"

/* ---- forward declarations for each compiled-in algorithm ---- */
extern int LolaREF_Load(RabbitCtGlobalData *);
extern int LolaREF_Prepare(RabbitCtGlobalData *);
extern int LolaREF_Backprojection(RabbitCtGlobalData *);
extern int LolaREF_Finish(RabbitCtGlobalData *);
extern int LolaREF_Unload(RabbitCtGlobalData *);

/* ---- global function pointer variables ---- */
TfncLoadAlgorithm      s_fncLoadAlgorithm;
TfncPrepareAlgorithm   s_fncPrepareAlgorithm;
TfncAlgorithmIteration s_fncAlgorithmIteration;
TfncFinishAlgorithm    s_fncFinishAlgorithm;
TfncUnloadAlgorithm    s_fncUnloadAlgorithm;

/* ---- static registry: add new algorithms here ---- */
static const AlgorithmEntry s_algorithms[] = {
    { "LolaREF",
      LolaREF_Load, LolaREF_Prepare, LolaREF_Backprojection,
      LolaREF_Finish, LolaREF_Unload },
    { NULL, NULL, NULL, NULL, NULL, NULL } /* sentinel */
};

int algorithmRegistry_find(const char *name)
{
    for (int i = 0; s_algorithms[i].name != NULL; i++) {
        if (strcmp(s_algorithms[i].name, name) == 0) {
            s_fncLoadAlgorithm      = s_algorithms[i].load;
            s_fncPrepareAlgorithm   = s_algorithms[i].prepare;
            s_fncAlgorithmIteration = s_algorithms[i].iterate;
            s_fncFinishAlgorithm    = s_algorithms[i].finish;
            s_fncUnloadAlgorithm    = s_algorithms[i].unload;
            return 1;
        }
    }
    printf("Unknown algorithm: %s\n", name);
    printf("Available algorithms:\n");
    for (int i = 0; s_algorithms[i].name != NULL; i++) {
        printf("  %s\n", s_algorithms[i].name);
    }
    return 0;
}
