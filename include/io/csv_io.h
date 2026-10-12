#ifndef POLYGONCLUSTERING_CSV_IO_H
#define POLYGONCLUSTERING_CSV_IO_H

#pragma once

#include <string>
#include <vector>

namespace CSVio {

    /**
     * Writes a CSV with columns: poly_id, cluster_id, is_medoid.
     * - poly_id is the index i (0..N-1)
     * - Overwrites the file if it exists
     * - Creates parent directories if they don't exist
     *
     * Throws std::invalid_argument on size mismatch.
     * Throws std::runtime_error on I/O failures.
     */
    void writeClustersToCsv(const std::string& filepath,
                            const std::vector<int>& cluster_ids,
                            const std::vector<int>& is_medoid);

} // namespace io


#endif //POLYGONCLUSTERING_CSV_IO_H