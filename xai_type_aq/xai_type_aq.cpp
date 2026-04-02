#include "xai_type_aq.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdarg>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <windows.h>

#include <cppabi/visionflow.hpp>
#include "visionflow/props/multi_names_polygon_region_list.hpp"
#include "visionflow/props/polygon_region_list.hpp"

namespace vflow = cpp2x::visionflow;

#if defined(UNICODE) && defined(_UNICODE) || defined(_MBCS)
#define U8(x) vflow::helper::locale_to_utf8({x})
#define U8L(x) vflow::helper::utf8_to_locale({x})
#else
#define U8(x) x
#define U8L(x) x
#endif

#if defined(XAI_TYPE_AQ_BUILD_SHARED)
#if defined(_WIN32)
#define XAI_TYPE_AQ_EXPORT extern "C" __declspec(dllexport)
#else
#define XAI_TYPE_AQ_EXPORT extern "C" __attribute__((visibility("default")))
#endif
#else
#define XAI_TYPE_AQ_EXPORT
#endif

namespace {

std::wstring ToLowerWideCopy(std::wstring value);
std::filesystem::path GetCurrentModuleDir();

struct PairHash {
    template <typename T1, typename T2>
    std::size_t operator()(const std::pair<T1, T2> &value) const {
        const std::size_t h1 = std::hash<T1>{}(value.first);
        const std::size_t h2 = std::hash<T2>{}(value.second);
        return h1 ^ (h2 << 1);
    }
};

struct PairEqual {
    template <typename T1, typename T2>
    bool operator()(const std::pair<T1, T2> &lhs, const std::pair<T1, T2> &rhs) const {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }
};

const char *MatDepthToString(int depth) {
    switch (depth) {
    case CV_8U:
        return "8U";
    case CV_8S:
        return "8S";
    case CV_16U:
        return "16U";
    case CV_16S:
        return "16S";
    case CV_32S:
        return "32S";
    case CV_32F:
        return "32F";
    case CV_64F:
        return "64F";
    default:
        return "未知";
    }
}

std::string MatSummary(const cv::Mat &mat) {
    if (mat.empty()) {
        return "空";
    }

    char buffer[128] = {0};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "宽=%d,高=%d,通道=%d,位深=%s",
        mat.cols,
        mat.rows,
        mat.channels(),
        MatDepthToString(mat.depth()));
    return std::string(buffer);
}

std::string RectToString(const cv::Rect &rect) {
    char buffer[96] = {0};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "[左=%d,上=%d,宽=%d,高=%d]",
        rect.x,
        rect.y,
        rect.width,
        rect.height);
    return std::string(buffer);
}

std::string PointToString(const cv::Point &point) {
    char buffer[64] = {0};
    std::snprintf(buffer, sizeof(buffer), "[横=%d,纵=%d]", point.x, point.y);
    return std::string(buffer);
}

std::string PathToLogString(const std::filesystem::path &path) {
    if (path.empty()) {
        return std::string();
    }

    std::error_code ec;
    const std::filesystem::path normalized = std::filesystem::weakly_canonical(path, ec);
    if (!ec && !normalized.empty()) {
        return normalized.u8string();
    }
    return path.u8string();
}

std::filesystem::path GetPackageRootDir() {
    const std::filesystem::path module_dir = GetCurrentModuleDir();
    const std::wstring dir_name = ToLowerWideCopy(module_dir.filename().wstring());
    if (dir_name == L"bin" || dir_name == L"tools" || dir_name == L"lib" || dir_name == L"dlls") {
        return module_dir.parent_path();
    }
    return module_dir;
}

std::string InputPathToLogString(const std::string &path_text) {
    if (path_text.empty()) {
        return std::string();
    }

    std::error_code ec;
    const std::filesystem::path locale_path(path_text);
    if (std::filesystem::exists(locale_path, ec)) {
        return PathToLogString(locale_path);
    }

    ec.clear();
    const std::filesystem::path utf8_path = std::filesystem::u8path(path_text);
    if (std::filesystem::exists(utf8_path, ec)) {
        return PathToLogString(utf8_path);
    }

    return path_text;
}

std::string BuildTimestampFolderName() {
    const auto now = std::chrono::system_clock::now();
    const auto now_time = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm local_tm{};
    localtime_s(&local_tm, &now_time);

    char buffer[64] = {0};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%04d%02d%02d_%02d%02d%02d_%03lld",
        local_tm.tm_year + 1900,
        local_tm.tm_mon + 1,
        local_tm.tm_mday,
        local_tm.tm_hour,
        local_tm.tm_min,
        local_tm.tm_sec,
        static_cast<long long>(ms.count()));
    return std::string(buffer);
}

std::filesystem::path MakeUniqueDebugDirectory() {
    const std::filesystem::path debug_root = GetPackageRootDir() / std::filesystem::u8path("调试图");
    std::error_code ec;
    std::filesystem::create_directories(debug_root, ec);

    const std::string base_name = BuildTimestampFolderName();
    std::filesystem::path target_dir = debug_root / std::filesystem::u8path(base_name);
    int suffix = 1;
    while (std::filesystem::exists(target_dir, ec)) {
        target_dir = debug_root / std::filesystem::u8path(base_name + "_" + std::to_string(suffix));
        ++suffix;
    }

    std::filesystem::create_directories(target_dir, ec);
    if (ec) {
        return std::filesystem::path();
    }
    return target_dir;
}

void AqLogV(const char *level, const char *stage, const char *fmt, va_list args) {
    const auto LocalizeLevel = [](const char *value) -> const char * {
        if (value == nullptr) {
            return "未知";
        }
        if (std::strcmp(value, "INFO") == 0) {
            return "信息";
        }
        if (std::strcmp(value, "WARN") == 0) {
            return "警告";
        }
        if (std::strcmp(value, "ERROR") == 0) {
            return "错误";
        }
        return value;
    };

    const auto LocalizeStage = [](const char *value) -> const char * {
        if (value == nullptr) {
            return "未知";
        }
        if (std::strcmp(value, "factory") == 0) {
            return "工厂";
        }
        if (std::strcmp(value, "lifecycle") == 0) {
            return "生命周期";
        }
        if (std::strcmp(value, "start") == 0) {
            return "启动";
        }
        if (std::strcmp(value, "stop") == 0) {
            return "停止";
        }
        if (std::strcmp(value, "infer") == 0) {
            return "推理";
        }
        if (std::strcmp(value, "preprocess") == 0) {
            return "预处理";
        }
        if (std::strcmp(value, "model") == 0) {
            return "模型";
        }
        if (std::strcmp(value, "vf") == 0) {
            return "视觉流";
        }
        if (std::strcmp(value, "debug") == 0) {
            return "调试";
        }
        return value;
    };

    if (fmt == nullptr) {
        return;
    }

    std::vector<char> buffer(512, '\0');
    va_list args_copy;
    va_copy(args_copy, args);
    int written = std::vsnprintf(buffer.data(), buffer.size(), fmt, args_copy);
    va_end(args_copy);

    if (written < 0) {
        m_msg_prt("[AQ][%s][%s] 日志格式化失败。", LocalizeLevel(level), LocalizeStage(stage));
        return;
    }

    if (static_cast<std::size_t>(written) >= buffer.size()) {
        buffer.resize(static_cast<std::size_t>(written) + 1U);
        va_copy(args_copy, args);
        written = std::vsnprintf(buffer.data(), buffer.size(), fmt, args_copy);
        va_end(args_copy);
        if (written < 0) {
            m_msg_prt("[AQ][%s][%s] 日志格式化失败。", LocalizeLevel(level), LocalizeStage(stage));
            return;
        }
    }

    m_msg_prt("[AQ][%s][%s] %s", LocalizeLevel(level), LocalizeStage(stage), buffer.data());
}

void AqLogInfo(const char *stage, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AqLogV("INFO", stage, fmt, args);
    va_end(args);
}

void AqLogWarn(const char *stage, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AqLogV("WARN", stage, fmt, args);
    va_end(args);
}

void AqLogError(const char *stage, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AqLogV("ERROR", stage, fmt, args);
    va_end(args);
}

int StartSehFilter(unsigned int exception_code, const void *self, const void *impl, const void *cfg) {
    AqLogError(
        "start",
        "启动发生结构化异常：异常码=0x%08lX，this=%p，impl_=%p，cfg=%p。",
        static_cast<unsigned long>(exception_code),
        self,
        impl,
        cfg);
    return EXCEPTION_EXECUTE_HANDLER;
}

int InferSehFilter(unsigned int exception_code, const void *self, const void *impl, const void *src_i, const void *roi) {
    AqLogError(
        "infer",
        "区域处理发生结构化异常：异常码=0x%08lX，this=%p，impl_=%p，src_i=%p，roi=%p。",
        static_cast<unsigned long>(exception_code),
        self,
        impl,
        src_i,
        roi);
    return EXCEPTION_EXECUTE_HANDLER;
}

struct DetectionCandidate {
    std::string label;
    float score = 0.0F;
    int subtype = 0;
    cv::Rect local_box;
    std::vector<cv::Point> local_contour;
};

struct HistoryEntry {
    unsigned long long xdas_tm = 0;
    std::vector<DetectionCandidate> candidates;
};

std::string TrimCString(const char *value) {
    if (value == nullptr) {
        return std::string();
    }
    std::size_t len = 0;
    while (len < 255 && value[len] != '\0') {
        ++len;
    }
    return std::string(value, len);
}

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::wstring ToLowerWideCopy(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::string WideToUtf8String(const std::wstring &value) {
    if (value.empty()) {
        return std::string();
    }

    const int size_needed = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size_needed <= 0) {
        return std::string();
    }

    std::string result(static_cast<std::size_t>(size_needed), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        size_needed,
        nullptr,
        nullptr);
    return result;
}

