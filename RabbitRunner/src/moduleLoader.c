#include <stdio.h>
#include <dlfcn.h>

#include <types.h>
#include <moduleLoader.h>


TfncLoadAlgorithm    s_fncLoadAlgorithm;
TfncPrepareAlgorithm   s_fncPrepareAlgorithm;
TfncAlgorithmIteration s_fncAlgorithmIteration;
TfncFinishAlgorithm   s_fncFinishAlgorithm;
TfncUnloadAlgorithm   s_fncUnloadAlgorithm;

#define LOCATE_FUNCTION(symbolname,mapping) \
    if (!(s_fnc ## symbolname = (Tfnc ## symbolname) dlsym(handle, mapping))) {  \
        printf("Failed to locate module function: %s ", mapping); \
        return 0;}


int
moduleLoader_loadSharedLibrary(const char * slpath)
{
    void* handle = dlopen(slpath, RTLD_LAZY);

    if (!handle)
    {
        printf("Failed to load algorithm module %s",slpath);
        return 0;
    }

    dlerror();  /* Clear any existing error */

    LOCATE_FUNCTION(LoadAlgorithm,RCT_FNCN_LOADALGORITHM);
    LOCATE_FUNCTION(PrepareAlgorithm,RCT_FNCN_PREPAREALGORITHM);
    LOCATE_FUNCTION(AlgorithmIteration,RCT_FNCN_ALGORITHMBACKPROJ);
    LOCATE_FUNCTION(FinishAlgorithm,RCT_FNCN_FINISHALGORITHM);
    LOCATE_FUNCTION(UnloadAlgorithm,RCT_FNCN_UNLOADALGORITHM);

    return 1;
}

