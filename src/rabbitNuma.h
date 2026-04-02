
#ifndef NUMA_H
#define NUMA_H
#include <sys/types.h>
#include <unistd.h>

#include "rabbitNuma_types.h"
/** Structure holding numa information
 *
 */
extern RabbitNumaTopology numa_info;

extern int rabbitNuma_init(void);
extern void rabbitNuma_setInterleaved(int *processorList, int numberOfProcessors);

#endif /*NUMA_H*/
