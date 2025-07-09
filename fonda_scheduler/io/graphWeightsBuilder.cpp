
#include <iostream>
#include <iterator>
#include <sstream>

#include "../extlibs/csv/single_include/csv2/csv2.hpp"
#include "fonda_scheduler/common.hpp"
#include <fonda_scheduler/io/graphWeightsBuilder.hpp>
#include <regex>

namespace Fonda {

enum MACHINE_COLUMNS {
    // machine,amount,cpu,memory,storage,cpu_events,linpack,ram_score,read_iops,write_iops
    MACHINE_NAME = 0,
    PROCESSOR_AMOUNT = 1,
    PROCESSOR_SPEED = 2,
    MEMORY_AMOUNT = 3,
    STORAGE_SIZE = 4,
    CPU_EVENTS = 5,
    LINPACK = 6,
    RAM_SCORE = 7,
    READ_IOPS = 8,
    WRITE_IOPS = 9,
};

Cluster* buildClusterFromCsv(const std::string& file, const int memoryMultiplicator, const double readWritePenalty, const double offloadPenalty, const int speedMultiplicator)
{
    csv2::Reader<csv2::delimiter<','>,
        csv2::quote_character<'"'>,
        csv2::first_row_is_header<true>,
        csv2::trim_policy::trim_whitespace>
        csv;

    auto* cluster = new Cluster();
    if (csv.mmap(file)) {
        int id = 0;
        for (const auto row : csv) {
            auto p = std::make_shared<Processor>();

            if (row.length() > 0) {
                p->id = id;
                cluster->addProcessor(p);
                id++;
            }

            int proc_count = 0;
            int cell_cntr = 0;
            for (const auto& cell : row) {
                std::string cell_value;
                cell.read_value(cell_value);

                switch (cell_cntr) {
                case MACHINE_NAME:
                    p->name = cell_value;
                    break;
                case PROCESSOR_AMOUNT:
                    proc_count = stoi(cell_value);
                    break;
                case PROCESSOR_SPEED:
                    p->setProcessorSpeed(stod(cell_value) * speedMultiplicator);
                    break;
                case MEMORY_AMOUNT:
                    p->setMemorySize(stod(cell_value) * memoryMultiplicator);
                    p->setAvailableMemory(p->getMemorySize());
                    p->setAfterAvailableMemory(p->getMemorySize());
                    break;
                case READ_IOPS:
                    p->readSpeedDisk = stod(cell_value) * readWritePenalty;
                    break;
                case WRITE_IOPS:
                    p->writeSpeedDisk = stod(cell_value) * readWritePenalty;
                    p->memoryOffloadingPenalty = stod(cell_value) * offloadPenalty;
                    break;
                default:
                    // Nothing to do
                    // TODO read/write iops
                    break;
                }
                ++cell_cntr;
            }

            for (int i = 1; i < proc_count; i++) {
                auto p1 = std::make_shared<Processor>(*p);
                p1->id = id;
                id++;
                cluster->addProcessor(p1);
            }
        }
    }

    return cluster;
}

enum WORKFLOW_COLUMNS {
    PROC_NAME = 3,
    PROC_SPEED_SCALE = 4,
    AVG_MEMORY = 5,
    AVG_WCHAR = 6,
    AVG_TINPS = 7,
};

void fillGraphWeightsFromExternalSource(graph_t* graphMemTopology,
    std::unordered_map<std::string, std::vector<std::vector<std::string>>> workflow_rows,
    const std::string& workflow_name, long inputSize, Cluster* cluster,
    int memShorteningDivision, double ioShorteningCoef)
{

    double minMem = std::numeric_limits<double>::max(), minTime = std::numeric_limits<double>::max(), minWchar = std::numeric_limits<double>::max(),
           mintt = std::numeric_limits<double>::max();
    for (vertex_t* v = graphMemTopology->first_vertex; v; v = v->next) {
        v->bottom_level = -1;
        v->factorForRealExecution = 1;
        std::string lowercase_name = v->name;
        std::regex pattern("_\\d+");
        lowercase_name = std::regex_replace(lowercase_name, pattern, "");
        std::string workflow_name1 = std::regex_replace(workflow_name, pattern, "");
        std::transform(lowercase_name.begin(),
            lowercase_name.end(),
            lowercase_name.begin(),
            tolower);

        std::string nameToSearch = workflow_name1.append(" ").append(lowercase_name).append(" ").append(std::to_string(inputSize));

        if (workflow_rows.find(nameToSearch) == workflow_rows.end()) {
            continue;
        }

        double avgMem = 0;
        double avgTime = 0;
        double avgwchar = 0;
        double avgtinps = 0;

        for (const auto& row : workflow_rows[nameToSearch]) {
            int col_idx = 0;
            std::string proc_name;
            for (const auto& cell : row) {
                switch (col_idx) {
                case PROC_NAME:
                    proc_name = cell;
                    break;
                case PROC_SPEED_SCALE:
                    avgTime += stod(cell) * cluster->getOneProcessorByName(proc_name)->getProcessorSpeed();
                    break;
                case AVG_MEMORY:
                    avgMem += stod(cell) / memShorteningDivision;
                    break;
                case AVG_WCHAR:
                    avgwchar += stod(cell) / (memShorteningDivision * ioShorteningCoef);
                    break;
                case AVG_TINPS:
                    avgtinps += stod(cell) / (memShorteningDivision * ioShorteningCoef);
                    break;
                default:
                    // Nothing to do
                    break;
                }

                col_idx++;
            }
        }

        if (workflow_rows[nameToSearch].empty()) {
            std::cout << "<<<<<" << '\n';
        }

        const auto numOfMsrs = static_cast<double>(workflow_rows[nameToSearch].size());
        avgMem /= numOfMsrs;
        avgTime /= numOfMsrs;
        avgwchar /= numOfMsrs;
        avgtinps /= numOfMsrs;

        v->time = avgTime; // avgTime==0? 1: avgTime;
        v->memoryRequirement = avgMem; // avgMem==0? 1: avgMem;
        v->wchar = avgwchar; // avgwchar==0? 1: avgwchar;
        v->taskinputsize = avgtinps; // avgtinps==0? 1: avgtinps;

        minMem = std::min(minMem, avgMem);
        minTime = std::min(minTime, avgTime);
        minWchar = std::min(minWchar, avgwchar);
        mintt = std::min(mintt, avgtinps);
    }

    for (vertex_t* v = graphMemTopology->first_vertex; v; v = v->next) {
        if (v->memoryRequirement == 0) {
            v->time = minTime;
            v->memoryRequirement = minMem;
            v->wchar = minWchar;
            v->taskinputsize = mintt;
        }
    }
    retrieveEdgeWeights(graphMemTopology);
}

void retrieveEdgeWeights(const graph_t* graphMemTopology)
{
    const vertex_t* vertex = graphMemTopology->first_vertex;
    while (vertex != nullptr) {
        double totalOutput = 1;
        for (int j = 0; j < vertex->in_degree; j++) {
            const edge* incomingEdge = vertex->in_edges.at(j);
            const vertex_t* predecessor = incomingEdge->tail;
            totalOutput += predecessor->wchar;
        }
        for (int j = 0; j < vertex->in_degree; j++) {
            edge* incomingEdge = vertex->in_edges.at(j);
            const vertex_t* predecessor = incomingEdge->tail;
            incomingEdge->weight = (predecessor->wchar / totalOutput) * vertex->taskinputsize;
            incomingEdge->factorForRealExecution = 1;
        }
        vertex = vertex->next;
    }
}

}