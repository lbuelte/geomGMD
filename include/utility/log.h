#ifndef GEOMGMD_UTILITY_LOG_H
#define GEOMGMD_UTILITY_LOG_H

// Small thread-safe logger with log levels.
//
// Messages are written to the console (errors and warnings to stderr, info and debug to stdout) and,
// if a log file was set, to the log file (with time stamp and level). Console and file have separate
// levels; a message is written to a sink if its level is at most the level of that sink.
// Defaults: console LogLevel::Info, file LogLevel::Debug (i.e. debug messages only go to the file).
// The arguments are only formatted if the message is written to at least one sink.
//
// Usage:
//     geomgmd::log::set_console_level(LogLevel::Warning);
//     geomgmd::log::set_file(output_path / "run.log");
//     geomgmd::log::info("Computed ", n, " pairs in ", seconds, " s.");

#include <cstddef>
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

// Maximum level of messages written to the console. Default: LogLevel::Info.
void set_console_level(LogLevel level);
LogLevel console_level();

// Maximum level of messages written to the log file. Default: LogLevel::Debug.
void set_file_level(LogLevel level);
LogLevel file_level();

// True if a message of the given level is written to at least one sink.
bool is_enabled(LogLevel level);

// Writes messages (up to the file level) to the given file (truncated; parent directories are created).
// Throws std::runtime_error if the file cannot be opened.
void set_file(const std::filesystem::path &path);
void close_file();

// Writes one message (one line) to the console and/or the log file, depending on their levels.
void write(LogLevel level, std::string_view message);

// Progress bar on the console (stdout), e.g. "[=====>     ] 42.0%  (21 / 50)", redrawn in place. Shown only if
// stdout is a terminal and info messages go to the console (not in redirected output, e.g. Slurm logs). A log
// message written while a bar is shown starts on a new line; finish_progress() ends the bar's line.
void progress(size_t current, size_t total);
void finish_progress();

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
