#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <filesystem>

#include "../clustering/clustering_base.h"


extern std::ofstream global_log;

void init_global_log(const std::string &filename);
void close_global_log();

void log_clustering_result(
        std::ofstream &log, 
        ClusteringResult const &clustering_result, 
        std::string const &clustering_name, 
        size_t starting_offset = 0);


#endif //LOGGER_H