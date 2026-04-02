#pragma once

#include "xai_def.h"

class XAITypeBase;

#if defined(_WIN32)
#define XAI_TYPE_AQ_C_API extern "C" __declspec(dllimport)
#else
#define XAI_TYPE_AQ_C_API extern "C"
#endif

XAI_TYPE_AQ_C_API XAITypeBase *createInstance();
XAI_TYPE_AQ_C_API void destroyInstance(XAITypeBase *instance);

XAI_TYPE_AQ_C_API int xai_start_instance(XAITypeBase *instance, const XAI_config_t *cfg);
XAI_TYPE_AQ_C_API int xai_stop_instance(XAITypeBase *instance);
XAI_TYPE_AQ_C_API int xai_do_process_roi_instance(
    XAITypeBase *instance,
    XAI_src_t *src_i,
    int offset_x,
    int offset_y,
    XAI_roi_t *roi);
