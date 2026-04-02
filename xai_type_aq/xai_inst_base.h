/******************************************************************************
 *
 * Copyright (C) 2012 XNDT Tech Inc. All rights reserved.
 *
 * FILE NAME   : xai_inst_base.h
 * AUTHOR      : shaofei    < zhousf04@163.com >
 * CREATED     : Fri Mar 30 23:44:00 2012
 * VERSION     : 1.0
 *
 * DESCRIPTION :
 *
 *    xai_inst_base
 *
 * HISTORY     :
 *
 *  2012/03/30 - version 1.0
 *
 *******************************************************************************/

#ifndef _XAI_INST_BASE_H_
#define _XAI_INST_BASE_H_


#include <opencv2/opencv.hpp>

#include "xai_meth.h"

// ---------------------------------------------------------
// class XAIInstBase
// -------
class XAIInstBase
{
public:
    XAIInstBase();
    virtual ~XAIInstBase();

    void init( const char* split_name = nullptr, const char* type_name = nullptr );
    void finalize();

    virtual int start(const XAI_config_t *t_cfg);
    virtual int stop(void);

    virtual int gen_roi_lst(XAI_src_t *src_i, cv::Point offset, std::vector<XAI_roi_t> &roi_lst,
                     std::vector<XAI_roi_t> &roi_byd_lst, int is_scanning);

    virtual int preprocess_roi_lst(XAI_src_t *src_i, cv::Point offset, std::vector<XAI_roi_t> &roi_lst, int xdas_pw);
    virtual int process_roi_lst(XAI_src_t *src_i, cv::Point offset, std::vector<XAI_roi_t> &roi_lst,
                         unsigned long long idx, unsigned long long tm, int fps_us);


    virtual int draw_roi_lst(cv::Mat &rgb, cv::Point offset, std::vector<XAI_roi_t> &roi_lst,
                      int is_scanning, int mirror_tb, int mirror_lr);

    virtual int do_process_roi(XAI_src_t *src_i, cv::Point offset, XAI_roi_t &roi);

public:

    int     cnt_;
protected:
    XAI_config_t *cfg_;
private:
    void    *split_;
    void    *type_;


};

#endif /* _XAI_INST_BASE_H_ */
