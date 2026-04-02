/******************************************************************************
 *
 * Copyright (C) 2013 XNDT Tech Inc. All rights reserved.
 *
 * FILE NAME   : xai_err.h
 * AUTHOR      : shaofei    < zhousf04@163.com >
 * CREATED     : Sat Nov  2 17:25:01 2013
 * VERSION     : 1.0
 *
 * DESCRIPTION :
 *
 *    xai_err
 *
 * HISTORY     :
 *
 *  2013/11/02 - created
 *
 *******************************************************************************/

#ifndef _XAI_ERR_H_
#define _XAI_ERR_H_


//错误码定义
#define XNDT_IMG_PROC_ERR_START                                 -10000  //根据具体情况定义错误码起始值

#define XNDT_ERR_LEARN_DATA             XNDT_IMG_PROC_ERR_START- 1   //自动学习时数据或参数错误
#define XNDT_ERR_LEARN_ROI_CNT          XNDT_IMG_PROC_ERR_START- 2   //多次自动学习时产品数量不一致
#define XNDT_ERR_RUN_DATA               XNDT_IMG_PROC_ERR_START- 3   //图像检测时数据或参数错误
#define XNDT_ERR_CROSS_BORDER           XNDT_IMG_PROC_ERR_START- 4   //图像中待检物越界错误，或太靠近边缘影响检测
#define XNDT_ERR_CONTOUR_MATCH          XNDT_IMG_PROC_ERR_START- 5   //待检物外形与产品学习外形不匹配
#define XNDT_ERR_CONTOUR_FIND           XNDT_IMG_PROC_ERR_START- 6   //学习或检测时，查找产品轮廓错误
#define XNDT_ERR_MARGIN_CUT             XNDT_IMG_PROC_ERR_START- 7   //图像上下边缘裁切参数错误
#define XNDT_ERR_LOAD_MODEL_ERR         XNDT_IMG_PROC_ERR_START- 8   //模型加载错误
#define XNDT_ERR_INFER_AQ_IMG_ERR       XNDT_IMG_PROC_ERR_START- 9   //阿丘推理图像转换失败
#define XNDT_ERR_INFER_EMPTY_INPUT      XNDT_IMG_PROC_ERR_START- 10  //推理失败-输入图像为空
#define XNDT_ERR_INFER_MODEL_ERR        XNDT_IMG_PROC_ERR_START- 11  //推理失败-模型为空
#define XNDT_ERR_INFER_EXE_ERR          XNDT_IMG_PROC_ERR_START- 12  //推理算法执行失败



#endif /* _XAI_ERR_H_ */