float NormalizeScoreThreshold(int raw_value) {
    if (raw_value <= 0) {
        return 0.5F;
    }
    if (raw_value > 100) {
        return 0.5F;
    }
    if (raw_value > 1) {
        return static_cast<float>(raw_value) / 100.0F;
    }
    return static_cast<float>(raw_value);
}

cv::Rect ClampRectToImage(const cv::Rect &roi, const cv::Size &size) {
    const cv::Rect image_rect(0, 0, size.width, size.height);
    return roi & image_rect;
}

std::vector<cv::Point> BuildRectContour(const cv::Rect &roi) {
    return {
        roi.tl(),
        cv::Point(roi.x + roi.width, roi.y),
        roi.br(),
        cv::Point(roi.x, roi.y + roi.height),
    };
}

cv::Mat ToGray8U(const cv::Mat &src) {
    if (src.empty()) {
        return cv::Mat();
    }

    cv::Mat gray;
    if (src.channels() == 1) {
        gray = src;
    } else if (src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else if (src.channels() == 4) {
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    } else {
        gray = src.reshape(1);
    }

    cv::Mat out;
    if (gray.depth() == CV_8U) {
        out = gray.clone();
    } else if (gray.depth() == CV_16U) {
        gray.convertTo(out, CV_8U, 1.0 / 256.0);
    } else {
        cv::normalize(gray, out, 0, 255, cv::NORM_MINMAX);
        out.convertTo(out, CV_8U);
    }
    return out;
}

cv::Mat WindowTo8U(const cv::Mat &src, int low, int high) {
    if (src.empty()) {
        return cv::Mat();
    }

    cv::Mat src8 = ToGray8U(src);
    if (src.depth() == CV_16U) {
        const int safe_low = std::max(0, std::min(65535, low));
        const int safe_high = std::max(safe_low + 1, std::min(65535, high));
        const double scaled_low = static_cast<double>(safe_low) / 256.0;
        const double scaled_high = static_cast<double>(safe_high) / 256.0;

        cv::Mat clipped;
        src8.convertTo(clipped, CV_32F);
        cv::threshold(clipped, clipped, scaled_high, scaled_high, cv::THRESH_TRUNC);
        cv::threshold(clipped, clipped, scaled_low, 0, cv::THRESH_TOZERO);
        clipped -= static_cast<float>(scaled_low);
        clipped *= static_cast<float>(255.0 / std::max(1.0, scaled_high - scaled_low));
        clipped.convertTo(src8, CV_8U);
    }
    return src8;
}

cv::Mat ApplyEnhancement(const cv::Mat &src, int level) {
    if (src.empty()) {
        return cv::Mat();
    }

    const int safe_level = std::max(0, std::min(level, 10));
    if (safe_level == 0) {
        return src.clone();
    }

    cv::Mat blurred;
    const int blur_size = 3 + (safe_level > 4 ? 2 : 0);
    cv::GaussianBlur(src, blurred, cv::Size(blur_size, blur_size), 0.0);

    cv::Mat sharpened;
    const double alpha = 1.0 + safe_level * 0.12;
    const double beta = -safe_level * 0.12;
    cv::addWeighted(src, alpha, blurred, beta, 0.0, sharpened);

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(2.0 + safe_level * 0.4);
    clahe->setTilesGridSize(cv::Size(8, 8));

    cv::Mat enhanced;
    clahe->apply(sharpened, enhanced);
    return enhanced;
}

cv::Mat ToBgrImage(const cv::Mat &src) {
    if (src.empty()) {
        return cv::Mat();
    }

    cv::Mat out;
    if (src.channels() == 1) {
        cv::cvtColor(src, out, cv::COLOR_GRAY2BGR);
        return out;
    }
    if (src.channels() == 3) {
        return src.clone();
    }
    if (src.channels() == 4) {
        cv::cvtColor(src, out, cv::COLOR_BGRA2BGR);
        return out;
    }

    return ToBgrImage(ToGray8U(src));
}

cv::Mat BuildOriginalDebugImage(const XAI_src_t &src, const cv::Rect &local_roi, const XAI_config_t &cfg) {
    const auto Crop = [&](const cv::Mat &mat) -> cv::Mat {
        if (mat.empty()) {
            return cv::Mat();
        }
        const cv::Rect safe_roi = ClampRectToImage(local_roi, mat.size());
        if (safe_roi.empty()) {
            return cv::Mat();
        }
        return mat(safe_roi);
    };

    const int gray_low = cfg.gray_th1 > 0 ? cfg.gray_th1 : 0;
    const int gray_high = cfg.gray_th2 > gray_low ? cfg.gray_th2 : 65535;

    cv::Mat base;
    if (!src.xhl_l.empty()) {
        base = WindowTo8U(Crop(src.xhl_l), gray_low, gray_high);
    } else if (!src.xhl_h.empty()) {
        base = WindowTo8U(Crop(src.xhl_h), gray_low, gray_high);
    } else if (!src.norm_l.empty()) {
        base = ToGray8U(Crop(src.norm_l));
    } else if (!src.norm_h.empty()) {
        base = ToGray8U(Crop(src.norm_h));
    } else if (!src.log_l.empty()) {
        base = ToGray8U(Crop(src.log_l));
    } else if (!src.diff.empty()) {
        base = ToGray8U(Crop(src.diff));
    } else if (!src.hsl_h.empty()) {
        base = ToGray8U(Crop(src.hsl_h));
    } else if (!src.xhl_c.empty()) {
        base = ToGray8U(Crop(src.xhl_c));
    }

    return ToBgrImage(base);
}

cv::Rect TranslateRectToLocal(const cv::Rect &global_rect, const cv::Point &global_origin, const cv::Size &size) {
    const cv::Rect local_rect(global_rect.tl() - global_origin, global_rect.size());
    return ClampRectToImage(local_rect, size);
}

void DrawMaskOverlay(cv::Mat &image, const cv::Mat &mask, const cv::Rect &draw_rect) {
    if (image.empty() || mask.empty() || draw_rect.empty()) {
        return;
    }

    cv::Mat mask8 = ToGray8U(mask);
    if (mask8.empty()) {
        return;
    }
    if (mask8.size() != draw_rect.size()) {
        cv::resize(mask8, mask8, draw_rect.size(), 0.0, 0.0, cv::INTER_NEAREST);
    }

    cv::threshold(mask8, mask8, 0, 255, cv::THRESH_BINARY);
    cv::Mat roi_img = image(draw_rect);
    cv::Mat overlay = roi_img.clone();
    overlay.setTo(cv::Scalar(0, 0, 255), mask8);
    cv::addWeighted(overlay, 0.35, roi_img, 0.65, 0.0, roi_img);
}

bool WriteBytesToFile(const std::filesystem::path &path, const std::vector<unsigned char> &buffer) {
    FILE *file = _wfopen(path.c_str(), L"wb");
    if (file == nullptr) {
        return false;
    }

    const std::size_t written = std::fwrite(buffer.data(), 1, buffer.size(), file);
    std::fflush(file);
    std::fclose(file);
    return written == buffer.size();
}

bool WriteImageFile(const std::filesystem::path &path, const cv::Mat &image) {
    if (image.empty()) {
        return false;
    }

    std::vector<unsigned char> encoded;
    if (!cv::imencode(".png", image, encoded)) {
        return false;
    }
    return WriteBytesToFile(path, encoded);
}

void SaveDebugImages(
    const XAI_src_t &src,
    const cv::Rect &local_roi,
    const XAI_config_t &cfg,
    const cv::Mat &infer_input,
    const XAI_roi_t &roi,
    const cv::Point &global_origin) {
    const std::filesystem::path output_dir = MakeUniqueDebugDirectory();
    if (output_dir.empty()) {
        AqLogWarn("debug", "创建调试图目录失败。");
        return;
    }

    cv::Mat original_image = BuildOriginalDebugImage(src, local_roi, cfg);
    if (original_image.empty()) {
        original_image = ToBgrImage(infer_input);
    }
    if (original_image.empty()) {
        AqLogWarn("debug", "生成原图失败，跳过存图。");
        return;
    }

    cv::Mat result_image = original_image.clone();
    const cv::Rect main_roi = TranslateRectToLocal(roi.roi, global_origin, result_image.size());
    if (!main_roi.empty()) {
        cv::rectangle(result_image, main_roi, cv::Scalar(0, 255, 255), 2);
    }
    DrawMaskOverlay(result_image, roi.mask, main_roi);

    if (!roi.contours.empty()) {
        std::vector<cv::Point> local_contour;
        local_contour.reserve(roi.contours.size());
        for (const cv::Point &point : roi.contours) {
            local_contour.push_back(point - global_origin);
        }
        const std::vector<std::vector<cv::Point>> contours = {local_contour};
        cv::polylines(result_image, contours, true, cv::Scalar(0, 255, 0), 2);
    }

    for (const cv::Rect &sub_roi : roi.sub_roi) {
        const cv::Rect local_sub_roi = TranslateRectToLocal(sub_roi, global_origin, result_image.size());
        if (!local_sub_roi.empty()) {
            cv::rectangle(result_image, local_sub_roi, cv::Scalar(0, 0, 255), 2);
        }
    }

    const cv::Point local_center = roi.center - global_origin;
    if (local_center.x >= 0 && local_center.x < result_image.cols &&
        local_center.y >= 0 && local_center.y < result_image.rows) {
        cv::circle(result_image, local_center, 4, cv::Scalar(255, 255, 0), cv::FILLED);
    }

    const std::filesystem::path original_path = output_dir / std::filesystem::u8path("原图.png");
    const std::filesystem::path result_path = output_dir / std::filesystem::u8path("推理后.png");

    const bool original_ok = WriteImageFile(original_path, original_image);
    const bool result_ok = WriteImageFile(result_path, result_image);

    if (original_ok && result_ok) {
        AqLogInfo(
            "debug",
            "调试图已保存：目录=%s，原图=%s，结果图=%s",
            PathToLogString(output_dir).c_str(),
            PathToLogString(original_path).c_str(),
            PathToLogString(result_path).c_str());
        return;
    }

    AqLogWarn(
        "debug",
        "调试图保存不完整：目录=%s，原图成功=%d，结果图成功=%d",
        PathToLogString(output_dir).c_str(),
        original_ok ? 1 : 0,
        result_ok ? 1 : 0);
}

cv::Mat BuildInferImage(const XAI_src_t &src, const cv::Rect &local_roi, const XAI_config_t &cfg) {
    AqLogInfo(
        "preprocess",
        "开始构建推理输入图：区域=%s，是否SE=%d，是否真实通道=%d，xhl_l=%s，xhl_h=%s，xhl_c=%s，hsl_h=%s，log_l=%s，diff=%s",
        RectToString(local_roi).c_str(),
        src.is_se,
        src.is_real_ch,
        MatSummary(src.xhl_l).c_str(),
        MatSummary(src.xhl_h).c_str(),
        MatSummary(src.xhl_c).c_str(),
        MatSummary(src.hsl_h).c_str(),
        MatSummary(src.log_l).c_str(),
        MatSummary(src.diff).c_str());
    AqLogInfo(
        "preprocess",
        "normalized image summary: norm_l=%s, norm_h=%s",
        MatSummary(src.norm_l).c_str(),
        MatSummary(src.norm_h).c_str());

    const auto Crop = [&](const cv::Mat &mat) -> cv::Mat {
        if (mat.empty()) {
            return cv::Mat();
        }
        const cv::Rect safe_roi = ClampRectToImage(local_roi, mat.size());
        if (safe_roi.empty()) {
            return cv::Mat();
        }
        return mat(safe_roi);
    };

    const cv::Mat low_raw = Crop(src.xhl_l);
    const cv::Mat high_raw = Crop(src.xhl_h);
    const cv::Mat norm_low = Crop(src.norm_l);
    const cv::Mat norm_high = Crop(src.norm_h);
    const cv::Mat log_img = Crop(src.log_l);
    const cv::Mat diff_img = Crop(src.diff);

    const int gray_low = cfg.gray_th1 > 0 ? cfg.gray_th1 : 0;
    const int gray_high = cfg.gray_th2 > gray_low ? cfg.gray_th2 : 65535;

    cv::Mat ch0;
    if (!low_raw.empty()) {
        ch0 = WindowTo8U(low_raw, gray_low, gray_high);
    } else if (!norm_low.empty()) {
        ch0 = ToGray8U(norm_low);
    } else if (!high_raw.empty()) {
        ch0 = ToGray8U(high_raw);
    } else {
        ch0 = ToGray8U(norm_high);
    }
    cv::Mat ch1;
    cv::Mat ch2;

    if (!log_img.empty()) {
        ch1 = ToGray8U(log_img);
    } else if (!high_raw.empty()) {
        const int alt_low = cfg.aq_param.prep_param.prepare_int_1 > 0
                                ? cfg.aq_param.prep_param.prepare_int_1
                                : gray_low;
        const int alt_high = cfg.aq_param.prep_param.prepare_int_2 > alt_low
                                 ? cfg.aq_param.prep_param.prepare_int_2
                                 : gray_high;
        ch1 = WindowTo8U(high_raw, alt_low, alt_high);
    } else if (!norm_high.empty()) {
        ch1 = ToGray8U(norm_high);
    } else {
        ch1 = ch0.clone();
    }

    if (!diff_img.empty()) {
        ch2 = ToGray8U(diff_img);
    } else if (!norm_high.empty()) {
        ch2 = ToGray8U(norm_high);
    } else if (!high_raw.empty()) {
        ch2 = ToGray8U(high_raw);
    } else {
        ch2 = ch0.clone();
    }

    const int enhance_level = std::max(0, cfg.aq_param.enhance_level);
    ch0 = ApplyEnhancement(ch0, enhance_level);
    ch1 = ApplyEnhancement(ch1, std::max(0, enhance_level - 1));
    ch2 = ApplyEnhancement(ch2, std::max(0, enhance_level - 2));

    if (ch0.empty() || ch1.empty() || ch2.empty()) {
        AqLogError(
            "preprocess",
            "构建推理输入图失败：ch0=%s，ch1=%s，ch2=%s",
            MatSummary(ch0).c_str(),
            MatSummary(ch1).c_str(),
            MatSummary(ch2).c_str());
        return cv::Mat();
    }

    std::vector<cv::Mat> channels = {ch0, ch1, ch2};
    cv::Mat merged;
    cv::merge(channels, merged);
    AqLogInfo(
        "preprocess",
        "构建推理输入图成功：合成图=%s，灰度下限=%d，灰度上限=%d，增强等级=%d",
        MatSummary(merged).c_str(),
        gray_low,
        gray_high,
        enhance_level);
    return merged;
}

vflow::img::Image MatToVFImage(const cv::Mat &mat) {
    if (mat.empty()) {
        return vflow::img::Image();
    }

    if (!mat.isContinuous()) {
        return MatToVFImage(mat.clone());
    }

    const unsigned long long step = static_cast<unsigned long long>(mat.step);

    switch (mat.type()) {
    case CV_8UC1:
        return vflow::img::Image(const_cast<unsigned char *>(mat.ptr<unsigned char>()),
                                 static_cast<unsigned int>(mat.rows),
                                 static_cast<unsigned int>(mat.cols),
                                 1,
                                 vflow::img::Image::kDepthU8,
                                 step);
    case CV_8UC3:
        return vflow::img::Image(const_cast<unsigned char *>(mat.ptr<unsigned char>()),
                                 static_cast<unsigned int>(mat.rows),
                                 static_cast<unsigned int>(mat.cols),
                                 3,
                                 vflow::img::Image::kDepthU8,
                                 step);
    case CV_8UC4:
        return vflow::img::data_to_image(const_cast<unsigned char *>(mat.ptr<unsigned char>()),
                                         static_cast<unsigned int>(mat.rows),
                                         static_cast<unsigned int>(mat.cols),
                                         vflow::img::kColorBGRA,
                                         vflow::img::kBGR,
                                         0,
                                         vflow::img::Image::kDepthU8,
                                         true);
    case CV_16UC1:
        return vflow::img::Image(const_cast<unsigned short *>(mat.ptr<unsigned short>()),
                                 static_cast<unsigned int>(mat.rows),
                                 static_cast<unsigned int>(mat.cols),
                                 1,
                                 vflow::img::Image::kDepthU16,
                                 step);
    default:
        return MatToVFImage(ToGray8U(mat));
    }
}

cpp2x::String GetInferParamNode(const std::string &module_type) {
    if (module_type == "Segmentation") {
        return vflow::Segmentation::get_pred();
    }
    if (module_type == "Detection") {
        return vflow::Detection::get_pred();
    }
    if (module_type == "Classification") {
        return vflow::Classification::get_pred();
    }
    if (module_type == "UnsuperClassification") {
        return vflow::UnsuperClassification::get_pred();
    }
    if (module_type == "RegionCalculation") {
        return vflow::RegionCalculation::get_pred();
    }
    return cpp2x::String();
}

std::string LocalizeModuleType(const std::string &module_type) {
    if (module_type == "Input") {
        return "输入";
    }
    if (module_type == "ViewTransformer") {
        return "视图变换";
    }
    if (module_type == "Segmentation") {
        return "分割";
    }
    if (module_type == "Detection") {
        return "检测";
    }
    if (module_type == "Classification") {
        return "分类";
    }
    if (module_type == "UnsuperClassification") {
        return "无监督分类";
    }
    if (module_type == "RegionCalculation") {
        return "区域计算";
    }
    return module_type;
}

int InferSubtypeFromLabel(const std::string &label) {
    const std::string lower = ToLowerCopy(label);
    if (lower.find("hard") != std::string::npos) {
        return 1;
    }
    if (lower.find("soft") != std::string::npos) {
        return 2;
    }
    if (lower.find("ok") != std::string::npos ||
        lower.find("background") != std::string::npos ||
        lower.find("leg") != std::string::npos ||
        lower.find("chicken") != std::string::npos) {
        return 0;
    }
    return 3;
}

bool IsBodyLabel(const std::string &label) {
    const std::string lower = ToLowerCopy(label);
    return lower.find("leg") != std::string::npos || lower.find("chicken") != std::string::npos;
}

std::vector<cv::Point> RingToContour(const vflow::geometry::Ring2f &ring) {
    std::vector<cv::Point> contour;
    contour.reserve(static_cast<std::size_t>(ring.size()));
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const auto point = ring.get(i);
        contour.emplace_back(
            static_cast<int>(std::lround(point.get_x())),
            static_cast<int>(std::lround(point.get_y())));
    }
    return contour;
}

