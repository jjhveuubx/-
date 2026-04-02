/******************************************************************************
 *
 * Copyright (C) 2013 XNDT Tech Inc. All rights reserved.
 *
 * FILE NAME   : xai_def.h
 * AUTHOR      : shaofei    < zhousf04@163.com >
 * CREATED     : Sat Nov  2 17:25:01 2013
 * VERSION     : 1.0
 *
 * DESCRIPTION :
 *
 *    xai_def
 *
 * HISTORY     :
 *
 *  2013/11/02 - created
 *
 *******************************************************************************/

#ifndef _XAI_DEF_H_
#define _XAI_DEF_H_

#include <vector>
#include <opencv2/opencv.hpp>
// OpenCV version
#if CV_MAJOR_VERSION >= 4
#include <opencv2/imgproc/imgproc_c.h>
#endif

#include "xai_err.h"

#define XAI_CHECK_TYPE_COUNT    8

#define XAI_CHECK_TYPE_HARD     0
#define XAI_CHECK_TYPE_SOFT     1
#define XAI_CHECK_TYPE_OTHERS   2

// 对应产品的检测算法列表
enum {
    RTTI_XAI_TYPE_NULL,      // 0-无识别算法   :仅作为过料分割使用
    RTTI_XAI_TYPE_100,       // 1-自研推理算法 :针对鸡腿肉的检测，通过加载训练模型，对过检物料进行推理；
    RTTI_XAI_TYPE_AQ,        // 2-阿丘算法     :针对鸡腿肉的检测，通过加载训练模型，对过检物料进行推理。
};

// 对应产品的分割算法列表，预留了100作为示例用于以后扩展，目前只有基础分割
enum {
    RTTI_XAI_SPLIT_NORMAL,            // 0-通用分割算法   :基本分割默认算法；
    RTTI_XAI_SPLIT_100
};


// AQ预留参数
typedef struct {
    char prepare_char_1[256];
    char prepare_char_2[256];

    int prepare_int_1;
    int prepare_int_2;
    int prepare_int_3;
    int prepare_int_4;
} PrepareParams;


typedef struct {
    int seg_gray_min;           // 前景分割灰度值阈值下限
    int seg_gray_max;           // 前景分割灰度值阈值下限

    int fore_seg_score;         // 前景分割置信度
    int dedup_space_thres;      // 前后ROI去重间隔阈值

    int enhance_level;          // 降噪增强程度
    int foreign_seg_score;      // 异物分割阈值
    PrepareParams prep_param;   // 预留参数
} XAI_config_aq;




