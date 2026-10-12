#include "../include/io/csv_io.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace CSVio {

    void writeClustersToCsv(const std::string& filepath,
                            const std::vector<int>& cluster_ids,
                            const std::vector<int>& is_medoid)
    {
        if (cluster_ids.size() != is_medoid.size()) {
            throw std::invalid_argument("cluster_ids and is_medoid must have the same size.");
        }

        namespace fs = std::filesystem;
        const fs::path outPath(filepath);

        // Create parent directory/directories if needed (and if there is a parent).
        const fs::path parent = outPath.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            fs::create_directories(parent, ec);
            if (ec) {
                throw std::runtime_error("Failed to create directories: " + parent.string());
            }
        }

        // Overwrite file if it exists.
        std::ofstream out(outPath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            throw std::runtime_error("Failed to open file for writing: " + outPath.string());
        }

        out << "poly_id,cluster_id,is_medoid\n";
        for (std::size_t i = 0; i < cluster_ids.size(); ++i) {
            out << i << "," << cluster_ids[i] << "," << is_medoid[i] << "\n";
        }

        if (!out.good()) {
            throw std::runtime_error("I/O error while writing: " + outPath.string());
        }
    }

} // namespace io