std::vector<DetectionCandidate> ParsePolygonRegionList(
    const vflow::props::PolygonRegionList &regions,
    float score_threshold) {
    std::vector<DetectionCandidate> candidates;
    const auto keys = regions.keys();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto key = keys.get(i);
        const auto region = regions.at(key);
        if (region.score() < score_threshold) {
            continue;
        }

        DetectionCandidate candidate;
        candidate.label = U8L(region.name()).c_str();
        candidate.score = region.score();
        candidate.subtype = InferSubtypeFromLabel(candidate.label);
        candidate.local_contour = RingToContour(region.polygon().get_outer());
        if (candidate.local_contour.size() >= 3) {
            candidate.local_box = cv::boundingRect(candidate.local_contour);
        }
        candidates.push_back(candidate);
    }
    return candidates;
}

std::vector<DetectionCandidate> ParseMultiNameRegionList(
    const vflow::props::MultiNamesPolygonRegionList &regions,
    float score_threshold) {
    std::vector<DetectionCandidate> candidates;
    const auto keys = regions.keys();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto key = keys.get(i);
        const auto region = regions.at(key);
        if (region.score() < score_threshold) {
            continue;
        }

        DetectionCandidate candidate;
        candidate.label = U8L(region.name()).c_str();
        candidate.score = region.score();
        candidate.subtype = InferSubtypeFromLabel(candidate.label);
        candidate.local_contour = RingToContour(region.polygon().get_outer());
        if (candidate.local_contour.size() >= 3) {
            candidate.local_box = cv::boundingRect(candidate.local_contour);
        }
        candidates.push_back(candidate);
    }
    return candidates;
}

