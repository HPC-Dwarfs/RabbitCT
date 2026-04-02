
#ifndef AFFINITY_TYPES_H
#define AFFINITY_TYPES_H

#include "bstrlib.h"
#include <stdint.h>

typedef struct {
  bstring tag;
  uint32_t numberOfProcessors;
  int *processorList;
} AffinityDomainType;

#endif /*AFFINITY_TYPES_H*/
