#ifndef _ADAPTIVE_SUBTITLE_SDK_H
#define _ADAPTIVE_SUBTITLE_SDK_H
#endif

#include "common/iva_adaptive_subtitle_common.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SUBTITLE_DETECT_NUM         50

typedef struct
{
    int x;
    int y;
} subtitle_point;

typedef struct __nn_subtitle_out_t
{
    unsigned int   detNum;
    subtitle_point pos[MAX_SUBTITLE_DETECT_NUM][4];
}subtitle_out_t;

aai_iva_adaptive_subtitle_handle_t adaptive_subtitle_init(const char *path);
void* adaptive_subtitle_process(aai_iva_adaptive_subtitle_handle_t qcontext,
                                unsigned char *in_addr,
                                int inputWidth, int inputHeight, int type,
                                int step, subtitle_out_t *nnOut);
void* adaptive_subtitle_uninit(aai_iva_adaptive_subtitle_handle_t context);
int isAdaptiveSubtitleInterfaceImplement();
void convert_to_aaiMat(aaiMat& aai_mat, unsigned char *in_addr,
                       const int inputWidth, const int inputHeight, int type,
                       const unsigned long step = 0x7FFFFFFF);

#ifdef __cplusplus
} //extern "C"
#endif