float IoU(const cv::Rect &lhs, const cv::Rect &rhs) {
    const cv::Rect inter = lhs & rhs;
    if (inter.empty()) {
        return 0.0F;
    }

    const float inter_area = static_cast<float>(inter.area());
    const float union_area =
        static_cast<float>(lhs.area() + rhs.area() - inter.area());
    if (union_area <= 0.0F) {
        return 0.0F;
    }
    return inter_area / union_area;
}

void DeduplicateCandidates(std::vector<DetectionCandidate> &candidates, float iou_threshold) {
    std::sort(candidates.begin(), candidates.end(), [](const DetectionCandidate &lhs, const DetectionCandidate &rhs) {
        return lhs.score > rhs.score;
    });

    std::vector<DetectionCandidate> deduped;
    for (const DetectionCandidate &candidate : candidates) {
        bool keep = true;
        for (const DetectionCandidate &existing : deduped) {
            if (candidate.subtype == existing.subtype &&
                IoU(candidate.local_box, existing.local_box) >= iou_threshold) {
                keep = false;
                break;
            }
        }
        if (keep) {
            deduped.push_back(candidate);
        }
    }
    candidates.swap(deduped);
}

cv::Mat BuildLocalMask(const std::vector<cv::Point> &global_contour, const cv::Rect &global_roi) {
    if (global_roi.width <= 0 || global_roi.height <= 0) {
        return cv::Mat();
    }

    cv::Mat mask = cv::Mat::zeros(global_roi.height, global_roi.width, CV_8UC1);
    if (global_contour.size() < 3) {
        cv::rectangle(mask, cv::Rect(0, 0, global_roi.width, global_roi.height), cv::Scalar(255), cv::FILLED);
        return mask;
    }

    std::vector<cv::Point> local_contour;
    local_contour.reserve(global_contour.size());
    for (const cv::Point &point : global_contour) {
        local_contour.push_back(point - global_roi.tl());
    }

    const std::vector<std::vector<cv::Point>> contours = {local_contour};
    cv::fillPoly(mask, contours, cv::Scalar(255));
    return mask;
}

std::filesystem::path FindModelFileInPath(const std::filesystem::path &path) {
    if (path.empty()) {
        AqLogWarn("model", "跳过空模型路径。");
        return std::filesystem::path();
    }

    std::error_code ec;
    if (std::filesystem::is_regular_file(path, ec)) {
        AqLogInfo("model", "模型路径是文件：%s", PathToLogString(path).c_str());
        return path;
    }

    if (!std::filesystem::is_directory(path, ec)) {
        AqLogWarn("model", "模型路径不是有效的文件或目录：%s", PathToLogString(path).c_str());
        return std::filesystem::path();
    }

    AqLogInfo("model", "开始扫描目录中的 .vfmodel：%s", PathToLogString(path).c_str());
    for (const auto &entry : std::filesystem::recursive_directory_iterator(path, ec)) {
        if (ec) {
            AqLogWarn("model", "目录扫描被中断：%s", PathToLogString(path).c_str());
            break;
        }
        if (entry.is_regular_file() && entry.path().extension() == ".vfmodel") {
            AqLogInfo("model", "找到模型文件：%s", PathToLogString(entry.path()).c_str());
            return entry.path();
        }
    }
    AqLogWarn("model", "在该目录下未找到 .vfmodel：%s", PathToLogString(path).c_str());
    return std::filesystem::path();
}

std::filesystem::path ResolveModelFile(const std::string &model_path) {
    if (model_path.empty()) {
        AqLogWarn("model", "模型路径解析时收到空路径。");
        return std::filesystem::path();
    }

    AqLogInfo("model", "开始解析模型路径：%s", InputPathToLogString(model_path).c_str());
    const std::filesystem::path locale_path(model_path);
    const std::filesystem::path locale_result = FindModelFileInPath(locale_path);
    if (!locale_result.empty()) {
        AqLogInfo("model", "按本地路径解析到模型：%s", PathToLogString(locale_result).c_str());
        return locale_result;
    }

    const std::filesystem::path utf8_path = std::filesystem::u8path(model_path);
    const std::filesystem::path utf8_result = FindModelFileInPath(utf8_path);
    if (!utf8_result.empty()) {
        AqLogInfo("model", "按 UTF-8 编码路径解析到模型：%s", PathToLogString(utf8_result).c_str());
    }
    return utf8_result;
}

std::wstring GetEnvValue(const wchar_t *name) {
    wchar_t *raw_value = nullptr;
    std::size_t value_len = 0;
    if (_wdupenv_s(&raw_value, &value_len, name) != 0 || raw_value == nullptr || value_len == 0) {
        return std::wstring();
    }
    std::unique_ptr<wchar_t, decltype(&std::free)> holder(raw_value, &std::free);
    return std::wstring(holder.get());
}

std::filesystem::path GetModuleFilePath(HMODULE module) {
    if (module == nullptr) {
        return std::filesystem::path();
    }

    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    buffer.resize(length);
    return std::filesystem::path(buffer);
}

std::filesystem::path GetExecutableDir() {
    return GetModuleFilePath(GetModuleHandleW(nullptr)).parent_path();
}

std::filesystem::path GetCurrentModuleDir() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetCurrentModuleDir),
            &module) ||
        module == nullptr) {
        return std::filesystem::path();
    }
    return GetModuleFilePath(module).parent_path();
}

void AddCandidateDir(std::vector<std::filesystem::path> &dirs, const std::filesystem::path &path) {
    if (path.empty()) {
        return;
    }

    std::error_code ec;
    if (std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec)) {
        dirs.push_back(path);
    }
}

bool ContainsVisionFlowDll(const std::filesystem::path &dir) {
    if (dir.empty()) {
        return false;
    }

    std::error_code ec;
    const std::filesystem::path dll_path = dir / "visionflow.dll";
    return std::filesystem::exists(dll_path, ec) && std::filesystem::is_regular_file(dll_path, ec);
}

bool IsVisionFlowDir(const std::filesystem::path &dir) {
    return ToLowerWideCopy(dir.filename().wstring()).find(L"visionflow") != std::wstring::npos;
}

std::vector<std::wstring> SplitEnvTokens(const std::wstring &value) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    for (wchar_t ch : value) {
        if (ch == L';' || ch == L',' || ch == L'|' || std::iswspace(ch)) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

void LogEnvSetting(const wchar_t *key) {
    const std::wstring value = GetEnvValue(key);
    const std::string key_text = WideToUtf8String(key);
    if (value.empty()) {
        AqLogInfo("vf", "环境变量 %s 为空。", key_text.c_str());
        return;
    }
    AqLogInfo("vf", "环境变量 %s=%s", key_text.c_str(), WideToUtf8String(value).c_str());
}

void AddVisionFlowCandidateDir(std::vector<std::filesystem::path> &dirs, const std::filesystem::path &dir) {
    AddCandidateDir(dirs, dir);
    AddCandidateDir(dirs, dir / "bin");
    AddCandidateDir(dirs, dir / "runtime");
    AddCandidateDir(dirs, dir / "DLLs");

    if (ContainsVisionFlowDll(dir / "bin")) {
        AddCandidateDir(dirs, dir);
        AddCandidateDir(dirs, dir / "bin");
        AddCandidateDir(dirs, dir / "bin" / "DLLs");
    }

    if (ContainsVisionFlowDll(dir)) {
        AddCandidateDir(dirs, dir);
        AddCandidateDir(dirs, dir / "DLLs");
    }
}

void AddVisionFlowCandidatesFromRoot(
    std::vector<std::filesystem::path> &dirs,
    const std::filesystem::path &root) {
    if (root.empty()) {
        return;
    }

    AddVisionFlowCandidateDir(dirs, root);

    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        return;
    }

    for (const auto &entry : std::filesystem::directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_directory()) {
            continue;
        }

        const std::filesystem::path child_dir = entry.path();
        if (IsVisionFlowDir(child_dir) || ContainsVisionFlowDll(child_dir) || ContainsVisionFlowDll(child_dir / "bin")) {
            AddVisionFlowCandidateDir(dirs, child_dir);
        }

        std::error_code child_ec;
        for (const auto &sub_entry : std::filesystem::directory_iterator(child_dir, child_ec)) {
            if (child_ec) {
                break;
            }
            if (!sub_entry.is_directory()) {
                continue;
            }

            const std::filesystem::path grandchild_dir = sub_entry.path();
            if (IsVisionFlowDir(grandchild_dir) ||
                ContainsVisionFlowDll(grandchild_dir) ||
                ContainsVisionFlowDll(grandchild_dir / "bin")) {
                AddVisionFlowCandidateDir(dirs, grandchild_dir);
            }
        }
    }
}

