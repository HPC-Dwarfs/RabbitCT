#ifndef MEMORYUTILS_TYPES_H
#define MEMORYUTILS_TYPES_H

#include <stdint.h>


typedef struct {
    float*** buffern;
    float** buffer;
    OutShadow extend;
    uint64_t paddedSize;
    int startOffset;
    int lineOffset;
    int lineSize;
    float**  savePtr;
} ZeroPadding;




#endif /*MEMORYUTILS_TYPES_H*/
