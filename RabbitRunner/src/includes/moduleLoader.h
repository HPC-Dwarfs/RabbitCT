#ifndef MODULELOADER_H
#define MODULELOADER_H

#include <types.h>

extern TfncLoadAlgorithm	   s_fncLoadAlgorithm;
extern TfncPrepareAlgorithm	   s_fncPrepareAlgorithm;
extern TfncAlgorithmIteration  s_fncAlgorithmIteration;
extern TfncFinishAlgorithm     s_fncFinishAlgorithm;
extern TfncUnloadAlgorithm     s_fncUnloadAlgorithm;

extern int moduleLoader_loadSharedLibrary(const char * slpath);

#endif 
