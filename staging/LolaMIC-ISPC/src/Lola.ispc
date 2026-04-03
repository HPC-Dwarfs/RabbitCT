export uniform int width() {
    return programCount;
}

export void Backprojection(
        uniform int start,
        uniform int stop,
		uniform float MM,         /* voxel size */
		uniform unsigned int imageWidth,
		uniform unsigned int imageHeight,
		uniform float A0,
		uniform float A1,
		uniform float A2,
		uniform float tmp_u,
		uniform float tmp_v,
		uniform float tmp_w,
		uniform float O_Index,
		uniform float* uniform VOL,       /* voxel volume data */
		uniform float* uniform I          /* image data */
		)
{

	foreach (i = start ... stop) {
		float wx = i * MM + O_Index;
		float u = A0 * wx + tmp_u;
		float v = A1 * wx + tmp_v;
		float w = A2 * wx + tmp_w;

        float rcp_w = 1.0f/w;

        float rcp2_w = rcp_w * rcp_w;
        float ix = u * rcp_w;
        float iy = v * rcp_w;

		int iix = (int)ix;
		int iiy = (int)iy;

		float scalx = ix - (float) iix;
		float scaly = iy - (float) iiy;

        float valtl, valtr, valbl, valbr;

        valtl = I[iiy*imageWidth + iix];
        valtr = I[iiy*imageWidth + iix+1];
        valbl = I[(iiy+1)*imageWidth + iix];
        valbr = I[(iiy+1)*imageWidth + iix+1];

		float vall = scaly * valbl + (1.0f-scaly) * valtl;
		float valr = scaly * valbr + (1.0f-scaly) * valtr;
		float val  = scalx * valr  + (1.0f-scalx) * vall;

		VOL[i] += val * rcp2_w;
	}
}
