
#include <iostream>
#include <iterator>
#include <sstream>

#include <fonda_scheduler/io/graphWeightsBuilder.hpp>
#include "fonda_scheduler/common.hpp"
#include "../extlibs/csv2/single_include/csv2/csv2.hpp"

namespace Fonda {

    Cluster * buildClusterFromCsv(int memoryMultiplicator, double readWritePenalty, double offloadPenalty, int speedMultiplicator){
        csv2::Reader<csv2::delimiter<','>,
                csv2::quote_character<'"'>,
                csv2::first_row_is_header<true>,
                csv2::trim_policy::trim_whitespace> csv;

        Cluster * cluster = new Cluster();
        if (csv.mmap("../input/machines.csv")) {
            int id=0;
            for (const auto row: csv) {
                std::vector<std::string> row_data;
                shared_ptr<Processor> p = make_shared<Processor>();

                int rl = row.length();
                if(rl>0) {
                    p->id = id;
                    cluster->addProcessor(p);
                    id++;
                }



                int cell_cntr=0;
                for (const auto& cell : row) {
                    std::string cell_value;
                    cell.read_value(cell_value);
                    row_data.push_back(cell_value);


                    if(cell_cntr==0){
                        p->name=cell_value;
                    }
                    if(cell_cntr==2){
                        p->setProcessorSpeed(stod(cell_value)*speedMultiplicator);
                    }
                    if(cell_cntr==3){
                        p->setMemorySize(stod(cell_value)*memoryMultiplicator);
                        p->availableMemory= p->getMemorySize();
                    }
                    if(cell_cntr==8){
                        p->readSpeedDisk=stod(cell_value)*readWritePenalty;
                    }
                    if(cell_cntr==9){
                        p->writeSpeedDisk=stod(cell_value)*readWritePenalty;
                        p->memoryOffloadingPenalty = stod(cell_value)* offloadPenalty;
                    }
                    //TODO read/write iops
                    ++cell_cntr;
                }


            }
        }

        return cluster;

    }

    Cluster *buildClusterFromJson(nlohmann::json query) {
        Cluster *cluster = new Cluster();
        int id = 0;
        for (auto element: query["cluster"]["machines"]) {
            shared_ptr<Processor>p;

            if(   element.contains("speed")) {
                p = make_shared<Processor>(element["memory"], element["speed"], id);
            }
            else p =make_shared<Processor>(element["memory"], 1, id);
            id++;
            cluster->addProcessor(p);

        }

        Cluster::setFixedCluster(cluster);
        return cluster;
    }

    void fillGraphWeightsFromExternalSource(graph_t *graphMemTopology, std::unordered_map<std::string, std::vector<std::vector<std::string>>>
            workflow_rows, const string& workflow_name, Cluster * cluster, int memShorteningDivision) {

        double minMem= std::numeric_limits<double>::max(), minTime =  std::numeric_limits<double>::max(), minWchar= std::numeric_limits<double>::max(),
        mintt = std::numeric_limits<double>::max();
        for(vertex_t *v=graphMemTopology->first_vertex; v; v=v->next) {
            v->bottom_level=-1;
            string lowercase_name = v->name;
            std::regex pattern("_\\d+");
            lowercase_name = std::regex_replace(lowercase_name, pattern, "");
           string workflow_name1 = std::regex_replace(workflow_name, pattern, "");
            transform(lowercase_name.begin(),
                      lowercase_name.end(),
                      lowercase_name.begin(),
                      [](unsigned char c) {
                          return tolower(
                                  c);
                      });

           string nameToSearch =  workflow_name1+" "+lowercase_name;
            if (workflow_rows.find(nameToSearch) != workflow_rows.end()) {
                double avgMem=0, avgTime = 0, avgwchar=0, avgtinps=0;


                for (const auto& row : workflow_rows[nameToSearch]) {
                    int col_idx = 0;
                    string proc_name;
                    for (const auto& cell : row) {

                        if (col_idx == 3) {
                           proc_name=cell;
                        }
                        if (col_idx == 4) {
                            //time
                            double procSpeed = cluster->getOneProcessorByName(proc_name)->getProcessorSpeed();
                            avgTime += stod(cell)* procSpeed;
                        }
                        if (col_idx == 5) {
                            //memory
                           avgMem+=stod(cell)/memShorteningDivision;
                        }
                        if (col_idx == 6) {
                            //memory
                            avgwchar+=stod(cell)/(memShorteningDivision*100);
                        }
                        if (col_idx == 7) {
                            //memory
                            avgtinps+=stod(cell)/(memShorteningDivision*100);
                        }
                        col_idx++;
                    }
                }
                if(workflow_rows[nameToSearch].empty()){
                    cout<<"<<<<<"<<endl;
                }
                double numOfMsrs = workflow_rows[nameToSearch].size();
                avgMem/= numOfMsrs;
                avgTime/= numOfMsrs;
                avgwchar/= numOfMsrs;
                avgtinps/= numOfMsrs;

                v->time=  avgTime;//avgTime==0? 1: avgTime;
                v->memoryRequirement= avgMem; //avgMem==0? 1: avgMem;
                v->wchar= avgwchar;//avgwchar==0? 1: avgwchar;
                v->taskinputsize= avgtinps; //avgtinps==0? 1: avgtinps;

                minMem= min(minMem, avgMem);
                minTime = min(minTime, avgTime);
                minWchar = min(minWchar, avgwchar);
                mintt = min(mintt,avgtinps);
            }
            else{
                //cout<<"Nothing found for "<<v->name<<endl;
            }
        }

        for(vertex_t *v=graphMemTopology->first_vertex; v; v=v->next) {
            if(v->memoryRequirement==0){
                v->time=  minTime;
                v->memoryRequirement= minMem;
                v->wchar= minWchar;
                v->taskinputsize= mintt;
            }
        }
        retrieveEdgeWeights(graphMemTopology);
    }

   void
    retrieveEdgeWeights(graph_t *graphMemTopology) {

       vertex_t *vertex = graphMemTopology->first_vertex;
       while (vertex != NULL) {
           double totalOutput =1;
           for (int j = 0; j < vertex->in_degree; j++) {
               edge *incomingEdge = vertex->in_edges[j];
               vertex_t *predecessor = incomingEdge->tail;
               totalOutput += predecessor->wchar;
           }
           for (int j = 0; j < vertex->in_degree; j++){
               edge *incomingEdge = vertex->in_edges[j];
               vertex_t *predecessor = incomingEdge->tail;
               incomingEdge->weight =(predecessor->wchar/ totalOutput) * vertex->taskinputsize;
           }
           vertex = vertex->next;
       }

    }



}