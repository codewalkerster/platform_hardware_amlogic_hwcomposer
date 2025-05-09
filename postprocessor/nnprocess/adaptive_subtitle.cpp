#define LOG_NDEBUG 0
#define LOG_TAG "hwc_aisubtitle"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <utils/Log.h>
#include "adaptive_subtitle.h"


static void *mAdaptiveSubtitleHandle;
static aai_iva_adaptive_subtitle_handle_t (*func_adaptive_subtitle_init)(const aai_iva_adaptive_subtitle_param_t&);
static IVA_STATUS_E (*func_adaptive_subtitle_process)(aai_iva_adaptive_subtitle_handle_t,
                                               aai_iva_adaptive_subtitle_input_t&,
                                               aai_iva_adaptive_subtitle_output_t&);
static IVA_STATUS_E (*func_adaptive_subtitle_deinit)(aai_iva_adaptive_subtitle_handle_t);
static IVA_STATUS_E (*func_adaptive_subtitle_clear)(aai_iva_adaptive_subtitle_output_t&);

void convert_to_aaiMat(aaiMat& aai_mat, unsigned char *in_addr,
                       const int inputWidth, const int inputHeight, int type,
                       const unsigned long step) {
    if (NULL == in_addr) {
        return;
    }

    if (inputWidth <= 0 || inputHeight <= 0) {
        return;
    }

    aai_mat.rows = inputHeight;
    aai_mat.cols = inputWidth;
    aai_mat.type = type;
    aai_mat.data = in_addr;
    aai_mat.step = step;
}

void* adaptive_subtitle_process(aai_iva_adaptive_subtitle_handle_t qcontext,
                                unsigned char *in_addr,
                                int inputWidth, int inputHeight, int type,
                                int step, subtitle_out_t *nnOut) {
    if (qcontext == NULL) {
        ALOGD("qcontext is fail is NULL\n");
        return NULL;
    }

    if (in_addr == NULL) {
        ALOGD("in_addr is NULL\n");
        return NULL;
    }

    memset(nnOut, 0, sizeof(nnOut));

    aai_iva_adaptive_subtitle_input_t iva_adaptive_subtitle_input;
    aai_iva_adaptive_subtitle_output_t iva_adaptive_subtitle_output;
    convert_to_aaiMat(iva_adaptive_subtitle_input.rgb_image, in_addr, inputWidth, inputHeight, type, step);

    int ret = func_adaptive_subtitle_process(qcontext, iva_adaptive_subtitle_input, iva_adaptive_subtitle_output);
    if (ret != IVA_STATUS_OK) {
        ALOGD("The aai iva adaptive_subtitle detect fail\n");
        return NULL;
    }

    unsigned int text_num = iva_adaptive_subtitle_output.iva_adaptive_subtitles.size();

    if (text_num > MAX_SUBTITLE_DETECT_NUM) {
        text_num = MAX_SUBTITLE_DETECT_NUM;
    }

    nnOut->detNum = text_num;

    for (int i = 0; i < text_num; i++) {
        aai_iva_adaptive_subtitle_node_t adaptive_subtitle_node = iva_adaptive_subtitle_output.iva_adaptive_subtitles[i];
        std::vector<aaiPoint> subtitle_box = adaptive_subtitle_node.text_coordinates;
        for (int j = 0; j < 4; j++) {
            nnOut->pos[i][j].x = subtitle_box[j].x;
            nnOut->pos[i][j].y = subtitle_box[j].y;
        }
    }

    func_adaptive_subtitle_clear(iva_adaptive_subtitle_output);

    return nnOut;
}

aai_iva_adaptive_subtitle_handle_t adaptive_subtitle_init(const char *path) {
    aai_iva_adaptive_subtitle_handle_t iva_adaptive_subtitle_handle;
    aai_iva_adaptive_subtitle_param_t adaptive_subtitle_init_param;
    adaptive_subtitle_init_param.text_detect_enable = 1;
    adaptive_subtitle_init_param.text_recog_enable = 0;

    strcpy(adaptive_subtitle_init_param.model_path, path);
    iva_adaptive_subtitle_handle = func_adaptive_subtitle_init(adaptive_subtitle_init_param);
    if (iva_adaptive_subtitle_handle == NULL) {
        ALOGD("The iva adaptive_subtitle init fail\n");
    }

    return iva_adaptive_subtitle_handle;
}

void* adaptive_subtitle_uninit(aai_iva_adaptive_subtitle_handle_t context) {
    if (context != NULL) {
       func_adaptive_subtitle_deinit(context);
    }

    if (mAdaptiveSubtitleHandle != NULL) {
        dlclose(mAdaptiveSubtitleHandle);
        mAdaptiveSubtitleHandle = NULL;
    }

    return NULL;
}

int isAdaptiveSubtitleInterfaceImplement() {
    ALOGD("enter %s.\n", __FUNCTION__);

    int ret = 0;

    if (mAdaptiveSubtitleHandle == NULL)
        mAdaptiveSubtitleHandle = dlopen("libaaisdk.so", RTLD_NOW);

    if (mAdaptiveSubtitleHandle == NULL) {
        ALOGD("open libaaisdk.so fail: %s.\n", dlerror());
    } else {
        func_adaptive_subtitle_init = (aai_iva_adaptive_subtitle_handle_t (*)(const aai_iva_adaptive_subtitle_param_t&))
            dlsym(mAdaptiveSubtitleHandle, "aai_iva_adaptive_subtitle_init");
        if (func_adaptive_subtitle_init == NULL)
            ALOGD("func_subtitle_init don't implement.\n");

        func_adaptive_subtitle_process = (IVA_STATUS_E (*)(aai_iva_adaptive_subtitle_handle_t,
                                               aai_iva_adaptive_subtitle_input_t&,
                                               aai_iva_adaptive_subtitle_output_t&))
            dlsym(mAdaptiveSubtitleHandle, "aai_iva_adaptive_subtitle_process");

        if (func_adaptive_subtitle_process == NULL)
            ALOGD("func_adaptive_subtitle_process don't implement.\n");

        func_adaptive_subtitle_deinit = (IVA_STATUS_E (*)(aai_iva_adaptive_subtitle_handle_t))
            dlsym(mAdaptiveSubtitleHandle, "aai_iva_adaptive_subtitle_deinit");

        if (func_adaptive_subtitle_deinit == NULL)
            ALOGD("func_adaptive_subtitle_deinit don't implement.\n");

        func_adaptive_subtitle_clear = (IVA_STATUS_E (*)(aai_iva_adaptive_subtitle_output_t&))
            dlsym(mAdaptiveSubtitleHandle, "aai_iva_adaptive_subtitle_clear");

        if (func_adaptive_subtitle_clear == NULL)
            ALOGD("func_adaptive_subtitle_clear don't implement.\n");
    }

    if ((mAdaptiveSubtitleHandle == NULL)
            || (func_adaptive_subtitle_init == NULL)
            || (func_adaptive_subtitle_process == NULL)
            || (func_adaptive_subtitle_deinit == NULL)
            || (func_adaptive_subtitle_clear == NULL)) {
            ALOGD("adaptive subtitle interface don't implement.\n");
    } else {
            ALOGD("adaptive subtitle is implement in.\n");
            ret = 1;
    }

    return ret;
}