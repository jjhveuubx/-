#pragma once

#include <opencv2/opencv.hpp>

#include "xai_def.h"

#define XAI_CHECK_INIT_P 255

#define XAI_DILATE_3_VAL 3
#define XAI_DILATE_5_VAL 5
#define XAI_DILATE_7_VAL 7
#define XAI_DILATE_9_VAL 9
#define XAI_DILATE_11_VAL 11
#define XAI_MORPH_KSIZE_MIN 9
#define XAI_MORPH_KSIZE_MID 15
#define XAI_MORPH_KSIZE_BIG 21

#define XAI_MATCH_RATIO 0.3
#define XAI_MATCH_GRAY_OFFSET 30
#define XAI_METHOD_ROI_COUNT_MAX 100

int check_base_method(cv::Mat &img, XAI_config_t *config_i, XAI_config_t *config_o, cv::Mat &back_mask, std::vector<XAI_roi_t> &roi_lst);
int check_special_1(cv::Mat &img, XAI_config_t *config_i, XAI_config_t *config_o, cv::Mat &back_mask, std::vector<XAI_roi_t> &roi_lst);
int check_shorter(cv::Mat &img, XAI_config_t *config_i, XAI_config_t *config_o, cv::Mat &back_mask, std::vector<XAI_roi_t> &roi_lst);
int check_big_matter(cv::Mat &img, XAI_config_t *config_i, XAI_config_t *config_o, cv::Mat &back_mask, std::vector<XAI_roi_t> &roi_lst);
int check_mid_matter(cv::Mat &img, XAI_config_t *config_i, XAI_config_t *config_o, cv::Mat &back_mask, std::vector<XAI_roi_t> &roi_lst);
int check_mini_matter(cv::Mat &img, XAI_config_t *config_i, XAI_config_t *config_o, cv::Mat &back_mask, std::vector<XAI_roi_t> &roi_lst);
int check_mini1_matter(cv::Mat &img, XAI_config_t *config_i, XAI_config_t *config_o, cv::Mat &back_mask, std::vector<XAI_roi_t> &roi_lst);
int check_background(cv::Mat &img, XAI_config_t *config_i, XAI_config_t *config_o, cv::Mat &back_mask, std::vector<XAI_roi_t> &roi_lst);

int getAttribute(cv::Mat &img, XAI_config_t *config, cv::Mat &back_mask, cv::Rect &bounding_o);
int getAttributeLearn(cv::Mat &img, XAI_config_t *config);
int getAttributeCheck_100(
    cv::Mat &img,
    XAI_config_t *config_i,
    XAI_config_t *config_o,
    cv::Mat &area_mask,
    cv::Mat &back_mask,
    cv::Rect &bounding_o);
int getAttributeCheck_300(
    cv::Mat &img,
    XAI_config_t *config_i,
    XAI_config_t *config_o,
    cv::Mat &area_mask,
    cv::Mat &back_mask,
    cv::Rect &bounding_o);
int getBestConfig(std::vector<XAI_config_t> cfg_lst, XAI_config_t *config);
int getBestConfig_300(std::vector<XAI_config_t> cfg_lst, XAI_config_t *config);
int isSubRect(XAI_config_t *config_o, cv::Rect rec, float diff = XAI_MATCH_RATIO);
void gammaAdjust(cv::Mat &src, cv::Mat &dst, float gamma = 0.4f);
int isBlackRoi(cv::Mat &img, std::vector<std::vector<cv::Point>> &contours, int index, int offset = 4);
int getAreaMask(cv::Mat &img, XAI_config_t *config_i, XAI_config_t *config_o, cv::Mat &area_mask, cv::Rect &bounding_o);
void calculateRectangleDimensions(const cv::Mat &img, const cv::Point &pt1, const cv::Point &pt2, int &width, int &topHeight, int &bottomHeight);
int calculate_liquid_leval(const cv::Mat &img);
std::vector<std::pair<int, int>> find_midLinePoints(const cv::Mat &img, int end_line);
void medianFilter(std::vector<int> &vec, int windowSize);
std::pair<int, int> detectTrends(std::vector<int> &ts, int windowSize, int minTrendLength);
void replace_with_average(std::vector<int> &ts);
std::vector<int> getSegment(const std::vector<int> &vec, const std::pair<int, int> &segment_l);
std::vector<std::pair<int, int>> mergeIntervals(std::vector<std::pair<int, int>> &intervals);
