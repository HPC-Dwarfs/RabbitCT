/* Copyright (C) NHR@FAU, University Erlangen-Nuremberg.
 * All rights reserved. This file is part of RabbitCT.
 * Use of this source code is governed by a MIT style
 * license that can be found in the LICENSE file. */
#ifndef CTFILEREADER_H
#define CTFILEREADER_H

#include "types.h"

extern void ctFileReader_openFile(char *filename, RabbitCtFile *data);
extern int ctFileReader_readImage(RabbitCtFile *ctFile, double *matrix, float *image);
extern void ctFileReader_readGeometry(RabbitCtFile *ctFile, double *projectionData);
extern void ctFileReader_close(RabbitCtFile *ctFile);

#endif /* CTFILEREADER_H */
