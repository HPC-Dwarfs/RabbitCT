#ifndef TYPES_H
#define TYPES_H


/* #####   HEADER FILE INCLUDES   ######################################### */
#include <stdint.h>
#include <rabbitHelper_types.h>

#include <rabbitCt.h>
#include <ctFileReader_types.h>
#include <moduleLoader_types.h>


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
#define LL_CAST  (long long)

#endif /*TYPES_H*/
