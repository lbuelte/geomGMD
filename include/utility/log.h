#ifndef GEOMGMD_UTILITY_LOG_H
#define GEOMGMD_UTILITY_LOG_H

// Small thread-safe logger with log levels.
//
// Every message is written to the console (errors and warnings to stderr, info and debug to stdout)
// and, if a log file was set, additionally to the log file (with time stamp and level).
// Messages above the current log level are discarded; the arguments are only formatted if the
// message is actually written.
//
// Usage:
//     geomgmd::log::set_level(LogLevel::Debug);
//     geomgmd::log::set_file(output_path / "run.log");
//     geomgmd::log::info("Computed ", n, " pairs in ", seconds, " s.");

#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

enum class LogLevel { Error = 0, Warning = 1, Info = 2, Debug = 3 };

std::string_view to_string(LogLevel level);

// Parses "error", "warning", "info" or "debug" (case-insensitive).
std::optional<LogLevel> parse_log_level(std::string_view s);

namespace geomgmd::log {

// Messages with a level above the given one are discarded. Default: LogLevel::Info.
void set_level(LogLevel level);
LogLevel level();
bool is_enabled(LogLevel level);

// Additionally writes all messages to the given file (truncated; parent directories are created).
// Throws std::runtime_error if the file cannot be opened.
void set_file(const std::filesystem::path &path);
void close_file();

// Writes one message (one line) to the console and, if set, to the log file.
void write(LogLevel level, std::string_view message);

template<class... Args>
void message(LogLevel level, Args&&... args)
{
    if (not is_enabled(level)) {
        return;
    }
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    write(level, oss.str());
}

template<class... Args>
void error(Args&&... args) { message(LogLevel::Error, std::forward<Args>(args)...); }

template<class... Args>
void warning(Args&&... args) { message(LogLevel::Warning, std::forward<Args>(args)...); }

template<class... Args>
void info(Args&&... args) { message(LogLevel::Info, std::forward<Args>(args)...); }

template<class... Args>
void debug(Args&&... args) { message(LogLevel::Debug, std::forward<Args>(args)...); }

} // namespace geomgmd::log

#endif // GEOMGMD_UTILITY_LOG_H
