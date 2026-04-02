
/* #####   HEADER FILE INCLUDES   ######################################### */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <pthread.h>

#include <error.h>
#include <rabbitHelper_types.h>
#include <rabbitNuma.h>
#include <rabbitAffinity.h>

/* #####   EXPORTED VARIABLES   ########################################### */


/* #####   MACROS  -  LOCAL TO THIS SOURCE FILE   ######################### */

#define gettid() syscall(SYS_gettid)


/* #####   VARIABLES  -  LOCAL TO THIS SOURCE FILE   ###################### */


/* #####   FUNCTION DEFINITIONS  -  LOCAL TO THIS SOURCE FILE   ########### */

static int
getProcessorID(cpu_set_t* cpu_set)
{
    int processorId;

    for (processorId=0;processorId<MAX_NUM_THREADS;processorId++)
    {
        if (CPU_ISSET(processorId,cpu_set))
        {
            break;
        }
    }
    return processorId;
}


/* #####   FUNCTION DEFINITIONS  -  EXPORTED FUNCTIONS   ################## */



int  rabbitAffinity_processGetProcessorId()
{
    int ret;
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    ret = sched_getaffinity(getpid(),sizeof(cpu_set_t), &cpu_set);

    if (ret < 0)
    {
        ERROR;
    }

    return getProcessorID(&cpu_set);
}


int  rabbitAffinity_threadGetProcessorId()
{
    cpu_set_t  cpu_set;
    CPU_ZERO(&cpu_set);
    sched_getaffinity(gettid(),sizeof(cpu_set_t), &cpu_set);

    return getProcessorID(&cpu_set);
}

void  rabbitAffinity_pinThread(int processorId)
{
    int ret;
	cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(processorId, &cpuset);
    ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    if (ret != 0) 
    {
        ERROR;
    }
}

void  rabbitAffinity_pinProcess(int processorId)
{
    int ret;
	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	CPU_SET(processorId, &cpuset);
	ret = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

	if (ret < 0) 
	{
        ERROR;
	}
}


