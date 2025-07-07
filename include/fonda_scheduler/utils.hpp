#ifndef UTILS_HPP
#define UTILS_HPP

#include "graph.hpp"

#include "csv/single_include/csv2/csv2.hpp"

namespace fonda_scheduler {

inline std::string trimQuotes(const std::string& str)
{
    if (str.empty()) {
        return str; // Return the empty string if input is empty
    }

    std::string result = str, prevResult;
    do {
        prevResult = result;
        size_t start = 0;
        size_t end = prevResult.length() - 1;

        // Check for leading quote
        if (prevResult[start] == '"' || prevResult[start] == '\\' || prevResult[start] == ' ') {
            start++;
        }

        // Check for trailing quote
        if (prevResult[end] == '"' || prevResult[end] == '\\' || prevResult[end] == ' ') {
            end--;
        }

        result = prevResult.substr(start, end - start + 1);
    } while (result != prevResult);
    return result;
}

inline auto loadTracesFile(const std::string& tracesFileName)
{
    csv2::Reader<csv2::delimiter<','>,
        csv2::quote_character<'"'>,
        csv2::first_row_is_header<true>,
        csv2::trim_policy::trim_whitespace>
        csv;

    if (!csv.mmap(tracesFileName)) {
        throw std::runtime_error("Failed to open traces file: " + tracesFileName);
    }

    std::unordered_map<std::string, std::vector<std::vector<std::string>>> workflow_rows;
    for (const auto row : csv) {
        std::vector<std::string> row_data;
        std::string task_name, workflow_name, inputSizeInRow;

        int col_idx = 0;
        for (const auto& cell : row) {
            std::string cell_value;
            cell.read_value(cell_value);
            row_data.push_back(cell_value);

            if (col_idx == 0) {
                workflow_name = cell_value;
            }
            if (col_idx == 1) {
                inputSizeInRow = cell_value;
            }

            if (col_idx == 2) {
                task_name = cell_value;
            }
            ++col_idx;
        }

        // Store row in the map under the workflow name
        workflow_rows[workflow_name.append(" ").append(task_name).append(" ").append(inputSizeInRow)].push_back(row_data);
    }

    return workflow_rows;
}

inline void scaleToFit(const graph_t* graphMemTopology, double biggestMem)
{
    static constexpr auto MEMORY_EPSILON = 1000;
    static constexpr auto MEMORY_DIVISION_FACTOR = 4;
    static constexpr auto N_TRIALS = 2;

    vertex_t* pv = graphMemTopology->first_vertex;

    auto scaleMemory = [&](auto memReqFunc, auto edgeCount, auto edgeAccessor, const char* direction) {
        for (int i = 0; i < N_TRIALS && memReqFunc(pv) > biggestMem; i++) {
            for (int j = 0; j < edgeCount; j++) {
                edgeAccessor(j)->weight /= MEMORY_DIVISION_FACTOR;
            }
        }
        if (memReqFunc(pv) > biggestMem) {
            throw std::runtime_error(std::string("(") + direction + ") Memory requirement of vertex " + std::string(pv->name) + " exceeds the biggest memory available in the cluster.");
        }
    };

    while (pv != nullptr) {
        if (peakMemoryRequirementOfVertex(pv) > pv->memoryRequirement) {
            pv->memoryRequirement = peakMemoryRequirementOfVertex(pv) + MEMORY_EPSILON;
        }

        scaleMemory(outMemoryRequirement, pv->out_degree, [&](const int j) { return pv->out_edges[j]; }, "Out");
        scaleMemory(inMemoryRequirement, pv->in_degree, [&](const int j) { return pv->in_edges[j]; }, "In");

        pv = pv->next;
    }
}

}

#endif // UTILS_HPP