std::vector<std::filesystem::path> CollectVisionFlowSearchDirs() {
    std::vector<std::filesystem::path> dirs;

    const std::array<const wchar_t *, 4> env_keys = {
        L"AQ_VISIONFLOW_DLL_DIR",
        L"AQ_VISIONFLOW_ROOT",
        L"VISIONFLOW_DLL_DIR",
        L"VISIONFLOW_ROOT",
    };
    for (const wchar_t *key : env_keys) {
        LogEnvSetting(key);
        const std::wstring value = GetEnvValue(key);
        if (value.empty()) {
            continue;
        }
        const std::filesystem::path env_path(value);
        if (std::wstring(key).find(L"ROOT") != std::wstring::npos) {
            AddVisionFlowCandidatesFromRoot(dirs, env_path);
        } else {
            AddCandidateDir(dirs, env_path);
        }
    }

    LogEnvSetting(L"AIDIVersions");
    const std::wstring aidi_versions = GetEnvValue(L"AIDIVersions");
    for (const std::wstring &token : SplitEnvTokens(aidi_versions)) {
        const std::string token_text = WideToUtf8String(token);
        const std::wstring aidi_path = GetEnvValue(token.c_str());
        if (aidi_path.empty()) {
            AqLogWarn("vf", "AIDI 版本键 %s 未解析到有效路径。", token_text.c_str());
            continue;
        }
        AqLogInfo("vf", "AIDI 版本键 %s 对应路径=%s", token_text.c_str(), WideToUtf8String(aidi_path).c_str());
        AddVisionFlowCandidatesFromRoot(dirs, std::filesystem::path(aidi_path));
    }

    const std::filesystem::path exe_dir = GetExecutableDir();
    const std::filesystem::path module_dir = GetCurrentModuleDir();
    const std::filesystem::path cwd = std::filesystem::current_path();
    AddVisionFlowCandidatesFromRoot(dirs, exe_dir);
    AddVisionFlowCandidatesFromRoot(dirs, exe_dir.parent_path());
    AddVisionFlowCandidatesFromRoot(dirs, exe_dir.parent_path().parent_path());
    AddVisionFlowCandidatesFromRoot(dirs, exe_dir.parent_path().parent_path().parent_path());
    AddVisionFlowCandidatesFromRoot(dirs, module_dir);
    AddVisionFlowCandidatesFromRoot(dirs, module_dir.parent_path());
    AddVisionFlowCandidatesFromRoot(dirs, module_dir.parent_path().parent_path());
    AddVisionFlowCandidatesFromRoot(dirs, module_dir.parent_path().parent_path().parent_path());
    AddVisionFlowCandidatesFromRoot(dirs, cwd);
    AddVisionFlowCandidatesFromRoot(dirs, cwd.parent_path());

    std::vector<std::filesystem::path> deduped;
    for (const auto &dir : dirs) {
        bool exists = false;
        for (const auto &added : deduped) {
            if (added == dir) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            deduped.push_back(dir);
        }
    }
    AqLogInfo("vf", "已收集 %zu 个视觉流搜索目录。", deduped.size());
    for (std::size_t i = 0; i < deduped.size(); ++i) {
        AqLogInfo("vf", "搜索目录[%zu]=%s", i, PathToLogString(deduped[i]).c_str());
    }
    return deduped;
}

void PrependProcessPath(const std::vector<std::filesystem::path> &dirs) {
    std::wstring merged;
    for (const auto &dir : dirs) {
        if (!dir.empty()) {
            merged += dir.native();
            merged += L";";
        }
    }
    merged += GetEnvValue(L"PATH");
    _wputenv_s(L"PATH", merged.c_str());
}

void EnsureVisionFlowRuntimeLoaded() {
    if (HMODULE loaded = GetModuleHandleW(L"visionflow.dll"); loaded != nullptr) {
        AqLogInfo(
            "vf",
            "宿主进程已提前加载 visionflow.dll：%s",
            PathToLogString(GetModuleFilePath(loaded)).c_str());
        return;
    }

    const auto search_dirs = CollectVisionFlowSearchDirs();
    std::vector<std::filesystem::path> path_additions;
    for (const auto &dir : search_dirs) {
        const std::filesystem::path dll_path = dir / "visionflow.dll";
        std::error_code ec;
        if (!std::filesystem::exists(dll_path, ec) || !std::filesystem::is_regular_file(dll_path, ec)) {
            continue;
        }

        AqLogInfo("vf", "尝试加载视觉流运行时：%s", PathToLogString(dll_path).c_str());
        path_additions.push_back(dir);
        AddCandidateDir(path_additions, dir / "DLLs");
        PrependProcessPath(path_additions);

        if (HMODULE module = LoadLibraryExW(dll_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH); module != nullptr) {
            AqLogInfo("vf", "已从以下位置加载视觉流运行时：%s", PathToLogString(dll_path).c_str());
            AqLogInfo("vf", "视觉流模块实际加载路径：%s", PathToLogString(GetModuleFilePath(module)).c_str());
            return;
        }

        AqLogWarn("vf", "从以下位置加载视觉流运行时失败：%s，错误码=%lu", PathToLogString(dll_path).c_str(), GetLastError());
    }

    AqLogInfo("vf", "回退到系统默认方式加载 visionflow.dll。");
    if (HMODULE module = LoadLibraryW(L"visionflow.dll"); module != nullptr) {
        AqLogInfo("vf", "已通过默认搜索路径加载视觉流运行时。");
        AqLogInfo("vf", "视觉流模块实际加载路径：%s", PathToLogString(GetModuleFilePath(module)).c_str());
        return;
    }

    AqLogError("vf", "无法加载 visionflow.dll，错误码=%lu", GetLastError());
}

void InitializeVisionFlowOnce() {
    static std::once_flag once;
    static std::exception_ptr init_error;

    std::call_once(once, []() {
        try {
            AqLogInfo("vf", "开始初始化视觉流框架。");
            EnsureVisionFlowRuntimeLoaded();
            vflow::InitOptions init_options;
            vflow::LoggerOptions logger_options;
            logger_options.set_file_sink("visionflow.log");
            logger_options.set_stdout_sink(true);
            init_options.set_logger(logger_options);
            init_options.set_language("zh_CN");
            vflow::initialize(init_options);
            AqLogInfo("vf", "视觉流框架初始化成功。");
        } catch (const std::exception &ex) {
            AqLogError("vf", "视觉流框架初始化异常：%s", ex.what());
            init_error = std::current_exception();
        } catch (...) {
            AqLogError("vf", "视觉流框架初始化异常：未知错误。");
            init_error = std::current_exception();
        }
    });

    if (init_error != nullptr) {
        AqLogError("vf", "视觉流框架初始化处于失败状态，将重新抛出异常。");
        std::rethrow_exception(init_error);
    }

    AqLogInfo("vf", "视觉流框架初始化就绪。");
}

}  // namespace

struct XAITypeAQ::Impl {
    using ModelNameTypeMap =
        std::unordered_map<std::pair<std::string, std::string>, bool, PairHash, PairEqual>;
    using ModelNameLabelMap = std::unordered_map<std::string, std::vector<std::string>>;

    std::mutex mutex;
    XAI_config_t cfg{};
    std::string resolved_model_file;
    float score_threshold = 0.5F;
    int dedup_space_threshold = 15;
    int max_gpu_num = 1;
    std::string input_tool_id;
    std::unique_ptr<vflow::Model> model;
    std::shared_ptr<vflow::Runtime> runtime;
    ModelNameTypeMap model_name_type_map;
    ModelNameLabelMap model_name_label_map;
    std::unordered_map<int, HistoryEntry> history_by_roi;
};

XAITypeAQ::XAITypeAQ()
    : impl_(new Impl()),
      input_node_name_("输入"),
      result_node_name_(),
      run_loops_(1),
      model_loaded_(false) {
    AqLogInfo("lifecycle", "*** XAITypeAQ::XAITypeAQ() is called");
    AqLogInfo("lifecycle", "算法实例已创建：this=%p，impl_=%p。", this, impl_.get());
}

XAITypeAQ::~XAITypeAQ() {
    AqLogInfo("lifecycle", "*** XAITypeAQ::~XAITypeAQ() is called");
    AqLogInfo("lifecycle", "算法实例已销毁：this=%p，impl_=%p。", this, impl_.get());
}

int XAITypeAQ::get_cfg(XAI_config_t &cfg) {
    cfg = impl_->cfg;
    return 0;
}

int XAITypeAQ::start(const XAI_config_t *t_cfg) {
    AqLogInfo("start", "XAITypeAQ::start() is called");
    AqLogInfo("start", "收到启动调用：this=%p，impl_=%p，cfg=%p。", this, impl_.get(), t_cfg);
    std::cout << "model_dir: " << (t_cfg ? t_cfg->model_dir : "null") << std::endl;
    model_path_ = TrimCString(t_cfg->model_dir);
    AqLogInfo("start", "model_dir=%s", InputPathToLogString(model_path_).c_str());
    if (impl_ == nullptr) {
        AqLogError("start", "启动失败：内部实现对象为空，this=%p。", this);
        return -100;
    }

    try {
        return StartImpl(t_cfg);
    } catch (const std::exception &ex) {
        AqLogError("start", "start() 执行异常：%s", ex.what());
        return -101;
    } catch (...) {
        AqLogError("start", "start() 执行异常：未知错误。");
        return -101;
    }
}

