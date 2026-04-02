#ifndef CT_FILEREADER_TYPES_H
#define CT_FILEREADER_TYPES_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint32_t imageDimension[2];    /* projection image dimension */
    uint32_t numberOfImages;       /* number of projection images */
    float    HUScalingFactors[2];  /* HU scaling factors */
} RabbitCtHeader;

typedef struct {
    RabbitCtHeader header;
    int imageSize;
    FILE*    file;
} RabbitCtFile;

#endif


