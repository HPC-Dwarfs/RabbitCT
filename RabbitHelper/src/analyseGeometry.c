#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <rabbitHelper_types.h>
#include <analyseGeometry.h>

static void
initCuboid(Point3D* s, Point3D corners[], Point3D* p)
{
    corners[0].x = p->x;
    corners[0].y = p->y;
    corners[0].z = p->z;

    corners[1].x = p->x;
    corners[1].y = p->y + s->y -1;
    corners[1].z = p->z;

    corners[2].x = p->x + s->x -1;
    corners[2].y = p->y;
    corners[2].z = p->z;

    corners[3].x = p->x + s->x -1;
    corners[3].y = p->y + s->y -1;
    corners[3].z = p->z;

    corners[4].x = p->x;
    corners[4].y = p->y;
    corners[4].z = p->z + s->z -1;

    corners[5].x = p->x;
    corners[5].y = p->y + s->y -1;
    corners[5].z = p->z + s->z -1;

    corners[6].x = p->x + s->x -1;
    corners[6].y = p->y;
    corners[6].z = p->z + s->z -1;

    corners[7].x = p->x + s->x -1;
    corners[7].y = p->y + s->y -1;
    corners[7].z = p->z + s->z -1;
}

void
computeShadowOfProjection(RabbitCtGlobalData* data, OutShadow* shadow)
{
    Point3D size = {data->problemSize, data->problemSize, data->problemSize};
    Point3D corners[8];
    Point3D principalCorner = {0,0,0};
    float  R_L = data->voxelSize;
    float  O_L = data->O_Index;
	double minU = data->imageWidth +1;
	double minV = data->imageHeight +1;
	double maxU = -2;
	double maxV = -2;

    initCuboid(&size, corners, &principalCorner);

    for(uint32_t view=0; view < data->numberOfProjections; view++)
    {
		double* A_n = data->globalGeometry + (view*12);

        for(int co=0; co < 8; co++)
        {
            Point3D p = corners[co];

            double x = O_L + (double)p.x * R_L;
            double y = O_L + (double)p.y * R_L;
            double z = O_L + (double)p.z * R_L;

            double w_n =  A_n[2] * x + A_n[5] * y + A_n[8] * z + A_n[11];
            double u_n = (A_n[0] * x + A_n[3] * y + A_n[6] * z + A_n[9] ) / w_n;
            double v_n = (A_n[1] * x + A_n[4] * y + A_n[7] * z + A_n[10]) / w_n;

            if (u_n < minU) minU = u_n;
            if (v_n < minV) minV = v_n;
            if (u_n > maxU) maxU = u_n;
            if (v_n > maxV) maxV = v_n;
        }
    }

	shadow->Umin = (int) floor(minU);
	shadow->Vmin = (int) floor(minV);
	shadow->Umax = (int) ceil(maxU);
	shadow->Vmax = (int) ceil(maxV);
}

#define COMPUTE_GEOMETRY \
     w_n =  A_n[2] * wx + A_n[5] * wy + A_n[8] * wz + A_n[11];   \
     u_n = (A_n[0] * wx + A_n[3] * wy + A_n[6] * wz + A_n[9] ) / w_n;  \
     v_n = (A_n[1] * wx + A_n[4] * wy + A_n[7] * wz + A_n[10]) / w_n; \
     iix = (int)u_n;  \
     iiy = (int)v_n


