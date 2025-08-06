#include <iostream>
#include <set>
#include <vector>
#include <filesystem>
#include "../extlibs/memdag/src/graph.hpp"
#include "../include/fonda_scheduler/SchedulerHeader.hpp"
#include "../include/fonda_scheduler/io/graphWeightsBuilder.hpp"
#include "../extlibs/csv/single_include/csv2/csv2.hpp"
#include "fonda_scheduler/DynamicSchedulerHeader.hpp"
#include <iomanip>

#include <chrono>
#include <cstring>
#include <csignal>


int currentAlgoNum = 0;

/*
 *
 *  Call: memoryMultiplicator speedMultiplicator readWritePenalty offloadPenalty,workflow, inputSize, algorithmNumber, isBaseline, root directory, machines file, number of deviation function, yes/no for preemptive writes
 *  1000000, 100, 1, 0.001, true ../ machines.csv 1
 *
 * algos with  memory awareness: 1 - HEFT-BL, 2- HEFT-BL, 3- HEFT-MM
 * HEFT (no memory awareness) : yes at isBaseline, algoNum is irrelevant then
 *  deviations :  1 - normal deviation function around historical value with 10% deviation
 *  2 - normal deviation function around historical value with 50% deviation
 *  3 - no deviation
 *  4 - everything x2
 *  5 - 30% deviation
 */
