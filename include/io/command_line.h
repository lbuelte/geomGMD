#ifndef GEOMGMD_IO_COMMAND_LINE_H
#define GEOMGMD_IO_COMMAND_LINE_H

#include "io/config.h"
#include "utility/log.h"

#include <optional>
#include <string>
#include <vector>

// Command line of the executables: positional arguments (e.g. graph ids, config file) plus options.
// Options (may appear anywhere, as "--option value" or "--option=value"):
//   --log-level <error|warning|info|debug>       console log level, overrides [Logging] level
//   --file-log-level <error|warning|info|debug>  log file level, overrides [Logging] file_level
struct CommandLine {
    std::vector<std::string> positional;
    std::optional<LogLevel> log_level;
    std::optional<LogLevel> file_log_level;
};

// Throws std::runtime_error for unknown options, missing option values or invalid log levels.
CommandLine parse_command_line(int argc, char* argv[]);

// Logging settings from the config, overridden by the command line options.
LoggingConfig resolve_logging(LoggingConfig const &config, CommandLine const &command_line);

#endif // GEOMGMD_IO_COMMAND_LINE_H
