#include <opencv2/imgproc.hpp>

#if defined(XAI_OPENCV_RUNTIME_LEGACY)
#include <windows.h>

#include <stdexcept>
#endif

namespace cv {

#if defined(XAI_OPENCV_RUNTIME_LEGACY)

namespace {

using LegacyGaussianBlurFn = void(__cdecl *)(
    const InputArray &,
    const OutputArray &,
    Size,
    double,
    double,
    int);

using LegacyCvtColorFn = void(__cdecl *)(
    const InputArray &,
    const OutputArray &,
    int,
    int);

LegacyGaussianBlurFn ResolveLegacyGaussianBlur() {
    static LegacyGaussianBlurFn fn = []() -> LegacyGaussianBlurFn {
        HMODULE module = GetModuleHandleW(L"" XAI_OPENCV_WORLD_DLL_NAME);
        if (module == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<LegacyGaussianBlurFn>(GetProcAddress(
            module,
            "?GaussianBlur@cv@@YAXAEBV_InputArray@1@AEBV_OutputArray@1@V?$Size_@H@1@NNH@Z"));
    }();
    return fn;
}

LegacyCvtColorFn ResolveLegacyCvtColor() {
    static LegacyCvtColorFn fn = []() -> LegacyCvtColorFn {
        HMODULE module = GetModuleHandleW(L"" XAI_OPENCV_WORLD_DLL_NAME);
        if (module == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<LegacyCvtColorFn>(GetProcAddress(
            module,
            "?cvtColor@cv@@YAXAEBV_InputArray@1@AEBV_OutputArray@1@HH@Z"));
    }();
    return fn;
}

}  // namespace

void GaussianBlur(
    const InputArray &src,
    const OutputArray &dst,
    Size ksize,
    double sigma_x,
    double sigma_y,
    int border_type,
    AlgorithmHint) {
    const LegacyGaussianBlurFn fn = ResolveLegacyGaussianBlur();
    if (fn == nullptr) {
        throw std::runtime_error("Failed to resolve legacy cv::GaussianBlur");
    }
    fn(src, dst, ksize, sigma_x, sigma_y, border_type);
}

void cvtColor(
    const InputArray &src,
    const OutputArray &dst,
    int code,
    int dst_cn,
    AlgorithmHint) {
    const LegacyCvtColorFn fn = ResolveLegacyCvtColor();
    if (fn == nullptr) {
        throw std::runtime_error("Failed to resolve legacy cv::cvtColor");
    }
    fn(src, dst, code, dst_cn);
}

#else

void GaussianBlur(
    const InputArray &src,
    const OutputArray &dst,
    Size ksize,
    double sigma_x,
    double sigma_y,
    int border_type) {
    GaussianBlur(src, dst, ksize, sigma_x, sigma_y, border_type, ALGO_HINT_DEFAULT);
}

void cvtColor(
    const InputArray &src,
    const OutputArray &dst,
    int code,
    int dst_cn) {
    cvtColor(src, dst, code, dst_cn, ALGO_HINT_DEFAULT);
}

#endif

}  // namespace cv