int XAITypeAQ::StartImpl(const XAI_config_t *t_cfg) {
    if (t_cfg == nullptr) {
        AqLogError("start", "启动失败：配置指针为空。");
        return -1;
    }

    AqLogInfo("start", "进入启动实现：this=%p，impl_=%p，cfg=%p。", this, impl_.get(), t_cfg);

    std::lock_guard<std::mutex> guard(impl_->mutex);

    impl_->cfg = *t_cfg;
    impl_->history_by_roi.clear();

    model_path_ = TrimCString(t_cfg->model_dir);
    input_node_name_ = TrimCString(t_cfg->aq_param.prep_param.prepare_char_1);
    result_node_name_ = TrimCString(t_cfg->aq_param.prep_param.prepare_char_2);
    AqLogInfo(
        "start",
        "启动参数检查：model_dir长度=%zu，输入节点长度=%zu，结果节点长度=%zu。",
        model_path_.size(),
        input_node_name_.size(),
        result_node_name_.size());
    if (model_path_.empty()) {
        AqLogError("start", "启动失败：model_dir 为空。");
        model_loaded_ = false;
        return -12;
    }
    if (input_node_name_.empty()) {
        AqLogWarn("start", "输入节点为空，将使用默认值“输入”。");
        input_node_name_ = "输入";
    }
    if (result_node_name_.empty()) {
        AqLogWarn("start", "结果节点为空，将按模型输出节点自动遍历。");
    }

    run_loops_ = std::max(1, t_cfg->aq_param.prep_param.prepare_int_4 > 0
                                 ? t_cfg->aq_param.prep_param.prepare_int_4
                                 : 1);

    impl_->score_threshold = NormalizeScoreThreshold(t_cfg->aq_param.foreign_seg_score);
    impl_->dedup_space_threshold = std::max(1, t_cfg->aq_param.dedup_space_thres);
    impl_->max_gpu_num = t_cfg->aq_param.prep_param.prepare_int_3 == 0 ? 1 : t_cfg->aq_param.prep_param.prepare_int_3;

    AqLogInfo(
        "start",
        "开始启动：模型路径=%s，输入节点=%s，结果节点=%s，运行次数=%d，分数阈值=%.3f，去重距离阈值=%d，最大图形处理器数=%d，灰度阈值1=%d，灰度阈值2=%d，增强等级=%d",
        InputPathToLogString(model_path_).c_str(),
        input_node_name_.c_str(),
        result_node_name_.c_str(),
        run_loops_,
        impl_->score_threshold,
        impl_->dedup_space_threshold,
        impl_->max_gpu_num,
        impl_->cfg.gray_th1,
        impl_->cfg.gray_th2,
        impl_->cfg.aq_param.enhance_level);

    try {
        AqLogInfo("start", "准备初始化视觉流运行时。");
        InitializeVisionFlowOnce();

        AqLogInfo("start", "准备根据输入路径解析模型文件。");
        const std::filesystem::path model_file = ResolveModelFile(model_path_);
        if (model_file.empty()) {
            AqLogError("start", "启动失败：未能从 %s 解析出 .vfmodel 文件。", InputPathToLogString(model_path_).c_str());
            model_loaded_ = false;
            return -2;
        }

        impl_->resolved_model_file = model_file.u8string();
        AqLogInfo("start", "开始加载模型文件：%s", impl_->resolved_model_file.c_str());
        impl_->model.reset(new vflow::Model(cpp2x::String(impl_->resolved_model_file.c_str()), cpp2x::String("")));

        impl_->input_tool_id.clear();
        impl_->model_name_type_map.clear();
        impl_->model_name_label_map.clear();

        const auto tool_list = impl_->model->tool_list();
        AqLogInfo("start", "模型加载完成，工具节点数量=%zu", tool_list.size());
        for (std::size_t i = 0; i < tool_list.size(); ++i) {
            const auto tool_id = tool_list.get(i);
            const std::string tool_id_raw = tool_id.c_str();
            const std::string tool_name = tool_id_raw;
            const auto info = impl_->model->tool_info(tool_id);
            const std::string module_type = info.type().c_str();
            const std::string module_type_display = LocalizeModuleType(module_type);

            AqLogInfo(
                "start",
                "工具[%zu]：编号=%s，类型=%s",
                i,
                tool_id_raw.c_str(),
                module_type_display.c_str());

            if (module_type == "Input") {
                if (impl_->input_tool_id.empty()) {
                    impl_->input_tool_id = tool_id_raw;
                }
                if (!input_node_name_.empty() &&
                    (tool_name == input_node_name_ || tool_id_raw == input_node_name_)) {
                    impl_->input_tool_id = tool_id_raw;
                }
                continue;
            }

            if (module_type == "ViewTransformer") {
                continue;
            }

            impl_->model_name_type_map.emplace(std::make_pair(tool_name, module_type), false);

            std::vector<std::string> labels;
            try {
                const auto label_param = impl_->model
                                             ->get_param({tool_id, cpp2x::String("classes"), -1})
                                             .get()
                                             .as<vflow::param::LabelClasses>()
                                             .get();
                for (std::size_t j = 0; j < label_param.size(); ++j) {
                    labels.emplace_back(U8L(label_param.get(j)).c_str());
                }
                AqLogInfo("start", "工具[%s] 标签数量=%zu", tool_name.c_str(), labels.size());
            } catch (const std::exception &ex) {
                AqLogWarn("start", "工具[%s] 类别标签查询失败：%s", tool_name.c_str(), ex.what());
            } catch (...) {
                AqLogWarn("start", "工具[%s] 类别标签查询失败：未知错误。", tool_name.c_str());
            }
            impl_->model_name_label_map.emplace(tool_name, labels);
        }

        AqLogInfo(
            "start",
            "已选输入工具编号=%s，请求输入节点=%s，请求结果节点=%s",
            impl_->input_tool_id.c_str(),
            input_node_name_.c_str(),
            result_node_name_.c_str());
        if (impl_->input_tool_id.empty()) {
            AqLogWarn("start", "未从模型中解析到输入工具，后续写入样本图像时可能失败。");
        }

        auto BuildRuntime = [&](int max_gpu_num, bool cpu_fallback) {
            vflow::runtime::StrategyOptions strategy_options;
            strategy_options.set_allow_max_gpu_num(max_gpu_num);
            if (cpu_fallback || max_gpu_num == 0) {
                strategy_options.set_allow_run_on_cpu(true);
                strategy_options.set_allow_max_gpu_num(0);
            }
            vflow::runtime::AllTools all_tools(strategy_options);
            return std::make_shared<vflow::Runtime>(impl_->model->create_runtime(all_tools));
        };

        try {
            AqLogInfo("start", "开始创建运行时，允许使用的图形处理器数=%d", impl_->max_gpu_num);
            impl_->runtime = BuildRuntime(impl_->max_gpu_num, false);
            AqLogInfo("start", "运行时已按图形处理器策略创建成功。");
        } catch (const std::exception &ex) {
            AqLogWarn("start", "图形处理器运行时创建失败，准备回退到处理器：%s", ex.what());
            impl_->runtime = BuildRuntime(0, true);
            AqLogInfo("start", "运行时已按处理器回退策略创建成功。");
        } catch (...) {
            AqLogWarn("start", "图形处理器运行时创建失败，准备回退到处理器：未知错误。");
            impl_->runtime = BuildRuntime(0, true);
            AqLogInfo("start", "运行时已按处理器回退策略创建成功。");
        }

        model_loaded_ = (impl_->runtime != nullptr);
        AqLogInfo(
            "start",
            "启动结束：返回码=%d，模型已加载=%d，运行时就绪=%d",
            model_loaded_ ? 0 : -3,
            model_loaded_ ? 1 : 0,
            impl_->runtime != nullptr ? 1 : 0);
        return model_loaded_ ? 0 : -3;
    } catch (const std::exception &ex) {
        AqLogError("start", "启动异常：%s", ex.what());
        impl_->runtime.reset();
        impl_->model.reset();
        model_loaded_ = false;
        return -4;
    } catch (...) {
        AqLogError("start", "启动异常：未知错误。");
        impl_->runtime.reset();
        impl_->model.reset();
        model_loaded_ = false;
        return -4;
    }
}

int XAITypeAQ::stop(void) {
    AqLogInfo("stop", "*** stop() is called");
    std::lock_guard<std::mutex> guard(impl_->mutex);
    AqLogInfo(
        "stop",
        "开始停止：模型已加载=%d，历史区域数量=%zu",
        model_loaded_ ? 1 : 0,
        impl_->history_by_roi.size());
    impl_->history_by_roi.clear();
    impl_->runtime.reset();
    impl_->model.reset();
    impl_->model_name_type_map.clear();
    impl_->model_name_label_map.clear();
    model_loaded_ = false;
    AqLogInfo("stop", "停止完成：资源已释放。");
    return 0;
}

int XAITypeAQ::gen_roi_lst(
    XAI_src_t *src_i,
    cv::Point offset,
    std::vector<XAI_roi_t> &roi_lst,
    std::vector<XAI_roi_t> &roi_byd_lst,
    int is_scanning) {
    AqLogInfo(
        "infer",
        "*** gen_roi_lst() is called，this=%p，src_i=%p，roi_lst=%p，当前数量=%zu，is_scanning=%d，offset=%s。",
        this,
        src_i,
        &roi_lst,
        roi_lst.size(),
        is_scanning,
        PointToString(offset).c_str());
    AqLogInfo("infer", "gen_roi_lst byd input: roi_byd_lst=%p, count=%zu", &roi_byd_lst, roi_byd_lst.size());

    if (impl_ == nullptr) {
        AqLogError("infer", "gen_roi_lst 失败：内部实现对象为空。");
        return -100;
    }

    try {
        const int ret = XAIInstBase::gen_roi_lst(src_i, offset, roi_lst, roi_byd_lst, is_scanning);
        AqLogInfo("infer", "gen_roi_lst 执行完成：返回码=%d，输出数量=%zu。", ret, roi_lst.size());
        AqLogInfo("infer", "gen_roi_lst byd output count=%zu", roi_byd_lst.size());
        return ret;
    } catch (const std::exception &ex) {
        AqLogError("infer", "gen_roi_lst 执行异常：%s", ex.what());
        return -5;
    } catch (...) {
        AqLogError("infer", "gen_roi_lst 执行异常：未知错误。");
        return -5;
    }
}