typedef struct {
    //图像上下边缘参数
    int     warn_tb;        //检测图像上下越界是否报警，自动学习和检测时都需传入参数 1-报警；0-不报警
    int     warn_lr;        //检测图像左右越界是否报警，自动学习和检测时都需传入参数 1-报警；0-不报警

    //基本检测算法选择参数
    int     base_check_sel; //0-基本检测默认算法；
                            //1-增强算法1，倒放大果粒底部检测；
                            //2-增强算法2，药片检测，包括缺片、缺角、碎裂、裂纹等；
                            //3-散料算法1，盒装散料干果类，只需学习最小灰度阈值，以大异物检测方法进行检测；
                            //4-散料算法2，产品无序通过，检查固定长度区域内所有产品中的异物；

    int     show_rect;          //显示物体外接矩形，与XAI定义保持一致，show_roi->show_rect
    int     show_sub_roi;       //显示异物的外接矩形
    int     show_txt;           //显示文本信息
    int     show_contours0;     //显示物体的原始轮廓
    int     show_contours;      //显示物体的拟合轮廓
    int     show_mask;          //显示物体的掩膜/标记
    int     xdas_pw;            //线阵精度，单位um
    int     margin_x1;          //图像上边缘需裁切的距离，自动学习和检测时都需传入参数
    int     margin_x2;          //图像下边缘需裁切的距离，自动学习和检测时都需传入参数
    char    config_dir[256];    //配置文件路径，与线阵相关的那个model
    char    config_file[256];   //配置文件名称，与线阵相关的那个model
    char    model_dir[256];     //模型文件路径，模型文件在该路径下，各算法按照自己的名称规范调用各自的模型
    int     gray_th1;           //图像背景与待检物分割阈值，16位数据，灰度下阈值，0-65535，对应产品信息的灰度阈值
    int     gray_th2;           //灰度上阈值，0-65535，预留


    int     sharpen_size;   //鸡腿肉分割，须设置为0，否则会过分割
    int     min_sz;         // in pix，会通过软件传进来，根据min_sz_mm计算好的像素值
    int     max_sz;         // in pix，会通过软件传进来，根据max_sz_mm计算好的像素值
    int     min_sz_mm;      // in mm，图像界面的最小尺寸（边长）
    int     max_sz_mm;      // in mm，图像界面的最大尺寸（边长）
    int     split_ver;


    int    object_area_min;     // xndt物体最小面积阈值，单位pix，默认5000，界面可设参数
    int    object_threshold;    // xndt物体二值化阈值，单位pix，默认值220，界面可设参数
    int    target_area_min;     // xndt异物最小面积，单位pix，默认100，界面可设参数
    int    target_area_max;     // xndt异物最大面积，单位pix，默认1000，界面可设参数
    int    target_len_min;      // xndt异物最小长度，单位pix，默认1，界面可设参数
    int    target_len_max;      // xndt异物最大长度，单位pix，默认20，界面可设参数
    int    mix_mode;            // xndt mix mode，调试用，0-禁用，1-启用，默认1，配置可设参数
    int    save_infer_src;      // xndt保存推理原图，调试用，0-禁用，1-启用，默认1，配置可设参数
    int    save_infer_chs;      // xndt保存推理XX图，调试用，0-禁用，1-启用，默认1，配置可设参数
    int    save_infer_res;      // xndt保存推理结果图，调试用，0-禁用，1-启用，默认1，配置可设参数
    int    param[8];            // 预留，默认值，所有元素为1


    XAI_config_aq aq_param; //AQ所用的参数结构


} XAI_config_t; //  检测参数的数据结构

typedef struct {
    int id; //物体流水编号
    int type;   //OK/NG类型，0-ok，1-ng
    int sub_type; //子类型，0-ok，1-硬骨，2-软骨，3-其他
    unsigned long long xdas_tm; // X0
    int particle_size; //物体粒度大小
    cv::Rect roi; //物体外接矩形
    cv::Mat mask; //待定
    cv::Mat mask_erode; //待定
    std::vector<cv::Point> contours; //物体轮廓
    std::vector<cv::Point> contours0;
    std::vector<cv::Rect> sub_roi; //异物的外接距
    cv::Point center; //物体中心点坐标
    int ts; //为了使用XCZ的show_roi_lst
    int tt;
    int arc_len; //为了使用XCZ的process_roi_lst
    int is_byd;
} XAI_roi_t;


class XAI_src_t {
public:
    XAI_src_t(): is_se(0) {}

    XAI_src_t operator ()(const cv::Rect &roi) const {
        XAI_src_t src;
        src.is_se = this->is_se;
        src.xhl_h = this->xhl_h(roi);
        src.xhl_l = this->xhl_l(roi);
        src.xhl_c = this->xhl_c(roi);
        src.hsl_h = this->hsl_h(roi);
        src.norm_l = this->norm_l(roi);
        src.norm_h = this->norm_h(roi);
        src.log_l = this->log_l(roi);
        src.diff  = this->diff(roi);
        return src;
    }

public:
    int     is_se;
    int     is_real_ch;    // 0-old raw file, rebuild x(channel id); 1-real x;
    cv::Mat xhl_h; // CV_16UC1
    cv::Mat xhl_l; // CV_16UC1
    cv::Mat xhl_c; // CV_16UC1  x = channel id
    cv::Mat hsl_h; // CV_8UC1
    cv::Mat norm_l; //低能归一化的结果
    cv::Mat norm_h; //高能归一化的结果
    cv::Mat log_l; // 低能-log归一化的结果
    cv::Mat diff;  // diff归一化的结果
};

typedef struct {
    cv::Rect r;
    std::vector<cv::Point> c;
} contour_t;


#endif /* _XAI_DEF_H_ */