void
computeLineRanges(RabbitCtGlobalData* data, LineRange** range  )
{
    unsigned int L = data->problemSize;
    unsigned int ISX = data->imageWidth;
    unsigned int ISY = data->imageHeight;
    unsigned int N = data->numberOfProjections;
    const float MM=data->voxelSize;

    /* Check whether a filename was specified using the -C option,
     * if not fall back to the default filename "LR_PREFIXxxx.rct",
     * where LR_PREFIX is "RabbitInput/LineRange" and xxx is the
     * problem size. */
    char *clipFile = data->clipFilename;
    if (clipFile == NULL) {
        int nchars;
        switch (L) {
            case 128:
            case 256:
            case 512:
                nchars = strlen(LR_PREFIX) + 3 + strlen(".rct") + 1;
                break;
            case 1024:
                nchars = strlen(LR_PREFIX) + 4 + strlen(".rct") + 1;
                break;
            default:
                printf("Unsupported problem size: %d\n", L);
                exit(EXIT_FAILURE);
        }

        switch (VECTORSIZE) {
            case 4:
                // fall through
            case 8:
                nchars += strlen("-x");
                break;
            case 16:
                nchars += strlen("-xx");
                break;
            default:
                printf("Unsupported VECTORSIZE size: %d\n", VECTORSIZE);
                exit(EXIT_FAILURE);
        }

        if ((clipFile = (char *)malloc(nchars * sizeof(char))) == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        snprintf(clipFile, nchars, "%s%d-%d.rct", LR_PREFIX, L, VECTORSIZE);
        printf("No clipping file specified, falling back to %s\n", clipFile);
    }

    /* If filename exists, verify the size and load contents into range */
    FILE *fClipFile;
    if ((fClipFile = fopen(clipFile, "r")) != NULL) {
        /* verify file size */
        fseek(fClipFile, 0L, SEEK_END);
        int fsize = ftell(fClipFile);
        if (fsize != N * L * L * sizeof(LineRange)) {
            printf("corrupted file %s: size is %lu (should be %lu)\n", clipFile,
                    (unsigned long)fsize, N * L * L * sizeof(LineRange));
            exit(EXIT_FAILURE);
        }
        fseek(fClipFile, 0L, SEEK_SET);

        /* Read file NUMA-aware into buffer. */
        int nbytes;
        for (int i = 0; i < N; ++i) {
#pragma omp parallel for schedule(static) ordered
            for (int j = 0; j < L; j++) {
#pragma omp ordered
                {
                    nbytes = fread(range[i] + j * L, sizeof(LineRange), L,
                            fClipFile);
                    if (nbytes != L) {
                        fprintf(stderr, "error reading from %s: ", clipFile);
                        perror("");
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }

        printf("Read %d bytes of clipping data from %s\n", fsize, clipFile);
        fclose(fClipFile);
        /* done */
        return;
    } else {
        /* Can't open clipFile for reading. This means that the file either
         * doesn't exist (in which case we create it, calculate the LineRange
         * and store it in the file) or there was some other problem opening
         * the file (in which case we exit). */
        if (errno != ENOENT) {
            fprintf(stderr, "fopen %s: ", clipFile);
            perror("");
            exit(EXIT_FAILURE);
        }

        /* Open file for writing. */
        if ((fClipFile = fopen(clipFile, "w")) == NULL) {
            fprintf(stderr, "fopen %s: ", clipFile);
            perror("");
            exit(EXIT_FAILURE);
        }

        /* Calculate LineRange */
#pragma omp parallel for
        for(int view=0; view<data->numberOfProjections; view++)
        {
            double w_n;
            double u_n;
            double v_n;
            int iix, iiy;
            float wz = data->O_Index;
            for(int z=0; z<L; z++, wz+=MM) {
                double* A_n = data->globalGeometry + (view*12);
                float wy = data->O_Index;
                float wx;

                for (int y=0; y<L; y++, wy+=MM) {
                    int xstart = -1;
                    int xstop = -1;

                    // idea: we scan from left to right and set xstart to the first
                    // voxel thats ray hits the projection image and break the loop
                    // next: we scan from right to left and set xstop to the first
                    // voxel thats raw hits the projection image and break the loop

                    // scan from left to right
                    for (int x=0; x<L; x++) {
                        wx = x * MM + data->O_Index;
                        COMPUTE_GEOMETRY;

                        // continue with next voxel if we missed the projection
                        if ((iix+1 < 0) || (iix > ISX-1) || (iiy+1 < 0) || (iiy > ISY-1))
                            continue;

                        // we hit the projection image, this means that we have
                        // to include voxels starting from here
                        xstart = x;
                        break;
                    } // x-loop scan from left to right

                    // if we scanned from left to right and didn't set xstart in
                    // the process this means that we scanned the whole x-line
                    // without ever hitting the projection image
                    if (xstart == -1) {
                        range[view][z*L +y].start = 0;
                        range[view][z*L +y].stop = 0;
                        continue; // jump to next iteration in y-loop
                    }

                    // since we hit a voxel while scanning from left to right
                    // we now scan from right to left to find out where to stop
                    // (if at all)
                    for (int x=L-1; x>=0; --x) {
                        wx = x * MM + data->O_Index;
                        COMPUTE_GEOMETRY;

                        // continue with next voxel if we missed the projection
                        if ((iix+1 < 0) || (iix > ISX-1) || (iiy+1 < 0) || (iiy > ISY-1))
                            continue;

                        // we hit the projection image, this means that we have
                        // to include voxels up to here
                        xstop = x + 1;
                        break;
                    } // x-loop scan from right to left

                    // fix alignment
                    int align = 0;
                    if ((align = (xstop % VECTORSIZE)))
                        xstop += (VECTORSIZE-align);
                    if ((align = (xstart % VECTORSIZE)))
                        xstart -= align;

                    range[view][z*L +y].start = xstart;
                    range[view][z*L +y].stop = xstop;
                } // y-loop
            } // z-loop
        } // view-loop

        /* Write LineRange into file. */
        int nbytes;
        for (int i = 0; i < N; ++i) {
            nbytes = fwrite(range[i], sizeof(LineRange), L * L, fClipFile);
            if (nbytes != L * L) {
                fprintf(stderr, "error writing to %s: ", clipFile);
                perror("");
                exit(EXIT_FAILURE);
            }
        }
        printf("Wrote %d bytes of clipping data to %s\n",
                N * L * L * sizeof(LineRange), clipFile);
        fclose(fClipFile);
    }
}



