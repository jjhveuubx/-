#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include <opencv2/opencv.hpp>

#include "xai_type_aq.h"

namespace {

void FillGradient16(cv::Mat &mat, int offset) {
    for (int y = 0; y < mat.rows; ++y) {
        for (int x = 0; x < mat.cols; ++x) {
            mat.at<std::uint16_t>(y, x) = static_cast<std::uint16_t>((x * 257 + y * 17 + offset) % 65535);
        }
    }
}

void FillGradient8(cv::Mat &mat, int offset) {
    for (int y = 0; y < mat.rows; ++y) {
        for (int x = 0; x < mat.cols; ++x) {
            mat.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>((x + y + offset) % 255);
        }
    }
}

}  // namespace

int main() {
    const char *env_model_dir = std::getenv("AQ_MODEL_DIR");
    const std::filesystem::path model_dir = env_model_dir != nullptr && env_model_dir[0] != '\0'
                                                ? std::filesystem::path(env_model_dir)
                                                : std::filesystem::path("C:/Users/30847/Desktop/算法对接/try");

    XAI_config_t cfg{};
    std::strncpy(cfg.model_dir, model_dir.string().c_str(), sizeof(cfg.model_dir) - 1);
    std::strncpy(cfg.aq_param.prep_param.prepare_char_1, "输入", sizeof(cfg.aq_param.prep_param.prepare_char_1) - 1);
    cfg.aq_param.foreign_seg_score = 10;
    cfg.aq_param.dedup_space_thres = 15;
    cfg.gray_th1 = 0;
    cfg.gray_th2 = 65535;

    XAITypeAQ instance;
    const int start_rc = instance.start(&cfg);
    std::cout << "start_rc=" << start_rc << std::endl;
    if (start_rc != 0) {
        return 1;
    }

    XAI_src_t src;
    src.xhl_l = cv::Mat(256, 256, CV_16UC1);
    src.xhl_h = cv::Mat(256, 256, CV_16UC1);
    src.log_l = cv::Mat(256, 256, CV_8UC1);
    src.diff = cv::Mat(256, 256, CV_8UC1);
    FillGradient16(src.xhl_l, 0);
    FillGradient16(src.xhl_h, 1024);
    FillGradient8(src.log_l, 0);
    FillGradient8(src.diff, 64);

    XAI_roi_t roi{};
    roi.id = 1;
    roi.xdas_tm = 1;
    roi.roi = cv::Rect(0, 0, 256, 256);

    const int process_rc = instance.do_process_roi(&src, cv::Point(0, 0), roi);
    std::cout << "process_rc=" << process_rc << std::endl;
    std::cout << "roi.type=" << roi.type << std::endl;
    std::cout << "roi.sub_type=" << roi.sub_type << std::endl;
    std::cout << "roi.sub_roi.size=" << roi.sub_roi.size() << std::endl;

    const int stop_rc = instance.stop();
    std::cout << "stop_rc=" << stop_rc << std::endl;
    return 0;
}
