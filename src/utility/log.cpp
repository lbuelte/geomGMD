#include "utility/log.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>

namespace {

std::atomic<LogLevel> current_level{LogLevel::Info};
std::mutex log_mutex;
std::ofstream log_file;

std::string time_stamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // end of anonymous namespace


std::string_view to_string(LogLevel level)
{
    switch (level) {
        case LogLevel::Error:   return "error";
        case LogLevel::Warning: return "warning";
        case LogLevel::Info:    return "info";
        case LogLevel::Debug:   return "debug";
    }
    return "unknown";
}

std::optional<LogLevel> parse_log_level(std::string_view s)
{
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "error")   return LogLevel::Error;
    if (lower == "warning") return LogLevel::Warning;
    if (lower == "info")    return LogLevel::Info;
    if (lower == "debug")   return LogLevel::Debug;
    return std::nullopt;
}


namespace geomgmd::log {

void set_level(LogLevel level)
{
    current_level = level;
}

LogLevel level()
{
    return current_level;
}

bool is_enabled(LogLevel level)
{
    return static_cast<int>(level) <= static_cast<int>(current_level.load());
}

void set_file(const std::filesystem::path &path)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    if (log_file.is_open()) {
        log_file.close();
    }
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    log_file.open(path, std::ios::out | std::ios::trunc);
    if (not log_file) {
        throw std::runtime_error("Could not open log file " + path.string());
    }
}

void close_file()
{
    std::lock_guard<std::mutex> lock(log_mutex);
    if (log_file.is_open()) {
        log_file.close();
    }
}

void write(LogLevel level, std::string_view message)
{
    if (not is_enabled(level)) {
        return;
    }
    std::lock_guard<std::mutex> lock(log_mutex);
    std::ostream &console = (level == LogLevel::Error or level == LogLevel::Warning) ? std::cerr : std::cout;
    console << message << '\n';
    if (log_file.is_open()) {
        log_file << time_stamp() << " [" << to_string(level) << "] " << message << std::endl;
    }
}

} // namespace geomgmd::log
