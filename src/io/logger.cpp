#include "io/logger.h"

#include <iostream>

std::ofstream global_log;

void init_global_log(const std::string &filename)
{
    namespace fs = std::filesystem;

    try {
        fs::path path(filename);
        fs::path dir = path.parent_path();

        // Create directory if it doesn't exist and is not empty
        if (!dir.empty() && !fs::exists(dir)) {
            std::cout << "creating dir " << filename << std::endl;
            fs::create_directories(dir);
        }

        global_log.open(filename, std::ios::out);
        if (!global_log) {
            std::cerr << "ERROR: Could not open log file: " << filename << "\n";
            return;
        }

        global_log << std::unitbuf;  // auto-flush
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    }
}

void close_global_log()
{
    if (global_log.is_open())
        global_log.close();
}