//1 1 0.1 0.01 debug 10 1 no ../ machines_debug.csv 3 -> gives evictions
//1000000 100 100 0.1 chipseq_200 41366257414 1 yes ../ machines.csv 3
//1000000 100 1 0.001 methylseq_200 110641579976 1 yes ../ machines.csv
//100000000 100 1 1 methylseq_2000 110641579976 1 yes ../ machines.csv
//1000000 100 1 1 bacass 3637252230 1 yes ../
//100000000 100 1 1 chipseq 3793245764 1 yes ../ machines.csv
//1 1 1 1 debug 10 1 yes ../ machines_debug.csv
//1000000 100 1 0.001 chipseq_1000 3793245764 1 no ../ machines.csv -> für beide gültig
//100000000 100 1 1 chipseq_2000 3793245764 1 yes ../ machines.csv
//1000000 100 1 0.001 eager_2000 25705994498 1 no ../ machines.csv
//100000000 100 1 0.001 eager 8330435694 1 no ../ machines.csv 3
int main(int argc, char *argv[]) {

 //   for (int i = 0; i < argc; ++i) {
  //      std::cout << argv[i] << " ";
   // }
   // cout<<endl;

    auto start = std::chrono::system_clock::now();
    string workflowName = argv[5];
    workflowName = trimQuotes(workflowName);
    long inputSize= stol(argv[6]);
    int algoNumber = std::stoi(argv[7]);
    cout << "algo_nr " << algoNumber << " " <<workflowName<<" "<<"input_size "<<inputSize<<" ";

    int memoryMultiplicator = stoi(argv[1]), speedMultiplicator = stoi(argv[2]);
    double readWritePenalty= stod(argv[3]), offloadPenalty= stod(argv[4]);
    bool isBaseline = (std::string(argv[8]) == "yes");
    string dotPrefix= argv[9];
    string machinesFile = (argc<10)?"input/machines.csv" : std::string("input/") + argv[10];
    int deviationVariant = stoi(argv[11]);
    bool usePreemptiveWrites = argc< 13 || std::string(argv[12]) =="yes";
    //1000000, 100, 1, 0.001
    csv2::Reader<csv2::delimiter<','>,
            csv2::quote_character<'"'>,
            csv2::first_row_is_header<true>,
            csv2::trim_policy::trim_whitespace> csv;

    std::unordered_map<std::string, std::vector<std::vector<std::string>>> workflow_rows;
    string tracesFileName = dotPrefix+ "input/traces.csv";
    if (csv.mmap(tracesFileName)) {
        for (const auto row: csv) {
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
    }


    imaginedCluster = Fonda::buildClusterFromCsv(dotPrefix +machinesFile, memoryMultiplicator,readWritePenalty, offloadPenalty, speedMultiplicator);

    actualCluster = Fonda::buildClusterFromCsv(dotPrefix +machinesFile, memoryMultiplicator,readWritePenalty, offloadPenalty, speedMultiplicator);


    double biggestMem = imaginedCluster->getMemBiggestFreeProcessor()->getMemorySize();

    string filename;
    if(workflowName.rfind("/home", 0) == 0 || workflowName.rfind("/work", 0) == 0){
        filename = workflowName.substr(0, workflowName.find("//")+1) + workflowName.substr(workflowName.find("//")+2, workflowName.size());

    }
    else{
        filename= dotPrefix+"input/";
        //string suffix = "00";
      //  bool isGenerated = workflowName.substr(workflowName.size() - suffix.size()) == suffix;
       // if (isGenerated) {
            filename += "generated/";//+filename;
      //  }
        filename += workflowName;

        size_t pos = filename.find(".dot");
        if(pos == std::string::npos){
            filename += ".dot";
        }

    }

    graph_t * graphMemTopology = read_dot_graph(filename.c_str(), NULL, NULL, NULL);
    checkForZeroMemories(graphMemTopology);

    currentAlgoNum = algoNumber;
    unsigned long i1 = workflowName.find("//");
    workflowName = i1 == string::npos? workflowName :
                   workflowName.substr(i1 + 2, workflowName.size());
    unsigned long n4 = workflowName.find('_');
    workflowName = workflowName.substr(0, n4);

    //10, 100                                                               memShorteningDivision, ioShorteningCoef
    Fonda::fillGraphWeightsFromExternalSource(graphMemTopology, workflow_rows, workflowName, inputSize, imaginedCluster, 1, 10);
    //print_graph_to_cout(graphMemTopology);

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
                    throw std::runtime_error("Memory requirement of vertex " + std::string(pv->name) + " exceeds the biggest memory available in the cluster. Out Memory requirement is "+ to_string(outMemoryRequirement(pv)));
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
                    throw std::runtime_error("Memory requirement of vertex " + std::string(pv->name) + " exceeds the biggest memory available in the cluster. In Memory requirement is "+ to_string(inMemoryRequirement(pv)));
                }
            }
        }
        pv = pv->next;
    }

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
   // std::cout << " duration_of_prep " << elapsed_seconds.count()<<" ";// << endl;

    vector<Assignment *> assignments;
    cout<<std::setprecision(15);


    cluster = Fonda::buildClusterFromCsv(dotPrefix +machinesFile, memoryMultiplicator,readWritePenalty, offloadPenalty, speedMultiplicator);
    assert(cluster->getProcessors().at(0)->getMemorySize()== actualCluster->getProcessors().at(0)->getMemorySize());
    assert(cluster->getProcessors().at(1)->getMemorySize()== actualCluster->getProcessors().at(1)->getMemorySize());
    assert(cluster->getProcessors().at(2)->getMemorySize()== actualCluster->getProcessors().at(2)->getMemorySize());
    assert(cluster->getProcessors().at(3)->getMemorySize()== actualCluster->getProcessors().at(3)->getMemorySize());

    double runtime =0;

    double d = new_heuristic_dynamic(graphMemTopology, cluster, algoNumber, isBaseline, deviationVariant, usePreemptiveWrites, runtime);

    events.deleteAll();
    std::cout << " duration_of_algorithm " << runtime<<" ";// << endl;
    cout<<"makespan_1 "<<d<<"\t";


    delete cluster;
    cluster = Fonda::buildClusterFromCsv(dotPrefix +machinesFile, memoryMultiplicator,readWritePenalty, offloadPenalty, speedMultiplicator);

    clearGraph(graphMemTopology);
    start = std::chrono::system_clock::now();
    double runtimeStatic=0;
    d = new_heuristic(graphMemTopology,  currentAlgoNum, isBaseline, runtimeStatic);

    std::cout << " duration_of_algorithm " << runtimeStatic<<" ";// << endl;
    cout<<"makespan_2 "<<d<<endl;


    delete graphMemTopology;
    delete cluster;
    delete imaginedCluster;
    delete actualCluster;
}

