#include "mmessage.h"

#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace {

std::wstring GetEnvValue(const wchar_t *name) {
    wchar_t *raw_value = nullptr;
    std::size_t value_len = 0;
    if (_wdupenv_s(&raw_value, &value_len, name) != 0 || raw_value == nullptr || value_len == 0) {
        return std::wstring();
    }

    std::unique_ptr<wchar_t, decltype(&std::free)> holder(raw_value, &std::free);
    return std::wstring(holder.get());
}

std::filesystem::path GetCurrentModuleDir() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&m_msg_prt),
            &module) ||
        module == nullptr) {
        return std::filesystem::current_path();
    }

    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    }

    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path ResolveLogFilePath() {
    const std::wstring env_path = GetEnvValue(L"AQ_LOG_FILE");
    if (!env_path.empty()) {
        return std::filesystem::path(env_path);
    }

    return GetCurrentModuleDir() / "aq_plugin.log";
}

std::string BuildTimestampPrefix() {
    const auto now = std::chrono::system_clock::now();
    const auto now_time = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm local_tm{};
    localtime_s(&local_tm, &now_time);

    char buffer[128] = {0};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "[%04d-%02d-%02d %02d:%02d:%02d.%03lld][pid=%lu][tid=%lu] ",
        local_tm.tm_year + 1900,
        local_tm.tm_mon + 1,
        local_tm.tm_mday,
        local_tm.tm_hour,
        local_tm.tm_min,
        local_tm.tm_sec,
        static_cast<long long>(ms.count()),
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long>(GetCurrentThreadId()));
    return std::string(buffer);
}

std::string FormatMessage(const char *fmt, va_list args) {
    std::vector<char> buffer(512, '\0');
    va_list args_copy;
    va_copy(args_copy, args);
    int written = std::vsnprintf(buffer.data(), buffer.size(), fmt, args_copy);
    va_end(args_copy);

    if (written < 0) {
        return "日志格式化失败";
    }

    if (static_cast<std::size_t>(written) >= buffer.size()) {
        buffer.resize(static_cast<std::size_t>(written) + 1U);
        va_copy(args_copy, args);
        written = std::vsnprintf(buffer.data(), buffer.size(), fmt, args_copy);
        va_end(args_copy);
        if (written < 0) {
            return "日志格式化失败";
        }
    }

    return std::string(buffer.data());
}

void AppendLogFile(const std::filesystem::path &path, const std::string &line) {
    std::error_code ec;
    const bool needs_bom =
        !std::filesystem::exists(path, ec) ||
        (std::filesystem::exists(path, ec) && std::filesystem::file_size(path, ec) == 0);

    FILE *file = _wfopen(path.c_str(), L"ab");
    if (file == nullptr) {
        return;
    }

    if (needs_bom) {
        static const unsigned char kUtf8Bom[] = {0xEF, 0xBB, 0xBF};
        std::fwrite(kUtf8Bom, 1, sizeof(kUtf8Bom), file);
    }

    std::fwrite(line.data(), 1, line.size(), file);
    std::fflush(file);
    std::fclose(file);
}

}  // namespace

extern "C" void m_msg_prt(const char *fmt, ...) {
    if (fmt == nullptr) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    const std::string message = FormatMessage(fmt, args);
    va_end(args);

    const std::string line = BuildTimestampPrefix() + message + "\n";

    static std::mutex log_mutex;
    std::lock_guard<std::mutex> guard(log_mutex);

    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
    OutputDebugStringA(line.c_str());
    AppendLogFile(ResolveLogFilePath(), line);
}
