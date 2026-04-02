#ifndef MODULELOADER_TYPES_H
#define MODULELOADER_TYPES_H

typedef int (*TfncLoadAlgorithm)(RabbitCtGlobalData *);
typedef int (*TfncPrepareAlgorithm)(RabbitCtGlobalData *);
typedef int (*TfncAlgorithmIteration)(RabbitCtGlobalData *);
typedef int (*TfncUnloadAlgorithm)(RabbitCtGlobalData *);
typedef int (*TfncFinishAlgorithm)(RabbitCtGlobalData *);

#endif