int XAITypeAQ::preprocess_roi_lst(
    XAI_src_t *src_i,
    cv::Point offset,
    std::vector<XAI_roi_t> &roi_lst,
    int xdas_pw) {
    AqLogInfo(
        "preprocess",
        "*** preprocess_roi_lst() is called，this=%p，src_i=%p，roi_lst=%p，当前数量=%zu，xdas_pw=%d，offset=%s。",
        this,
        src_i,
        &roi_lst,
        roi_lst.size(),
        xdas_pw,
        PointToString(offset).c_str());

    if (impl_ == nullptr) {
        AqLogError("preprocess", "preprocess_roi_lst 失败：内部实现对象为空。");
        return -100;
    }

    try {
        const int ret = XAIInstBase::preprocess_roi_lst(src_i, offset, roi_lst, xdas_pw);
        AqLogInfo("preprocess", "preprocess_roi_lst 执行完成：返回码=%d，输出数量=%zu。", ret, roi_lst.size());
        return ret;
    } catch (const std::exception &ex) {
        AqLogError("preprocess", "preprocess_roi_lst 执行异常：%s", ex.what());
        return -5;
    } catch (...) {
        AqLogError("preprocess", "preprocess_roi_lst 执行异常：未知错误。");
        return -5;
    }
}

int XAITypeAQ::process_roi_lst(
    XAI_src_t *src_i,
    cv::Point offset,
    std::vector<XAI_roi_t> &roi_lst,
    unsigned long long idx,
    unsigned long long tm,
    int fps_us) {
    AqLogInfo(
        "infer",
        "*** process_roi_lst() is called，this=%p，src_i=%p，roi_lst=%p，当前数量=%zu，idx=%llu，tm=%llu，fps_us=%d，offset=%s。",
        this,
        src_i,
        &roi_lst,
        roi_lst.size(),
        static_cast<unsigned long long>(idx),
        static_cast<unsigned long long>(tm),
        fps_us,
        PointToString(offset).c_str());

    if (impl_ == nullptr) {
        AqLogError("infer", "process_roi_lst 失败：内部实现对象为空。");
        return -100;
    }

    try {
        const int ret = XAIInstBase::process_roi_lst(src_i, offset, roi_lst, idx, tm, fps_us);
        AqLogInfo("infer", "process_roi_lst 执行完成：返回码=%d，输出数量=%zu。", ret, roi_lst.size());
        return ret;
    } catch (const std::exception &ex) {
        AqLogError("infer", "process_roi_lst 执行异常：%s", ex.what());
        return -5;
    } catch (...) {
        AqLogError("infer", "process_roi_lst 执行异常：未知错误。");
        return -5;
    }
}

int XAITypeAQ::draw_roi_lst(
    cv::Mat &rgb,
    cv::Point offset,
    std::vector<XAI_roi_t> &roi_lst,
    int is_scanning,
    int mirror_tb,
    int mirror_lr) {
    AqLogInfo(
        "debug",
        "*** draw_roi_lst() is called，this=%p，rgb=%s，roi_lst=%p，当前数量=%zu，is_scanning=%d，mirror_tb=%d，mirror_lr=%d，offset=%s。",
        this,
        MatSummary(rgb).c_str(),
        &roi_lst,
        roi_lst.size(),
        is_scanning,
        mirror_tb,
        mirror_lr,
        PointToString(offset).c_str());

    if (impl_ == nullptr) {
        AqLogError("debug", "draw_roi_lst 失败：内部实现对象为空。");
        return -100;
    }

    try {
        const int ret = XAIInstBase::draw_roi_lst(rgb, offset, roi_lst, is_scanning, mirror_tb, mirror_lr);
        AqLogInfo("debug", "draw_roi_lst 执行完成：返回码=%d。", ret);
        return ret;
    } catch (const std::exception &ex) {
        AqLogError("debug", "draw_roi_lst 执行异常：%s", ex.what());
        return -5;
    } catch (...) {
        AqLogError("debug", "draw_roi_lst 执行异常：未知错误。");
        return -5;
    }
}

int XAITypeAQ::do_process_roi(XAI_src_t *src_i, cv::Point offset, XAI_roi_t &roi) {
    AqLogInfo("infer", "*** do_process_roi() is called");
    AqLogInfo(
        "infer",
        "收到区域处理调用：this=%p，impl_=%p，src_i=%p，roi=%p，偏移量=[横=%d,纵=%d]。",
        this,
        impl_.get(),
        src_i,
        &roi,
        offset.x,
        offset.y);

    if (impl_ == nullptr) {
        AqLogError("infer", "区域处理失败：内部实现对象为空，this=%p，src_i=%p，roi=%p。", this, src_i, &roi);
        return -100;
    }

    try {
        return DoProcessRoiImpl(src_i, offset, roi);
    } catch (const std::exception &ex) {
        AqLogError("infer", "do_process_roi() 执行异常：%s", ex.what());
        return -101;
    } catch (...) {
        AqLogError("infer", "do_process_roi() 执行异常：未知错误。");
        return -101;
    }
}

