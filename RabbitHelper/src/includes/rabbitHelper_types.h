#ifndef RABBITHELPER_TYPES_H
#define RABBITHELPER_TYPES_H


/* #####   HEADER FILE INCLUDES   ######################################### */
#include <stdint.h>

#include <bstrlib.h>
#include <rabbitCt.h>
#include <rabbitNuma_types.h>
#include <rabbitAffinity_types.h>
#include <rabbitTimer_types.h>
#include <analyseGeometry_types.h>
#include <memoryUtils_types.h>


/* #####   EXPORTED MACROS   ############################################## */

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif
#ifndef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#endif

#define TRUE  1
#define FALSE 0

#define HLINE "-------------------------------------------------------------\n"
#define SLINE "*************************************************************\n"

#define LLU_CAST  (unsigned long long)

#endif /*RABBITHELPER_TYPES_H*/
