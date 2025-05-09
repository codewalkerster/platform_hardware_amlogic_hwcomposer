#ifndef AAI_IVA_ADAPTIVE_SUBTITILE_H
#define AAI_IVA_ADAPTIVE_SUBTITILE_H

#include "common/iva_adaptive_subtitle_common.hpp"

#ifdef __cplusplus
extern "C" {
#endif

__attribute ((visibility("default"))) aai_iva_adaptive_subtitle_handle_t aai_iva_adaptive_subtitle_init(const aai_iva_adaptive_subtitle_param_t& t_adaptive_subtitle_param);

__attribute ((visibility("default"))) IVA_STATUS_E aai_iva_adaptive_subtitle_process(aai_iva_adaptive_subtitle_handle_t iva_adaptive_subtitle_handle,
                                aai_iva_adaptive_subtitle_input_t& t_adaptive_subtitle_input,
                                aai_iva_adaptive_subtitle_output_t& t_adaptive_subtitle_output);

__attribute ((visibility("default"))) IVA_STATUS_E aai_iva_adaptive_subtitle_param_set(aai_iva_adaptive_subtitle_handle_t iva_adaptive_subtitle_handle, const aai_iva_adaptive_subtitle_param_t& t_adaptive_subtitle_param);

__attribute ((visibility("default"))) IVA_STATUS_E aai_iva_adaptive_subtitle_param_get(aai_iva_adaptive_subtitle_handle_t iva_adaptive_subtitle_handle, aai_iva_adaptive_subtitle_param_t& t_adaptive_subtitle_param);

__attribute ((visibility("default"))) IVA_STATUS_E aai_iva_adaptive_subtitle_deinit(aai_iva_adaptive_subtitle_handle_t iva_adaptive_subtitle_handle);

__attribute ((visibility("default"))) IVA_STATUS_E aai_iva_adaptive_subtitle_clear(aai_iva_adaptive_subtitle_output_t& t_adaptive_subtitle_output);

#ifdef __cplusplus
}
#endif //end of __cplusplus

#endif //end of iva_adaptive_subtitle.hpp