int XAITypeAQ::DoProcessRoiImpl(XAI_src_t *src_i, cv::Point offset, XAI_roi_t &roi) {
    if (src_i == nullptr || !model_loaded_ || impl_->runtime == nullptr || impl_->model == nullptr) {
        AqLogError(
            "infer",
            "区域处理在加锁前失败：src_i=%p，模型已加载=%d，运行时就绪=%d，模型指针=%p",
            src_i,
            model_loaded_ ? 1 : 0,
            impl_->runtime != nullptr ? 1 : 0,
            impl_->model.get());
        return -1;
    }

    std::lock_guard<std::mutex> guard(impl_->mutex);

    AqLogInfo(
        "infer",
        "开始区域处理：区域编号=%d，xdas_tm=%llu，偏移量=%s，输入区域=%s",
        roi.id,
        static_cast<unsigned long long>(roi.xdas_tm),
        PointToString(offset).c_str(),
        RectToString(roi.roi).c_str());
    AqLogInfo(
        "infer",
        "输入图摘要：xhl_l=%s，xhl_h=%s，log_l=%s，diff=%s，hsl_h=%s，xhl_c=%s。",
        MatSummary(src_i->xhl_l).c_str(),
        MatSummary(src_i->xhl_h).c_str(),
        MatSummary(src_i->log_l).c_str(),
        MatSummary(src_i->diff).c_str(),
        MatSummary(src_i->hsl_h).c_str(),
        MatSummary(src_i->xhl_c).c_str());

    const cv::Size image_size = !src_i->xhl_l.empty()
                                    ? src_i->xhl_l.size()
                                    : (!src_i->xhl_h.empty() ? src_i->xhl_h.size()
                                                             : (!src_i->log_l.empty() ? src_i->log_l.size()
                                                                                      : src_i->diff.size()));
    if (image_size.width <= 0 || image_size.height <= 0) {
        AqLogError(
            "infer",
            "区域处理失败：图像尺寸无效，xhl_l=%s，xhl_h=%s，log_l=%s，diff=%s",
            MatSummary(src_i->xhl_l).c_str(),
            MatSummary(src_i->xhl_h).c_str(),
            MatSummary(src_i->log_l).c_str(),
            MatSummary(src_i->diff).c_str());
        return -2;
    }

    cv::Rect local_roi;
    cv::Point global_origin = offset;
    if (roi.roi.width > 0 && roi.roi.height > 0) {
        local_roi = cv::Rect(roi.roi.tl() - offset, roi.roi.size());
        local_roi = ClampRectToImage(local_roi, image_size);
        global_origin = offset + local_roi.tl();
        roi.roi = cv::Rect(global_origin, local_roi.size());
    } else {
        local_roi = cv::Rect(0, 0, image_size.width, image_size.height);
        roi.roi = cv::Rect(offset.x, offset.y, image_size.width, image_size.height);
        global_origin = roi.roi.tl();
    }

    AqLogInfo(
        "infer",
        "区域准备完成：图像尺寸=%dx%d，局部区域=%s，全局起点=%s，输出区域=%s",
        image_size.width,
        image_size.height,
        RectToString(local_roi).c_str(),
        PointToString(global_origin).c_str(),
        RectToString(roi.roi).c_str());

    if (local_roi.empty()) {
        AqLogError("infer", "区域处理失败：裁剪后的区域为空。");
        return -3;
    }

    const cv::Mat infer_input = BuildInferImage(*src_i, local_roi, impl_->cfg);
    if (infer_input.empty()) {
        AqLogError("infer", "区域处理失败：推理输入图为空。");
        return -4;
    }

    AqLogInfo("infer", "推理输入已准备完成：%s", MatSummary(infer_input).c_str());

    std::vector<DetectionCandidate> all_candidates;
    try {
        int matched_output_count = 0;
        AqLogInfo("infer", "开始创建运行时样本。");
        auto sample = impl_->runtime->create_sample();
        AqLogInfo("infer", "运行时样本创建完成。");
        auto infer_image = MatToVFImage(infer_input);
        const std::string effective_input_id =
            impl_->input_tool_id.empty() ? input_node_name_ : impl_->input_tool_id;
        AqLogInfo("infer", "开始向样本写入图像：输入工具编号=%s", effective_input_id.c_str());
        vflow::helper::add_image_to_sample(sample, infer_image, cpp2x::String(effective_input_id.c_str()), false, 0);

        for (int i = 0; i < run_loops_; ++i) {
            AqLogInfo("infer", "执行运行时循环 %d/%d", i + 1, run_loops_);
            impl_->runtime->execute(sample, 0, true);
        }

        for (const auto &entry : impl_->model_name_type_map) {
            const std::string &tool_name = entry.first.first;
            const std::string &tool_type = entry.first.second;
            if (!result_node_name_.empty() && tool_name != result_node_name_) {
                continue;
            }

            ++matched_output_count;
            const cpp2x::String param_node = GetInferParamNode(tool_type);
            const std::string tool_type_display = LocalizeModuleType(tool_type);
            if (param_node.empty()) {
                AqLogWarn("infer", "跳过不支持输出节点的工具：工具=%s，类型=%s", tool_name.c_str(), tool_type_display.c_str());
                continue;
            }

            try {
                AqLogInfo("infer", "开始读取结果：工具=%s，类型=%s", tool_name.c_str(), tool_type_display.c_str());
                auto result = sample.get({cpp2x::String(tool_name.c_str()), param_node, -1});
                if (tool_type == "Classification") {
                    const auto regions = result.get().as<vflow::props::MultiNamesPolygonRegionList>();
                    auto candidates = ParseMultiNameRegionList(regions, impl_->score_threshold);
                    AqLogInfo("infer", "工具 %s 解析出的候选数量=%zu", tool_name.c_str(), candidates.size());
                    all_candidates.insert(all_candidates.end(), candidates.begin(), candidates.end());
                } else {
                    const auto regions = result.get().as<vflow::props::PolygonRegionList>();
                    auto candidates = ParsePolygonRegionList(regions, impl_->score_threshold);
                    AqLogInfo("infer", "工具 %s 解析出的候选数量=%zu", tool_name.c_str(), candidates.size());
                    all_candidates.insert(all_candidates.end(), candidates.begin(), candidates.end());
                }
            } catch (const std::exception &ex) {
                AqLogWarn("infer", "读取结果失败：工具=%s，类型=%s，错误=%s", tool_name.c_str(), tool_type_display.c_str(), ex.what());
            } catch (...) {
                AqLogWarn("infer", "读取结果失败：工具=%s，类型=%s，错误=未知错误", tool_name.c_str(), tool_type_display.c_str());
            }
        }

        AqLogInfo("infer", "输出节点命中数量=%d，原始候选数量=%zu", matched_output_count, all_candidates.size());
        if (matched_output_count == 0) {
            AqLogWarn("infer", "没有输出节点匹配到请求的结果节点=%s", result_node_name_.c_str());
        }
    } catch (const std::exception &ex) {
        AqLogError("infer", "区域处理运行时异常：%s", ex.what());
        return -5;
    } catch (...) {
        AqLogError("infer", "区域处理运行时异常：未知错误。");
        return -5;
    }

    const std::size_t raw_candidate_count = all_candidates.size();
    DeduplicateCandidates(all_candidates, 0.5F);
    AqLogInfo(
        "infer",
        "候选去重完成：去重前=%zu，去重后=%zu",
        raw_candidate_count,
        all_candidates.size());

    HistoryEntry *history = nullptr;
    if (roi.id > 0) {
        history = &impl_->history_by_roi[roi.id];
        AqLogInfo(
            "infer",
            "已启用历史去重：区域编号=%d，上次候选数量=%zu，上次xdas_tm=%llu",
            roi.id,
            history->candidates.size(),
            static_cast<unsigned long long>(history->xdas_tm));
    }

    if (history != nullptr && !history->candidates.empty()) {
        std::vector<DetectionCandidate> filtered;
        for (const DetectionCandidate &candidate : all_candidates) {
            bool duplicated = false;
            for (const DetectionCandidate &old_candidate : history->candidates) {
                const cv::Point delta = candidate.local_box.tl() - old_candidate.local_box.tl();
                if (candidate.subtype == old_candidate.subtype &&
                    (IoU(candidate.local_box, old_candidate.local_box) >= 0.5F ||
                     std::abs(delta.x) <= impl_->dedup_space_threshold)) {
                    duplicated = true;
                    break;
                }
            }
            if (!duplicated) {
                filtered.push_back(candidate);
            }
        }
        AqLogInfo(
            "infer",
            "历史去重完成：去重前=%zu，去重后=%zu",
            all_candidates.size(),
            filtered.size());
        all_candidates.swap(filtered);
    }

    if (history != nullptr) {
        history->xdas_tm = roi.xdas_tm;
        history->candidates = all_candidates;
        AqLogInfo(
            "infer",
            "历史缓存已更新：区域编号=%d，缓存候选数量=%zu，xdas_tm=%llu",
            roi.id,
            history->candidates.size(),
            static_cast<unsigned long long>(history->xdas_tm));
    }

    DetectionCandidate *body_candidate = nullptr;
    std::vector<cv::Rect> global_sub_roi;
    std::vector<DetectionCandidate> defect_candidates;

    for (DetectionCandidate &candidate : all_candidates) {
        if (IsBodyLabel(candidate.label) && body_candidate == nullptr) {
            body_candidate = &candidate;
            continue;
        }
        if (candidate.subtype == 0) {
            continue;
        }
        if (candidate.local_box.area() <= 0) {
            continue;
        }
        defect_candidates.push_back(candidate);
        global_sub_roi.emplace_back(candidate.local_box + global_origin);
    }

    roi.sub_roi = global_sub_roi;
    roi.type = defect_candidates.empty() ? 0 : 1;
    roi.sub_type = 0;
    for (const DetectionCandidate &candidate : defect_candidates) {
        roi.sub_type = std::max(roi.sub_type, candidate.subtype);
    }

    if (body_candidate != nullptr && body_candidate->local_contour.size() >= 3) {
        roi.contours.clear();
        roi.contours0.clear();
        for (const cv::Point &point : body_candidate->local_contour) {
            roi.contours.push_back(point + global_origin);
        }
        roi.contours0 = roi.contours;
        roi.roi = cv::boundingRect(roi.contours);
    } else if (roi.contours.empty()) {
        roi.contours = BuildRectContour(roi.roi);
        roi.contours0 = roi.contours;
    }

    if (roi.roi.width > 0 && roi.roi.height > 0) {
        roi.center = cv::Point(roi.roi.x + roi.roi.width / 2, roi.roi.y + roi.roi.height / 2);
    }

    if (roi.mask.empty()) {
        roi.mask = BuildLocalMask(roi.contours, roi.roi);
    }

    AqLogInfo(
        "infer",
        "区域处理结束：缺陷数量=%zu，结果类型=%d，结果子类型=%d，区域=%s，中心点=%s，子区域数量=%zu，轮廓点数量=%zu，掩码=%s",
        defect_candidates.size(),
        roi.type,
        roi.sub_type,
        RectToString(roi.roi).c_str(),
        PointToString(roi.center).c_str(),
        roi.sub_roi.size(),
        roi.contours.size(),
        MatSummary(roi.mask).c_str());

    SaveDebugImages(*src_i, local_roi, impl_->cfg, infer_input, roi, global_origin);

    return static_cast<int>(defect_candidates.size());
}

XAI_TYPE_AQ_EXPORT XAITypeBase *createInstance() {
    AqLogInfo("factory", "*** createInstance() is called");
    AqLogInfo("factory", "已调用实例创建接口。");
    XAITypeBase *instance = new XAITypeAQ();
    AqLogInfo("factory", "实例创建完成：instance=%p。", instance);
    return instance;
}

XAI_TYPE_AQ_EXPORT void destroyInstance(XAITypeBase *instance) {
    AqLogInfo("factory", "*** destroyInstance() is called");
    AqLogInfo("factory", "已调用实例销毁接口：实例指针=%p", instance);
    if (instance == nullptr) {
        AqLogWarn("factory", "实例销毁接口收到空指针，已忽略。");
        return;
    }
    delete instance;
}

XAI_TYPE_AQ_EXPORT int xai_start_instance(XAITypeBase *instance, const XAI_config_t *cfg) {
    AqLogInfo("factory", "*** xai_start_instance() is called");
    AqLogInfo("factory", "已调用启动桥接接口：instance=%p，cfg=%p。", instance, cfg);
    if (instance == nullptr) {
        AqLogError("factory", "启动桥接接口失败：实例指针为空。");
        return -201;
    }
    return instance->start(cfg);
}

XAI_TYPE_AQ_EXPORT int xai_stop_instance(XAITypeBase *instance) {
    AqLogInfo("factory", "*** xai_stop_instance() is called");
    AqLogInfo("factory", "已调用停止桥接接口：instance=%p。", instance);
    if (instance == nullptr) {
        AqLogError("factory", "停止桥接接口失败：实例指针为空。");
        return -201;
    }
    return instance->stop();
}

XAI_TYPE_AQ_EXPORT int xai_do_process_roi_instance(
    XAITypeBase *instance,
    XAI_src_t *src_i,
    int offset_x,
    int offset_y,
    XAI_roi_t *roi) {
    AqLogInfo("factory", "*** xai_do_process_roi_instance() is called");
    AqLogInfo(
        "factory",
        "已调用区域处理桥接接口：instance=%p，src_i=%p，roi=%p，offset=[横=%d,纵=%d]。",
        instance,
        src_i,
        roi,
        offset_x,
        offset_y);
    if (instance == nullptr || roi == nullptr) {
        AqLogError(
            "factory",
            "区域处理桥接接口失败：instance=%p，roi=%p，至少有一个为空。",
            instance,
            roi);
        return -201;
    }
    return instance->do_process_roi(src_i, cv::Point(offset_x, offset_y), *roi);
}
