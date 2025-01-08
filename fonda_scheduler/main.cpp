#include <iostream>
#include <set>
#include <vector>
#include <filesystem>
#include "../extlibs/memdag/src/graph.hpp"
#include "../include/fonda_scheduler/dynSched.hpp"
#include "../include/fonda_scheduler/io/graphWeightsBuilder.hpp"
#include "../extlibs/csv2/single_include/csv2/csv2.hpp"
#include <iomanip>

#include <chrono>
#include <cstring>
#include <csignal>


int currentAlgoNum = 0;

/*
 *
 *  Call: workflow, inputSIze, memoryMultiplicator speedMultiplicator readWritePenalty offloadPenalty, isBaseline
 *  1000000, 100, 1, 0.001, true ../
 *
 */

int main(int argc, char *argv[]) {

    for (int i = 0; i < argc; ++i) {
        std::cout << argv[i] << " ";
    }
    cout<<endl;

    auto start = std::chrono::system_clock::now();
    string workflowName = argv[5];
    workflowName = trimQuotes(workflowName);
    long inputSize= stol(argv[6]);
    int algoNumber = std::stoi(argv[7]);
    cout << "new, algo " << algoNumber << " " <<workflowName<<" ";

    int memoryMultiplicator = stoi(argv[1]), speedMultiplicator = stoi(argv[2]);
    double readWritePenalty= stod(argv[3]), offloadPenalty= stod(argv[4]);
    bool isBaseline = (std::string(argv[8]) == "yes");
    string dotPrefix= argv[9];
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

    Cluster * cluster = Fonda::buildClusterFromCsv(dotPrefix +"input/machines.csv", memoryMultiplicator,readWritePenalty, offloadPenalty, speedMultiplicator);
    double biggestMem = cluster->getMemBiggestFreeProcessor()->getMemorySize();
    cout<<workflowName<<endl;
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
    cout<<filename<<endl;
    graph_t * graphMemTopology = read_dot_graph(filename.c_str(), NULL, NULL, NULL);
    checkForZeroMemories(graphMemTopology);

    currentAlgoNum = algoNumber;
    unsigned long i1 = workflowName.find("//");
    workflowName = i1 == string::npos? workflowName :
                   workflowName.substr(i1 + 2, workflowName.size());
    unsigned long n4 = workflowName.find('_');
    workflowName = workflowName.substr(0, n4);


    Fonda::fillGraphWeightsFromExternalSource(graphMemTopology, workflow_rows, workflowName, inputSize, cluster, 10);


    vertex_t *pv = graphMemTopology->first_vertex;
    while(pv!= nullptr){
        if(peakMemoryRequirementOfVertex(pv)> pv->memoryRequirement){
            pv->memoryRequirement=peakMemoryRequirementOfVertex(pv)+1000;
            cout<<"peak of "<< pv->name<<" "<<peakMemoryRequirementOfVertex(pv)<<endl;
        }
        if(outMemoryRequirement(pv)> biggestMem){
            cout<<"WILL BE INVALID "<< outMemoryRequirement(pv)<<" vs "<<biggestMem<< endl;
            return 0;
        }
        pv= pv->next;
    }
    // cluster->printProcessors();

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << " duration_of_prep " << elapsed_seconds.count()<<" ";// << endl;

    start = std::chrono::system_clock::now();
    vector<Assignment *> assignments;
    cout<<std::setprecision(15);
    double d = new_heuristic(graphMemTopology, cluster, currentAlgoNum, isBaseline);

     end = std::chrono::system_clock::now();
     elapsed_seconds = end - start;
    std::cout << " duration_of_algorithm " << elapsed_seconds.count()<<" ";// << endl;
    cout<<"makespan "<<d<<endl;

    //for (const auto &item: cluster->getProcessors()){
   //     cout<< item.second->id<<": "<<item.second->assignment<<endl;
   // }

    delete graphMemTopology;
    delete cluster;
}

