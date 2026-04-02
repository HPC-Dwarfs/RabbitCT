#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#include <types.h>
#include <likwid.h>
#include <ctFileReader.h>
#include <moduleLoader.h>
#include <rabbitProgress.h>
#include <rabbitTimer.h>
#include <memoryUtils.h>
#include <likwid.h>

#define  US_ROUND(x) (floorf((x)+0.5f))

#define HELP_MSG \
printf("RabbitRunner --  Version 0.1 \n\n"); \
printf("Supported Options:\n"); \
printf("Example usage: ./RabbitRunner -i filename \n"); \
printf("-h\t Help message\n"); \
printf("-v\t verbose output\n"); \
printf("-i\t Specify input filename\n"); \
printf("-a\t Specify geometry filename\n"); \
printf("-s\t problem size: 128, 256, 512, 1024\n"); \
printf("-b\t buffer size of projection images\n"); \
printf("-c\t check error\n"); \
printf("-C\t Specify line clipping filename\n"); \
printf("-o\t output volume image\n"); \
printf("-m\t module path\n\n")


static float volumeResolution(const int problemSize)
{
    float rv = 0.0f;

    switch (problemSize)
    {
        case 128:
            rv = 2.0f;
            break;
        case 256:
            rv = 1.0f;
            break;
        case 512:
            rv = 0.5f;
            break;
        case 1024:
            rv = 0.25f;
            break;
        default:
            printf("Wrong problem size, check again!");
            exit(EXIT_FAILURE);
    }

    return rv;
}


