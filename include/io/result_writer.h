// Created by Laura Bülte on 2026-03-30.

#ifndef RESULT_WRITER_H
#define RESULT_WRITER_H

#include <fstream>
#include <mutex>
#include <string>

#include "json/json.hpp"

class JSONWriter {
private:
    std::ofstream out;
    std::mutex mtx;

public:
    JSONWriter(const std::string& filename) {
        out.open(filename, std::ios::app);
        if (!out) {
            throw std::runtime_error("Failed to open file");
        }
    }

    void write(const nlohmann::json& j) {
        std::lock_guard<std::mutex> lock(mtx);
        out << j.dump() << "\n";
    }

    void flush() {
        out.flush();
    }
};

class CSVWriter {
private:
    std::ofstream out;
    std::mutex mtx;

public:
    CSVWriter(const std::string& filename) {
        out.open(filename, std::ios::app);
        if (!out) {
            throw std::runtime_error("Failed to open file");
        }
    }

    void write_row(const std::vector<std::string>& columns) {
        std::lock_guard<std::mutex> lock(mtx);

        for (size_t i = 0; i < columns.size(); ++i) {
            out << columns[i];
            if (i + 1 < columns.size()) {
                out << ",";
            }
        }
        out << "\n";
    }

    void flush() {
        std::lock_guard<std::mutex> lock(mtx);
        out.flush();
    }
};


#endif // RESULT_WRITER_H