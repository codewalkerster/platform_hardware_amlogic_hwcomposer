#ifndef AAI_IVA_ADAPTIVE_SUBTITLE_COMMON_H
#define AAI_IVA_ADAPTIVE_SUBTITLE_COMMON_H

#include <vector>
#include <string>

#include "iva/iva.hpp"


typedef struct adaptive_subtitle_runtime_struct *adaptive_subtitle_context_runtime_handle_t;

typedef struct aai_iva_adaptive_subtitle_input {
    aai_iva_adaptive_subtitle_input() : need_padded(0), scale(1.0f) {

    }

    int need_padded = 0;
    float scale     = 1.0f;

    aaiMat rgb_image_scaled;
    aaiMat rgb_image;
} aai_iva_adaptive_subtitle_input_t;

typedef struct aai_iva_adaptive_subtitle_node {
    std::vector<aaiPoint>               text_coordinates;      //< The coordinates of the text
    std::string                         recog_str;             //< Recognized text content
} aai_iva_adaptive_subtitle_node_t;

typedef struct aai_iva_adaptive_subtitle_output {
    std::vector<aai_iva_adaptive_subtitle_node_t> iva_adaptive_subtitles;
} aai_iva_adaptive_subtitle_output_t;

typedef struct aai_iva_adaptive_subtitle_param {
    aai_iva_adaptive_subtitle_param() : text_detect_enable(1), text_recog_enable(1),
    need_padded(0), model_path("/etc/aai_nndata") {

    }
    int text_detect_enable = 1;
    int text_recog_enable = 1;
    int need_padded = 0;

    char model_path[256] = "/etc/aai_nndata";
} aai_iva_adaptive_subtitle_param_t;

typedef struct aai_iva_adaptive_subtitle_sturct {
    adaptive_subtitle_context_runtime_handle_t                 ctx_adaptive_subtitle_runtime;
} aai_iva_adaptive_subtitle_struct_t, *aai_iva_adaptive_subtitle_handle_t;

#endif