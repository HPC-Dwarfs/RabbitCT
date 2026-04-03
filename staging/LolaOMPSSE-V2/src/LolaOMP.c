#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <assert.h>

#include <rabbitCt.h>
#include <rabbitHelper_types.h>
#include <analyseGeometry.h>
#include <memoryUtils.h>


extern void fastrabbit(float**,
        const float*,
        float**,
        float*,
        float*,
        float*,
        float*,
        int);

static int init = 0;
static ZeroPadding Padding;
static LineRange** Range;

int RCTLoadAlgorithm(RabbitCtGlobalData * rcgd)
{
    return 1;
}


int RCTFinishAlgorithm(RabbitCtGlobalData * rcgd)
{
    return 1;
}

int RCTPrepareAlgorithm(RabbitCtGlobalData * rcgd)
{
    OutShadow shadow;

    if (rcgd->numberOfProjections == 0)
    {
        fprintf(stderr,"This module needs global geometry information.\nPlease add -a switch!\n");
        exit(0);
    }

    Range = (LineRange**) malloc(rcgd->numberOfProjections * sizeof(LineRange*));

    for (int i=0; i<rcgd->numberOfProjections; i++)
    {
        Range[i] = (LineRange*) malloc(rcgd->problemSize * rcgd->problemSize *sizeof(LineRange));
    }

    memoryUtils_init();
    computeShadowOfProjection(rcgd, &shadow);
    computeLineRanges(rcgd, Range);

    Padding.extend.Umin =  abs(shadow.Umin);
    Padding.extend.Umax =  abs(shadow.Umax) -  rcgd->imageWidth;
    Padding.extend.Vmin =  abs(shadow.Vmin);
    Padding.extend.Vmax =  abs(shadow.Vmax) - rcgd->imageHeight;

    printf("Padding\n");
    printf("bottom:%d \n",Padding.extend.Vmin);
    printf("top: %d \n",Padding.extend.Vmax);
    printf("left: %d \n",Padding.extend.Umin);
    printf("right: %d \n",Padding.extend.Umax);

    memoryUtils_zeroPadInit(rcgd, &Padding);

    return 1;
}


int RCTUnloadAlgorithm(RabbitCtGlobalData * rcgd)
{
    return 1;
}


#define BLOCKING_FACTOR 8

int RCTAlgorithmBackprojection(RabbitCtGlobalData * rcgd)
{
    const int L = rcgd->problemSize;
    float* const vol = rcgd->volumeData;
    const float MM = rcgd->voxelSize;

    if (!init)
    {
        printf("Allocating Padded Images\n");
        memoryUtils_zeroPadAllocate(rcgd, &Padding );
        init = 1;
    }

    //    memoryUtils_zeroPadEnter(rcgd,&Padding);


#pragma omp parallel
    {
        Projection* projectionBuffer = (Projection*) malloc(rcgd->numberOfProjections * sizeof(Projection));
        int imageWidth = Padding.lineOffset;
        memoryUtils_zeroPadEnterExp(rcgd,&Padding, projectionBuffer );
#pragma omp barrier

#pragma omp for schedule(runtime)
        for(int z=0; z<L; z++) 
        {
            unsigned int offset;
            float tmp[3];

            float wz=z*MM + rcgd->O_Index;

            for (int y=0; y<L; y+=BLOCKING_FACTOR)
            {
                for (int proj=0; proj<rcgd->numberOfProjections; proj++)
                {
                    for (int yh=0; yh<BLOCKING_FACTOR; yh++)
                    {
                        int yn = (y+yh);
                        float wy=yn*MM + rcgd->O_Index;
                        const double* const A = projectionBuffer[proj].matrix;
                        const float*  const I = projectionBuffer[proj].image;
                        int start  = Range[projectionBuffer[proj].id-1][z*L+yn].start;
                        int count  = Range[projectionBuffer[proj].id-1][z*L+yn].stop-
                        Range[projectionBuffer[proj].id-1][z*L+yn].start;

                        offset = z*L*L + yn*L+start;
                        float wx= start*MM + rcgd->O_Index;

                        if (start < 0)
                        {
                            continue;
                        }

                        tmp[0] = (float) (A[3]*wy + A[6]*wz + A[9]);
                        tmp[1] = (float) (A[4]*wy + A[7]*wz + A[10]);
                        tmp[2] = (float) (A[5]*wy + A[8]*wz + A[11]);

                        __attribute__((aligned(16))) float WX[4];
                        __attribute__((aligned(16))) float ISX4[4];
                        __attribute__((aligned(16))) float MM4[4];
                        __attribute__((aligned(16))) float A0[4];
                        __attribute__((aligned(16))) float A1[4];
                        __attribute__((aligned(16))) float A2[4];
                        __attribute__((aligned(16))) float TMP0[4];
                        __attribute__((aligned(16))) float TMP1[4];
                        __attribute__((aligned(16))) float TMP2[4];

                        float* APTR[3];
                        float* TMP[3];

                        APTR[0] = A0;
                        APTR[1] = A1;
                        APTR[2] = A2;
                        TMP[0] = TMP0;
                        TMP[1] = TMP1;
                        TMP[2] = TMP2;

                        /* Initialize Vector register data */
                        WX[0] = wx;
                        WX[1] = wx + MM;
                        WX[2] = wx + 2.0 * MM;
                        WX[3] = wx + 3.0 * MM;

                        for (int i = 0; i < 4; i++)
                        {
                            ISX4[i] = (float)imageWidth ;
                            MM4[i] = 4.0 * MM;
                            A0[i] = A[0];
                            A1[i] = A[1];
                            A2[i] = A[2];
                            TMP0[i] = tmp[0];
                            TMP1[i] = tmp[1];
                            TMP2[i] = tmp[2];
                        }

                        fastrabbit(APTR,   /* projection matrix component ptrs */
                                I,      /* projection image  */
                                TMP,    /* temp component ptrs */
                                vol+offset, /* Volume image ptr */
                                WX,     /* float ptr */
                                MM4,    /* float ptr */
                                ISX4,    /* integer constant */
                                count);     /* stopping criterion */
                    }
                }
            }
        }
        free(projectionBuffer);
    }

    return 1;
}



