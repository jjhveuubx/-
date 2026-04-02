#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>

#include <opencv2/opencv.hpp>

#include "xai_type_base.h"

namespace {

using CreateInstanceFn = XAITypeBase *(*)();
using DestroyInstanceFn = void (*)(XAITypeBase *);

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

std::filesystem::path GetExecutableDir() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path GetEnvPath(const wchar_t *name) {
    wchar_t *raw_value = nullptr;
    std::size_t value_len = 0;
    if (_wdupenv_s(&raw_value, &value_len, name) != 0 || raw_value == nullptr || value_len == 0) {
        return {};
    }
    std::unique_ptr<wchar_t, decltype(&std::free)> holder(raw_value, &std::free);
    return std::filesystem::path(holder.get());
}

bool FileExists(const std::filesystem::path &path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec);
}

bool DirExists(const std::filesystem::path &path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec);
}

std::filesystem::path ResolveDllDir(const std::filesystem::path &exe_dir) {
    const std::filesystem::path env_dir = GetEnvPath(L"AQ_DLL_DIR");
    if (!env_dir.empty()) {
        return env_dir;
    }

    const std::vector<std::filesystem::path> candidates = {
        exe_dir,
        exe_dir / "bin",
        exe_dir.parent_path(),
        exe_dir.parent_path() / "bin",
    };

    for (const auto &dir : candidates) {
        if (FileExists(dir / "xai_type_aq.dll")) {
            return dir;
        }
    }

    return exe_dir;
}

std::filesystem::path ResolveModelDir(const std::filesystem::path &exe_dir, const std::filesystem::path &dll_dir) {
    const std::filesystem::path env_dir = GetEnvPath(L"AQ_MODEL_DIR");
    if (!env_dir.empty()) {
        return env_dir;
    }

    const std::vector<std::filesystem::path> candidates = {
        dll_dir / "model",
        exe_dir / "model",
        exe_dir.parent_path() / "model",
        dll_dir.parent_path() / "model",
    };

    for (const auto &dir : candidates) {
        if (DirExists(dir)) {
            return dir;
        }
    }

    return dll_dir / "model";
}

bool ShouldPauseOnExit() {
    DWORD process_ids[8] = {};
    const DWORD count = GetConsoleProcessList(process_ids, static_cast<DWORD>(std::size(process_ids)));
    return count <= 1;
}

int ExitWithPauseIfNeeded(int code, const std::string &message) {
    if (!message.empty()) {
        std::cout << message << std::endl;
    }

    if (ShouldPauseOnExit()) {
        std::cout << "按回车键退出..." << std::endl;
        std::cin.get();
    }
    return code;
}

}  // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    const std::filesystem::path exe_dir = GetExecutableDir();
    const std::filesystem::path dll_dir = ResolveDllDir(exe_dir);
    const std::filesystem::path model_dir = ResolveModelDir(exe_dir, dll_dir);
    const std::filesystem::path dll_path = dll_dir / "xai_type_aq.dll";
    const std::filesystem::path package_dll_dir = dll_dir / "DLLs";

    std::cout << "可执行目录=" << exe_dir.u8string() << std::endl;
    std::cout << "动态库目录=" << dll_dir.u8string() << std::endl;
    std::cout << "模型目录=" << model_dir.u8string() << std::endl;
    std::cout << "动态库路径=" << dll_path.u8string() << std::endl;

    wchar_t *raw_path = nullptr;
    std::size_t path_length = 0;
    if (_wdupenv_s(&raw_path, &path_length, L"PATH") != 0) {
        raw_path = nullptr;
    }
    std::unique_ptr<wchar_t, decltype(&std::free)> original_path(raw_path, &std::free);

    std::wstring merged_path = dll_dir.native() + L";";
    if (std::filesystem::exists(package_dll_dir)) {
        merged_path += package_dll_dir.native() + L";";
    }
    if (original_path) {
        merged_path += original_path.get();
    }
    _wputenv_s(L"PATH", merged_path.c_str());
    HMODULE module = LoadLibraryExW(dll_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (module == nullptr) {
        std::cerr << "动态加载库失败，错误码=" << GetLastError() << std::endl;
        return ExitWithPauseIfNeeded(1, "自测在创建实例前失败。");
    }

    const auto create_instance =
        reinterpret_cast<CreateInstanceFn>(GetProcAddress(module, "createInstance"));
    const auto destroy_instance =
        reinterpret_cast<DestroyInstanceFn>(GetProcAddress(module, "destroyInstance"));
    if (create_instance == nullptr || destroy_instance == nullptr) {
        std::cerr << "获取导出函数地址失败。" << std::endl;
        FreeLibrary(module);
        return ExitWithPauseIfNeeded(2, "自测在解析导出接口时失败。");
    }

    XAITypeBase *instance = create_instance();
    if (instance == nullptr) {
        std::cerr << "实例创建接口返回空指针。" << std::endl;
        FreeLibrary(module);
        return ExitWithPauseIfNeeded(3, "自测失败：实例创建接口返回空指针。");
    }

    XAI_config_t cfg{};
    const std::string model_dir_u8 = model_dir.u8string();
    std::strncpy(cfg.model_dir, model_dir_u8.c_str(), sizeof(cfg.model_dir) - 1);
    std::strncpy(cfg.aq_param.prep_param.prepare_char_1, "输入", sizeof(cfg.aq_param.prep_param.prepare_char_1) - 1);
    cfg.aq_param.foreign_seg_score = 10;
    cfg.aq_param.dedup_space_thres = 15;
    cfg.gray_th1 = 0;
    cfg.gray_th2 = 65535;

    const int start_rc = instance->start(&cfg);
    std::cout << "启动返回码=" << start_rc << std::endl;
    if (start_rc != 0) {
        destroy_instance(instance);
        FreeLibrary(module);
        return ExitWithPauseIfNeeded(4, "自测失败：启动接口执行异常。");
    }

    XAI_src_t src;
    src.xhl_l = cv::Mat(256, 256, CV_16UC1);
    src.xhl_h = cv::Mat(256, 256, CV_16UC1);
    src.norm_l = cv::Mat(256, 256, CV_8UC1);
    src.norm_h = cv::Mat(256, 256, CV_8UC1);
    src.log_l = cv::Mat(256, 256, CV_8UC1);
    src.diff = cv::Mat(256, 256, CV_8UC1);
    FillGradient16(src.xhl_l, 0);
    FillGradient16(src.xhl_h, 1024);
    FillGradient8(src.norm_l, 16);
    FillGradient8(src.norm_h, 32);
    FillGradient8(src.log_l, 0);
    FillGradient8(src.diff, 64);

    XAI_roi_t roi{};
    roi.id = 1;
    roi.xdas_tm = 1;
    roi.roi = cv::Rect(0, 0, 256, 256);

    const int process_rc = instance->do_process_roi(&src, cv::Point(0, 0), roi);
    std::cout << "推理返回码=" << process_rc << std::endl;
    std::cout << "区域类型=" << roi.type << std::endl;
    std::cout << "区域子类型=" << roi.sub_type << std::endl;
    std::cout << "子区域数量=" << roi.sub_roi.size() << std::endl;

    const int stop_rc = instance->stop();
    std::cout << "停止返回码=" << stop_rc << std::endl;

    destroy_instance(instance);
    FreeLibrary(module);
    return ExitWithPauseIfNeeded(0, "自测完成。");
}
