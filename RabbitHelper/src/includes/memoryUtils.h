#ifndef MEMORYUTILS_H
#define MEMORYUTILS_H

extern void memoryUtils_init();
extern void memoryUtils_allocate(float** ptr, uint64_t size);
extern void memoryUtils_zeroPadInit(RabbitCtGlobalData * rcgd, ZeroPadding* padding);
extern void memoryUtils_zeroPadAllocate(RabbitCtGlobalData * rcgd, ZeroPadding* padding );
extern void memoryUtils_zeroPadEnterExp(RabbitCtGlobalData * rcgd, ZeroPadding* padding, Projection* projectionBuffer );
extern void memoryUtils_zeroPadEnter(RabbitCtGlobalData * rcgd, ZeroPadding* padding );
extern void memoryUtils_zeroPadLeave(RabbitCtGlobalData * rcgd, ZeroPadding* padding);

#endif /*MEMORYUTILS_H*/

