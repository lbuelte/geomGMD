#include "io/command_line.h"

#include <stdexcept>
#include <string_view>

namespace {

LogLevel parse_level_option(std::string_view option, std::string const &value)
{
    if (auto level = parse_log_level(value)) {
        return *level;
    }
    throw std::runtime_error("Invalid value for " + std::string(option) + ": " + value
                             + " (expected error, warning, info or debug)");
}

} // end of anonymous namespace


CommandLine parse_command_line(int argc, char* argv[])
{
    CommandLine command_line;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--", 0) != 0) {
            command_line.positional.push_back(arg);
            continue;
        }

        // Split "--option=value" or take the value from the next argument
        std::string option = arg;
        std::optional<std::string> value;
        if (auto eq = arg.find('='); eq != std::string::npos) {
            option = arg.substr(0, eq);
            value = arg.substr(eq + 1);
        }
        auto take_value = [&]() -> std::string {
            if (value) return *value;
            if (i + 1 >= argc) throw std::runtime_error("Missing value for option " + option);
            return argv[++i];
        };

        if (option == "--log-level") {
            command_line.log_level = parse_level_option(option, take_value());
        } else if (option == "--file-log-level") {
            command_line.file_log_level = parse_level_option(option, take_value());
        } else {
            throw std::runtime_error("Unknown option: " + option);
        }
    }
    return command_line;
}

LoggingConfig resolve_logging(LoggingConfig const &config, CommandLine const &command_line)
{
    LoggingConfig logging = config;
    if (command_line.log_level) logging.level = *command_line.log_level;
    if (command_line.file_log_level) logging.file_level = *command_line.file_log_level;
    return logging;
}
