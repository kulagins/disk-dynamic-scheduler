#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <vector>
#include <map>
#include <filesystem>

#include "../extlibs/memdag/src/graph.hpp"
#include "../include/fonda_scheduler/dynSched.hpp"
#include "../include/fonda_scheduler/io/graphWeightsBuilder.hpp"
#include "../extlibs/csv2/single_include/csv2/csv2.hpp"

#include <chrono>
#include <cstring>
#include <csignal>



graph_t *currentWorkflow = NULL;
string currentName;
Cluster *currentCluster;
vector<Assignment *> currentAssignment;

double lastTimestamp = 0;
int currentAlgoNum = 0;
int updateCounter=0;
int delayCnt=0;
double lastAvgMem;

vector<Assignment *> currentAssignmentWithNoRecalculation;
bool isNoRecalculationStillValid= true;

std::shared_ptr<Http::Endpoint> endpoint;


int main(int argc, char *argv[]) {

    csv2::Reader<csv2::delimiter<','>,
            csv2::quote_character<'"'>,
            csv2::first_row_is_header<true>,
            csv2::trim_policy::trim_whitespace> csv;

    std::unordered_map<std::string, std::vector<std::vector<std::string>>> workflow_rows;


    if (csv.mmap("../input/traces.csv")) {
        for (const auto row: csv) {
            std::vector<std::string> row_data;
            std::string task_name, workflow_name;

            int col_idx = 0;
            for (const auto& cell : row) {
                std::string cell_value;
                cell.read_value(cell_value);
                row_data.push_back(cell_value);

                if (col_idx == 0) {
                    workflow_name = cell_value;
                }
                // Assuming the workflow name is in the first column (index 0)
                if (col_idx == 2) {
                    task_name = cell_value;
                }
                ++col_idx;
            }

            // Store row in the map under the workflow name
            workflow_rows[workflow_name+" "+task_name].push_back(row_data);
        }
    }

  /*  std::string target_task = "atacseq";  // Replace with your target workflow name
    if (workflow_rows.find(target_task) != workflow_rows.end()) {
        std::cout << "Rows for " << target_task << ":" << std::endl;
        for (const auto& row : workflow_rows[target_task]) {
            for (const auto& cell : row) {
                std::cout << cell << " ";
            }
            std::cout << std::endl;
        }
    } */

    Cluster * cluster = Fonda::buildClusterFromCsv(1000000, 1, 0.001);



    string workflowName = argv[1];
    workflowName = trimQuotes(workflowName);
    currentName = workflowName;
    int algoNumber = std::stoi(argv[2]);
    cout << "new, algo " << algoNumber << " " <<currentName<<" ";

    string filename = "../input/";
    string suffix = "00";
    if (workflowName.substr(workflowName.size() - suffix.size()) == suffix) {
        filename += "generated/";//+filename;
    }
    filename += workflowName;

    filename += workflowName.substr(workflowName.size() - suffix.size()) == suffix ? ".dot" : "_sparse.dot";
    graph_t * graphMemTopology = read_dot_graph(filename.c_str(), NULL, NULL, NULL);
    checkForZeroMemories(graphMemTopology);

    currentAlgoNum = algoNumber;
    Fonda::fillGraphWeightsFromExternalSource(graphMemTopology, workflow_rows, workflowName, cluster, 10);


    vertex_t *pv = graphMemTopology->first_vertex;
    while(pv!=NULL){
        if(peakMemoryRequirementOfVertex(pv)> pv->memoryRequirement){
            pv->memoryRequirement=peakMemoryRequirementOfVertex(pv)+1000;
            //cout<<"peak "<<peakMemoryRequirementOfVertex(pv)<<endl;
        }
        pv= pv->next;
    }
    // cluster->printProcessors();


    vector<Assignment *> assignments;
    cout<<std::setprecision(15);
    double d = new_heuristic(graphMemTopology, cluster, true);
    cout<<"makespan "<<d<<endl;
}

