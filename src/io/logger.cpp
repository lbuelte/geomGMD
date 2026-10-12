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


void log_clustering_result(std::ofstream &log, ClusteringResult const &clustering_result, std::string const &clustering_name, size_t starting_offset)
{
    log << "\n" << clustering_name << " with solution value " << clustering_result.clustering_solution_value << " :\n";
    for (int c = 0; c < clustering_result.clusters.size(); c++){
        log << "Cluster " << c << " with medoid " << clustering_result.medoid_indices[c] << " :\n";
        for (auto const & g : clustering_result.clusters[c]){
            log << g + starting_offset << " ";
        }
        log << "\n";
    }
}