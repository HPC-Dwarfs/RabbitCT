
#ifndef RABBITAFFINITY_H
#define RABBITAFFINITY_H

extern void rabbitAffinity_init();
extern void rabbitAffinity_finalize();
extern int  rabbitAffinity_processGetProcessorId();
extern int  rabbitAffinity_threadGetProcessorId();
extern void  rabbitAffinity_pinProcess(int processorId);
extern void  rabbitAffinity_pinThread(int processorId);
extern const AffinityDomain* rabbitAffinity_getDomain(bstring domain);
extern void rabbitAffinity_printDomains();

#endif /*RABBITAFFINITY_H*/

