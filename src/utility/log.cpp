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

std::atomic<LogLevel> console_max_level{LogLevel::Info};
std::atomic<LogLevel> file_max_level{LogLevel::Debug};
std::atomic<bool> file_is_open{false};
std::mutex log_mutex;
std::ofstream log_file;

bool at_most(LogLevel level, LogLevel max_level)
{
    return static_cast<int>(level) <= static_cast<int>(max_level);
}

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

void set_console_level(LogLevel level)
{
    console_max_level = level;
}

LogLevel console_level()
{
    return console_max_level;
}

void set_file_level(LogLevel level)
{
    file_max_level = level;
}

LogLevel file_level()
{
    return file_max_level;
}

bool is_enabled(LogLevel level)
{
    return at_most(level, console_max_level) or (file_is_open and at_most(level, file_max_level));
}

void set_file(const std::filesystem::path &path)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    if (log_file.is_open()) {
        log_file.close();
        file_is_open = false;
    }
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    log_file.open(path, std::ios::out | std::ios::trunc);
    if (not log_file) {
        throw std::runtime_error("Could not open log file " + path.string());
    }
    file_is_open = true;
}

void close_file()
{
    std::lock_guard<std::mutex> lock(log_mutex);
    if (log_file.is_open()) {
        log_file.close();
    }
    file_is_open = false;
}

void write(LogLevel level, std::string_view message)
{
    const bool to_console = at_most(level, console_max_level);
    const bool to_file = file_is_open and at_most(level, file_max_level);
    if (not to_console and not to_file) {
        return;
    }
    std::lock_guard<std::mutex> lock(log_mutex);
    if (to_console) {
        std::ostream &console = (level == LogLevel::Error or level == LogLevel::Warning) ? std::cerr : std::cout;
        console << message << '\n';
    }
    if (to_file and log_file.is_open()) {
        log_file << time_stamp() << " [" << to_string(level) << "] " << message << std::endl;
    }
}

} // namespace geomgmd::log
