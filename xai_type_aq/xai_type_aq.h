/******************************************************************************
 *
 * Copyright (C) 2012 XNDT Tech Inc. All rights reserved.
 *
 * FILE NAME   : xai_type_aq.h
 *
 *******************************************************************************/

#ifndef _XAI_TYPE_AQ_H_
#define _XAI_TYPE_AQ_H_

#include <memory>
#include <string>

#include <opencv2/opencv.hpp>

#include "xai_type_base.h"

class XAITypeAQ : public XAITypeBase {
public:
    XAITypeAQ();
    ~XAITypeAQ() override;

    int get_cfg(XAI_config_t &cfg);
    int start(const XAI_config_t *t_cfg) override;
    int stop(void) override;
    int gen_roi_lst(
        XAI_src_t *src_i,
        cv::Point offset,
        std::vector<XAI_roi_t> &roi_lst,
        std::vector<XAI_roi_t> &roi_byd_lst,
        int is_scanning) override;
    int preprocess_roi_lst(XAI_src_t *src_i, cv::Point offset, std::vector<XAI_roi_t> &roi_lst, int xdas_pw) override;
    int process_roi_lst(
        XAI_src_t *src_i,
        cv::Point offset,
        std::vector<XAI_roi_t> &roi_lst,
        unsigned long long idx,
        unsigned long long tm,
        int fps_us) override;
    int draw_roi_lst(
        cv::Mat &rgb,
        cv::Point offset,
        std::vector<XAI_roi_t> &roi_lst,
        int is_scanning,
        int mirror_tb,
        int mirror_lr) override;
    int do_process_roi(XAI_src_t *src_i, cv::Point offset, XAI_roi_t &roi) override;

private:
    struct Impl;

    int StartImpl(const XAI_config_t *t_cfg);
    int DoProcessRoiImpl(XAI_src_t *src_i, cv::Point offset, XAI_roi_t &roi);

    std::unique_ptr<Impl> impl_;
    std::string model_path_;
    std::string input_node_name_;
    std::string result_node_name_;
    int run_loops_;
    bool model_loaded_;
};

#endif /* _XAI_TYPE_AQ_H_ */