int main (int argc, char** argv)
{ 
    int c;
    int size = 0;
    int bufferSize = 1;
    uint64_t time = 0.0;
    int opt_verbose = 0;
    char* inFilename = NULL;
    char* outFilename = NULL;
    char* refFilename = NULL;
    char* clipFilename = NULL;
    char* analysisFile = NULL;
    char* modulePath = NULL;
    int globalNumberOfProjections = 0;
    RabbitCtGlobalData data;
    double error = 0.0;
    double maxError = 0.0;
    uint64_t numberOfVoxel = 0;
    RabbitCtFile inputFile;

    if (argc ==  1) { 
        HELP_MSG; 
        exit (EXIT_SUCCESS);    
    }

    while ((c = getopt (argc, argv, "+o:b:a:C:c:m:s:i:hv")) != -1)
    {
        switch (c)
        {
            case 'h':
                HELP_MSG;
                exit (EXIT_SUCCESS);    
            case 'v':
                opt_verbose = 1;
                break;
            case 'i':
                inFilename = (char*) malloc(strlen(optarg)+1);
                strcpy(inFilename, optarg);
                break;
            case 'a':
                analysisFile = (char*) malloc(strlen(optarg)+1);
                strcpy(analysisFile, optarg);
                break;
            case 's':
                size = atoi(optarg);
                break;
            case 'b':
                bufferSize = atoi(optarg);
                break;
            case 'C':
                clipFilename = (char*) malloc(strlen(optarg)+1);
                strcpy(clipFilename, optarg);
                break;
            case 'c':
                refFilename = (char*) malloc(strlen(optarg)+1);
                strcpy(refFilename, optarg);
                break;
            case 'o':
                outFilename = (char*) malloc(strlen(optarg)+1);
                strcpy(outFilename, optarg);
                break;
            case 'm':
                modulePath = (char*) malloc(strlen(optarg)+1);
                strcpy(modulePath, optarg);
                break;
            default:
                HELP_MSG;
                exit(EXIT_FAILURE);
        }
    }

    if (!inFilename || !modulePath)
    {
        HELP_MSG;
        exit(EXIT_FAILURE);
    }

    LIKWID_MARKER_INIT;
    rabbitTimer_init();

    /********************************************************
     * LOAD ALGORITHM
     * *****************************************************/
    if (!moduleLoader_loadSharedLibrary(modulePath)) {
        exit(EXIT_FAILURE);
    }

    data.voxelSize = volumeResolution(size);
    data.problemSize = size;
    data.O_Index = -0.5f * data.voxelSize * ( (float)(data.problemSize) - 1.0f );

    numberOfVoxel = size*size*size;
    memoryUtils_allocate(&data.volumeData, numberOfVoxel);

    ctFileReader_openFile(inFilename, &inputFile);

    globalNumberOfProjections = inputFile.header.numberOfImages;
    data.imageWidth  = inputFile.header.imageDimension[0];
    data.imageHeight = inputFile.header.imageDimension[1];
    data.numberOfProjections = 0;

    rabbitProgress_init(globalNumberOfProjections, size);

    /* NULL if -C was not used */
    data.clipFilename = clipFilename;

    /********************************************************
     * OPTIONAL: LOAD GLOBAL GEOMETRY
     * *****************************************************/
    if (analysisFile != NULL)
    {
        data.numberOfProjections = globalNumberOfProjections;
        data.globalGeometry = (double*) malloc(globalNumberOfProjections * 12 * sizeof(double));
        RabbitCtFile geometryFile;
        ctFileReader_openFile(analysisFile, &geometryFile);
        ctFileReader_readGeometry(&geometryFile, data.globalGeometry);
        ctFileReader_close(&geometryFile);
    }

    /********************************************************
     * PREPARE ALGORITHM
     * *****************************************************/
    if (s_fncPrepareAlgorithm != NULL) {
        CyclesData cycleData;

        rabbitTimer_startCycles(&cycleData);
        s_fncPrepareAlgorithm(&data);
        rabbitTimer_stopCycles(&cycleData);

        if (opt_verbose)
        {
            printf("Prepare Algorithm Timing: %g sec\n",
                    (double) rabbitTimer_printCyclesTime(&cycleData)) / (double) 1000000.0;
        }
    }

	s_fncLoadAlgorithm(&data);

    /********************************************************
     * BACKPROJECTION LOOP
     * *****************************************************/
    {
        printf("Running ... this may take some time.\n");
        if( opt_verbose )
        {
            printf("Processing %d projections with size %d X %d\n",
                    globalNumberOfProjections,
                    data.imageWidth,
                    data.imageHeight);

            printf("Buffering %d projections per call.\n",bufferSize);
            printf("Parameters:\n");
            printf("Problem size: %d\n",data.problemSize);
            printf("Voxel size: %g\n",data.voxelSize);
        }

        int counter = globalNumberOfProjections;
        CyclesData cycleData;
        data.numberOfProjections = bufferSize;
        data.projectionBuffer = (Projection*)
            malloc(bufferSize * sizeof(Projection));

        for (int i=0; i<bufferSize; i++)
        {
            data.projectionBuffer[i].matrix = (double*) malloc(12 * sizeof(double));
            memoryUtils_allocate(&data.projectionBuffer[i].image, data.imageWidth*data.imageHeight); 
        }


        while (counter > 0)
        {
            if (opt_verbose)
            {
                printf("Processing buffered run - %d left\n",counter);
            }

            /********************************************************
             * BUFFER PROJECTIONS
             * *****************************************************/
            for (int i=0; i<bufferSize; i++)
            {
                data.projectionBuffer[i].id = 
                    ctFileReader_readImage(&inputFile,
                            data.projectionBuffer[i].matrix,
                            data.projectionBuffer[i].image);
            }
            rabbitTimer_startCycles(&cycleData);
            s_fncAlgorithmIteration(&data);
            rabbitTimer_stopCycles(&cycleData);
            time += rabbitTimer_printCyclesTime(&cycleData);

            counter -= bufferSize;
            rabbitProgress_progress(globalNumberOfProjections - counter);
        }
    }

    s_fncFinishAlgorithm(&data);

    /********************************************************
     * OPTIONAL VERIFY RESULT
     * *****************************************************/
    if (refFilename != NULL)
    {
        const int magicNum    = 591984;
        const int magicNumEnd = 489195;
        uint16_t*  referenceVolume =
            (uint16_t*) malloc(numberOfVoxel * sizeof(uint16_t));
        uint64_t*  runtime =
            (uint64_t*) malloc(globalNumberOfProjections * sizeof(uint64_t));
        FILE* file =  fopen(refFilename, "r");
        FILE* resFile =  fopen("result.rctd", "w");

        if (file != NULL)
        {
            if (! fread((void*) referenceVolume, sizeof(uint16_t),
                        numberOfVoxel, file))
            {
                printf("Failed to read reference file !\n");
            }
        }
        else
        {
            printf("Failed to read reference file!\n");
        }

        fclose(file);

        float* rec_volume = data.volumeData;
        float* HUS = inputFile.header.HUScalingFactors;
        float HU_ax; 
        const int NUM_BINS = 11;
        // 0-1, 1-2, 2-3, 3-4,  ... 9-10, >10
        uint32_t errorHist[NUM_BINS];
        memset(errorHist, 0, NUM_BINS*sizeof(uint32_t));

        for (uint32_t i=0; i<numberOfVoxel; i++)
        {
            if (rec_volume[i] > 0.0f)
            {
                HU_ax = US_ROUND( (HUS[0] * rec_volume[i] + HUS[1]));
            }
            else
            {
                HU_ax = 0.0f;
            }

            if (HU_ax > 4095.0f)
            {
                HU_ax = 4095.0f;
            }

            const float cerr = (HU_ax - (float)(referenceVolume[i]));
            const float absErr = fabs(cerr);

            maxError = MAX( maxError, (double) absErr );
            error += cerr * cerr;
            rec_volume[i] = HU_ax;
            uint32_t histIdx = (uint32_t) floor(absErr);

            if (histIdx > NUM_BINS-1)
            {
                histIdx = NUM_BINS-1;
            }

            ++errorHist[histIdx];
        }
        error /= (double)numberOfVoxel;

        const double psnr = 10.0 * log10(4095.0 * 4095.0 / error);
        printf("\n--------------------------------------------------------------\n");
        printf("Quality of reconstructed volume:\n");
        printf("Root Mean Squared Error: %g HU\n", sqrt(error));
        printf("Mean Squared Error: %g HU^2\n",error);
        printf("Max. Absolute Error: %g HU\n",maxError);

        if (error > 1e-8)
        {
            printf("PSNR: %g db\n", psnr);
        }
        else
        {
            printf("PSNR: INF db\n");
        }

        /* the reference RabbitCtRunner implementation saves the 
         * runtime of every image. So we compute the average time
         * per image here since we only measure the total runtime.*/
        uint64_t meantime = time / globalNumberOfProjections;
        printf("mean time [usec]: %lld \n", LL_CAST meantime);

        for (int i=0; i<globalNumberOfProjections; i++)
        {
            runtime[i] = meantime;
        }

        fwrite(&magicNum,sizeof(int),1,resFile);
        fwrite(&size,sizeof(int),1,resFile);
        fwrite(&inputFile.header.numberOfImages,sizeof(uint32_t),1,resFile);
        fwrite(runtime,sizeof(uint64_t),globalNumberOfProjections,resFile);
        fwrite((rec_volume +(data.problemSize/2 - 1) * data.problemSize * data.problemSize),
                sizeof(float),data.problemSize * data.problemSize,resFile);
        fwrite(&error,sizeof(double),1,resFile);
        fwrite(&maxError,sizeof(double),1,resFile);
        fwrite(&errorHist,sizeof(uint32_t), NUM_BINS,resFile);
        fwrite(&magicNumEnd,sizeof(int),1,resFile);

        fclose(resFile);

        /* check error of timing conversion */
        {
            uint64_t newtime = 0ULL;

            for (int i=0; i<globalNumberOfProjections; i++)
            {
                newtime += runtime[i];
            }

            printf("Error [usec]: %lld \n", LL_CAST (int64_t) newtime - (int64_t) time);
        }
    }


    /********************************************************
     * OPTIONAL  WRITE RESULT
     * *****************************************************/
    if (outFilename != NULL)
    {
        FILE* file =  fopen(outFilename, "w");
        fwrite(data.volumeData, sizeof(float),numberOfVoxel, file);
        fclose(file);
    }

    /********************************************************
     * OUTPUT RESULT
     * *****************************************************/

	double time_mean = (double) time / (double) (1000.0 * (double) globalNumberOfProjections);
    printf("\n--------------------------------------------------------------\n");
    printf("Runtime statistics:\n");
    printf("Total: %g s\n", (double) time /  (double) 1000000.0);
    printf("Average: %g ms \n\n",  (time_mean ));

    LIKWID_MARKER_CLOSE;

    return EXIT_SUCCESS;
}



