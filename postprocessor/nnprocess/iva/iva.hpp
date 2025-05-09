/*
 *   The confidential and proprietary information contained in this file may
 *   only be used by a person authorized under and to the extent permitted
 *   by a subsisting licensing agreement from the owner.
 *
 *   This entire notice must be reproduced on all copies of this file
 *   and copies of this file may only be made by a person if such person is
 *   permitted to do so under the terms of a subsisting license agreement
 *   from the owner.
 */

#ifndef _AAI_IVA_H_
#define _AAI_IVA_H_

#include <vector>
#include <stdint.h>
#include <cstddef>

typedef enum
{
    IVA_STATUS_MIX_LINE = 1,
    IVA_STATUS_OK = 0,
    IVA_STATUS_ERROR = -1,
    IVA_STATUS_OUT_OF_MEMORY = -2,
} IVA_STATUS_E;



struct aaiMat {
    aaiMat(): rows(0), cols(0), type(16),data(NULL), step(0x7FFFFFFF){}
    aaiMat(int i_height, int i_width, int i_type, uint8_t *i_data, unsigned long i_step): rows(i_height), cols(i_width), type(i_type),data(i_data),step(i_step){}
    aaiMat(int i_height, int i_width, int i_type, uint8_t *i_data): rows(i_height), cols(i_width), type(i_type),data(i_data), step(0x7FFFFFFF){}
    aaiMat(int i_height, int i_width, uint8_t *i_data): rows(i_height), cols(i_width), type(16),data(i_data),step(0x7FFFFFFF){}

    ~aaiMat(){
        rows=0;
        cols=0;
        type=16;
        data=NULL;
        step = 0x7FFFFFFF;
    }
    int rows=0; //height
    int cols=0; //width
    //it should be same as cv::Mat type();
    //default is CV_8UC3=16
    //CV_8UC1=0, CV_16UC1=2, CV_16UC3=18, CV_8SC1=1, CV_8SC3=7
    int type=16;

    uint8_t *data=NULL; //data should be [HxWxC], C is RGB format, in memory, it is [ R00(first value) G00 B00 R01 G01 B01 R02 G02 B02.....]
    //data is only a point, it is copied from other array, no deinit for it.

    //Byte number for each line
    //it should be cols*elemSize()
    //default is 0x7FFFFFFF, it means continuous
    unsigned long  step=0x7FFFFFFF;
};

struct aaiRect2d {
    aaiRect2d(): x(0), y(0), width(0), height(0){}
    aaiRect2d(int i_x, int i_y, int i_width, int i_height): x(i_x), y(i_y), width(i_width), height(i_height){}
    int x;
    int y;
    int width;
    int height;
};
struct aaiRect2f {
    aaiRect2f(): x(0), y(0), width(0), height(0){}
    aaiRect2f(float i_x, float i_y, float i_width, float i_height): x(i_x), y(i_y), width(i_width), height(i_height){}
    float x;
    float y;
    float width;
    float height;
};

struct aaiPoint2d {
    aaiPoint2d(): x(0), y(0){}
    aaiPoint2d(int i_x, int i_y): x(i_x), y(i_y){}
    int x;
    int y;
};

struct aaiPoint2f {
    aaiPoint2f(): x(0), y(0){}
    aaiPoint2f(float i_x, float i_y): x(i_x), y(i_y){}
    float x;
    float y;
};

struct aaiPoint {
    aaiPoint(): x(0), y(0){}
    aaiPoint(float i_x, float i_y): x(i_x), y(i_y){}
    float x;
    float y;
};

#endif
