#ifndef UTILS_HPP
#define UTILS_HPP

#include "graph.hpp"

namespace fonda_scheduler {

inline auto loadTracesFile(const std::string& tracesFileName)
{
    csv2::Reader<csv2::delimiter<','>,
        csv2::quote_character<'"'>,
        csv2::first_row_is_header<true>,
        csv2::trim_policy::trim_whitespace>
        csv;

    const auto success = csv.mmap(tracesFileName);

    if (!success) {
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

inline void scaleToFit(graph_t* graphMemTopology, double biggestMem)
{
    vertex_t* pv = graphMemTopology->first_vertex;
    while (pv != nullptr) {
        const auto MEMORY_EPSILON = 1000;
        const auto MEMORY_DIVISION_FACTOR = 4;
        if (peakMemoryRequirementOfVertex(pv) > pv->memoryRequirement) {
            pv->memoryRequirement = peakMemoryRequirementOfVertex(pv) + MEMORY_EPSILON;
            // cout<<"peak of "<< pv->name<<" "<<peakMemoryRequirementOfVertex(pv)<<endl;
        }
        if (outMemoryRequirement(pv) > biggestMem) {
            // cout<<"WILL BE INVALID "<< outMemoryRequirement(pv)<<" vs "<<biggestMem<< " on "<<pv->name<< endl;

            for (int i = 0; i < pv->out_degree; i++) {
                pv->out_edges[i]->weight /= MEMORY_DIVISION_FACTOR;
                // throw an error
            }
            double d = inMemoryRequirement(pv);
            double requirement = outMemoryRequirement(pv);
            if (outMemoryRequirement(pv) > biggestMem) {

                // cout<<"WILL BE INVALID "<< outMemoryRequirement(pv)<<" vs "<<biggestMem<< " on "<<pv->name<< endl;
                for (int i = 0; i < pv->out_degree; i++) {
                    pv->out_edges[i]->weight /= MEMORY_DIVISION_FACTOR;
                    // throw an error
                }
                if (outMemoryRequirement(pv) > biggestMem) {
                    return 0;
                }
            }
        }

        if (inMemoryRequirement(pv) > biggestMem) {
            // cout<<"WILL BE INVALID "<< inMemoryRequirement(pv)<<" vs "<<biggestMem<< " on "<<pv->name<< endl;
            for (int i = 0; i < pv->in_degree; i++) {
                pv->in_edges[i]->weight /= MEMORY_DIVISION_FACTOR;
            }

            if (inMemoryRequirement(pv) > biggestMem) {
                // cout<<"WILL BE INVALID "<< outMemoryRequirement(pv)<<" vs "<<biggestMem<< " on "<<pv->name<< endl;
                for (int i = 0; i < pv->in_degree; i++) {
                    pv->in_edges[i]->weight /= MEMORY_DIVISION_FACTOR;
                }
                if (inMemoryRequirement(pv) > biggestMem) {
                    throw std::runtime_error("Memory requirement of vertex " + std::string(pv->name) + " exceeds the biggest memory available in the cluster.");
                }
            }
        }
        pv = pv->next;
    }
}
}

#endif // UTILS_HPP
