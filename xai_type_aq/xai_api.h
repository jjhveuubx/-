/******************************************************************************
 *
 * Copyright (C) 2013 XNDT Tech Inc. All rights reserved.
 *
 * FILE NAME   : xai_api.h
 * AUTHOR      : shaofei    < zhousf04@163.com >
 * CREATED     : Sat Nov  2 17:25:01 2013
 * VERSION     : 1.0
 *
 * DESCRIPTION :
 *
 *    xai_api
 *
 * HISTORY     :
 *
 *  2013/11/02 - created
 *
 *******************************************************************************/

#ifndef _XAI_API_H_
#define _XAI_API_H_



#if (defined WIN32 || defined _WIN32 || defined WINCE) && defined XAI_API_EXPORTS
#  define XAI_EXPORTS __declspec(dllexport)
#else
#  define XAI_EXPORTS
#endif

#ifdef __cplusplus
#  define XAI_EXTERN_C extern "C"
#else
#  define XAI_EXTERN_C
#endif

#if defined WIN32 || defined _WIN32
#  define XAI_CDECL __cdecl
#  define XAI_STDCALL __stdcall
#else
#  define XAI_CDECL
#  define XAI_STDCALL
#endif

#ifndef XAI_API
#  define XAI_API(rettype) XAI_EXTERN_C XAI_EXPORTS rettype XAI_CDECL
#endif


// user include
#include <opencv2/opencv.hpp>
#include <mmessage.h>


#include "xai_def.h"

// ---------------------------------------------------------
// XAI APIs
// -------
XAI_API(void) XAI_bind_msg_handler(m_msg_handler_t *handler);
XAI_API(const char*) XAI_get_version(void);


// ---------------------------------------------------------
// XAI APIs
// -------
class XAI_EXPORTS XAI
{
public:
    XAI(const char* split_name, const char* type_name);
    ~XAI();

    int start(const XAI_config_t *t_cfg);    //用于点击开始时，使配置参数生效，不支持参数在线更新，并清空roi_lst
    int stop(void);     //用于点击停止时，清空roi_lst

    int gen_roi_lst(XAI_src_t *src_i, cv::Point offset, std::vector<XAI_roi_t> &roi_lst,
                     std::vector<XAI_roi_t> &roi_byd_lst, int is_scanning);
    int preprocess_roi_lst(XAI_src_t *src_i, cv::Point offset, std::vector<XAI_roi_t> &roi_lst, int xdas_pw);
    int process_roi_lst(XAI_src_t *src_i, cv::Point offset, std::vector<XAI_roi_t> &roi_lst,
                         unsigned long long idx, unsigned long long tm, int fps_us);
    int draw_roi_lst(cv::Mat &rgb, cv::Point offset, std::vector<XAI_roi_t> &roi_lst,
                      int is_scanning, int mirror_tb, int mirror_lr);


private:
    void    *pdata_;

};


#endif /* _XAI_API_H_ */
