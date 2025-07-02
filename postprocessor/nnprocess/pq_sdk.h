/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef _PQ_SDK_H
#define _PQ_SDK_H
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __nn_image_classify
{
    float   score[5];
    unsigned int  topClass[5];
}img_classify_out_t;

// void* init(const char *path, int model_type, int inputWidth, int inputHeight);
void* init(const char *path, int model_type, int inputWidth, int inputHeight, bool is_secure);
// void* process_network(void *context, unsigned char *rawdata);
void* process_network(void *qcontext,unsigned char *qrawdata, bool is_secure, unsigned char *paddr);
void* uninit(void* context);
int isPqInterfaceImplement();
#ifdef __cplusplus
} //extern "C"
#endif
