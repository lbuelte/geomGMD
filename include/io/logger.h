#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <filesystem>
#include <string>


extern std::ofstream global_log;

void init_global_log(const std::string &filename);
void close_global_log();


#endif //LOGGER_H