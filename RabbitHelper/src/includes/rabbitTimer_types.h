#ifndef RABBITTIMER_TYPES_H
#define RABBITTIMER_TYPES_H

#include <stdint.h>
#include <sys/time.h>

typedef union
{  
    uint64_t int64;                   /** 64 bit unsigned integer fo cycles */
    struct {uint32_t lo, hi;} int32;  /** two 32 bit unsigned integers used
                                        for register values */
} TscCounter;

typedef struct {
    struct timeval before;
    struct timeval after;
} TimerData;

typedef struct {
    TscCounter start;
    TscCounter stop;
    uint64_t base;
} CyclesData;


#endif /*RABBITTIMER_TYPES_H*/
