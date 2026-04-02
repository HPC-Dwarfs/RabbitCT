#ifndef ANALYSE_GEOMETRY_H
#define ANALYSE_GEOMETRY_H

#define LR_PREFIX   "RabbitInput/LineRange"

extern void computeShadowOfProjection(RabbitCtGlobalData* data, OutShadow* shadow);
extern void computeLineRanges(RabbitCtGlobalData* data, LineRange** range  );

#endif /*ANALYSE_GEOMETRY_H*/

