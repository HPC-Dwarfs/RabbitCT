#ifndef RABBITTIMER_H
#define RABBITTIMER_H

#include <sys/time.h>
#include <rabbitHelper_types.h>

#ifdef __x86_64
#define RDTSC2(cpu_c) \
__asm__ volatile( "rdtsc\n\t"           \
"movl %%eax, %0\n\t"  \
"movl %%edx, %1\n\t"  \
: "=r" ((cpu_c).int32.lo), "=r" ((cpu_c).int32.hi) \
: : "%eax", "%edx")

#define RDTSC(cpu_c) \
__asm__ volatile("xor %%eax,%%eax\n\t"           \
"cpuid\n\t"           \
"rdtsc\n\t"           \
"movl %%eax, %0\n\t"  \
"movl %%edx, %1\n\t"  \
: "=r" ((cpu_c).int32.lo), "=r" ((cpu_c).int32.hi) \
: : "%eax","%ebx","%ecx","%edx")
#endif



extern void rabbitTimer_init(void );
extern void rabbitTimer_start(TimerData* time);
extern void rabbitTimer_stop(TimerData* time);
extern float rabbitTimer_print(TimerData* timer);
extern void rabbitTimer_startCycles(CyclesData* cycles);
extern void rabbitTimer_stopCycles(CyclesData* cycles);
extern uint64_t rabbitTimer_printCyclesTime(CyclesData* cycles);
extern uint64_t rabbitTimer_printCycles(CyclesData* cycles);
extern uint64_t rabbitTimer_getCpuClock(void);
extern uint64_t rabbitTimer_getCpuidCycles(void);

#endif /* RABBITTIMER_H */
