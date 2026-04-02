/******************************************************************************
 *
 * Copyright (C) 2026 XNDT Tech Inc. All rights reserved.
 *
 * FILE NAME   : xai_type_api.h
 * AUTHOR      : zhangzhichao
 * CREATED     : zhangzhichao
 * VERSION     : 1.0
 *
 * DESCRIPTION :
 *
 *    XAI Type API - 识别算法库对外接口
 *
 * HISTORY     :
 *
 *  Created - 初始版本
 *
 *******************************************************************************/

#pragma once

// user include
#include <opencv2/opencv.hpp>
#include <mmessage.h>
#include "xai_def.h"
#include "xai_inst_base.h"

class XAITypeBase : public XAIInstBase
{
public:
    XAITypeBase( );
    virtual ~XAITypeBase();

    static XAITypeBase* create( const char* type_name );

    virtual int start(const XAI_config_t *t_cfg);    //用于点击开始时，使配置参数生效，不支持参数在线更新，并清空roi_lst
    virtual int stop(void);
    virtual int do_process_roi(XAI_src_t *src_i, cv::Point offset, XAI_roi_t &roi);

};
