#ifndef cluster_h
#define cluster_h

#include <iostream>
#include <list>
#include <ostream>
#include <stdio.h>
#include <assert.h>
#include <forward_list>
#include <map>
#include <vector>

#include <limits>
#include <algorithm>
#include <set>
#include "graph.hpp"


enum ClusteringModes {
    treeDependent, staticClustering
};
using namespace std;


class Processor  : public std::enable_shared_from_this<Processor> {
protected:
    double memorySize;
    double processorSpeed;
    vertex_t *assignedTask;


public:
    static auto comparePendingMemories(edge_t* a, edge_t*b) -> bool {
        if(a->weight==b->weight){
            if(a->head->id==b->head->id)
                return a->tail->id< b->tail->id;
            else return a->head->id>b->head->id;
        }
        else
            return a->weight<b->weight;
    }
    int id;
    string name;
    bool isBusy;
    double readyTimeCompute;
    double readyTimeRead;
    double readyTimeWrite;
    double softReadyTimeWrite;

    double memoryOffloadingPenalty;

    double availableMemory;
    std::set<edge_t *, decltype(comparePendingMemories)*> pendingMemories{comparePendingMemories};

    double afterAvailableMemory;
    std::set<edge_t *, decltype(comparePendingMemories)*> afterPendingMemories{comparePendingMemories};

    double readSpeedDisk;
    double writeSpeedDisk;
    double peakMemConsumption=0;


    Processor() {
        this->memorySize = 0;
        this->processorSpeed = 1;
        isBusy = false;
        assignedTask = nullptr;
        id = -1;
        this->readyTimeCompute=0;
        this->readyTimeRead=0;
        this->readyTimeWrite=0;
        this->softReadyTimeWrite=0;

    }

    explicit Processor(double memorySize, int id=-1) {
        this->memorySize = memorySize;
        this->availableMemory = memorySize;
        this->processorSpeed = 1;
        isBusy = false;
        assignedTask = nullptr;
        this->id = id;
        this->readyTimeCompute=0;
        this->readyTimeRead=0;
        this->readyTimeWrite=0;
        this->softReadyTimeWrite=0;
    }

    Processor(double memorySize, double processorSpeed, int id=-1) {
        this->memorySize = memorySize;
        this->availableMemory = memorySize;
        this->processorSpeed = processorSpeed;
        isBusy = false;
        assignedTask = nullptr;
        this->id = id;
        this->readyTimeCompute=0;
        this->readyTimeRead=0;
        this->readyTimeWrite=0;
        this->softReadyTimeWrite=0;
    }

    Processor(const Processor &copy);

    //TODO impelement
    ~Processor() {
        //  if (assignedTask != nullptr) delete assignedTask;
        pendingMemories.clear();
    }

    double getMemorySize() const {
        return memorySize;
    }

    void setMemorySize(double memory) {
        this->memorySize = memory;
    }

    double getProcessorSpeed() const {
        return processorSpeed;
    }

    void setProcessorSpeed(double s) {
        this->processorSpeed=s;
    }

    double getAvailableMemory(){
        return this->availableMemory;
    }
    void setAvailableMemory(double mem){
        this->availableMemory = mem;
    }

    int getAssignedTaskId() const;

      vertex_t *getAssignedTask() const;

    void assignSubgraph(vertex_t *taskToBeAssigned);

    std::set<edge_t *, decltype(comparePendingMemories)*>::iterator delocateToDisk(edge_t* edge);
    void loadFromDisk(edge_t* edge);
    void loadFromNowhere(edge_t* edge);

};

class Cluster {
protected:

    static Cluster *fixedCluster;
    vector<edge_t *> filesOnDisk;
    int maxSpeed = -1;
public:
    vector<shared_ptr<Processor>> processors;


    Cluster() {
        processors.resize(0);

    }

    Cluster(unsigned int clusterSize) {
        processors.resize(clusterSize);
        for (unsigned long i = 0; i < clusterSize; i++) {
            processors.at(i) = make_shared<Processor>();
        }



    }

    Cluster(vector<unsigned int> *groupSizes, vector<double> *memories, vector<double> *speeds) {
        for (int i = 0; i < groupSizes->size(); i++) {
            unsigned int groupSize = groupSizes->at(i);
            for (unsigned int j = 0; j < groupSize; j++) {
                this->processors.push_back(make_shared<Processor>(memories->at(i), speeds->at(i)));
            }
        }

    }

  //  Cluster(const Cluster * copy) ;


public:

    void addProcessor(const shared_ptr<Processor>& p){
        if(std::find_if(processors.begin(), processors.end(),[p](const shared_ptr<Processor>& p1){
            return p->id==p1->id;
        }) != processors.end()){
            throw new runtime_error("non-unique processor id "+ to_string(p->id));
        }
        this->processors.emplace_back(p);
    }


    void removeProcessor(Processor * toErase){
        const vector<shared_ptr<Processor>>::iterator &iterator = std::find_if(this->processors.begin(), this->processors.end(),
                                                                     [toErase](const shared_ptr<Processor>& p) {
                                                                         return toErase->getMemorySize() ==
                                                                                p->getMemorySize() &&
                                                                                toErase->getProcessorSpeed() ==
                                                                                p->getProcessorSpeed() && !p->isBusy;
                                                                     });
        this->processors.erase(iterator);
        delete toErase;
    }

    vector<shared_ptr<Processor>> getProcessors() {
        return this->processors;
    }

    [[nodiscard]] unsigned int getNumberProcessors() const {
        return this->processors.size();
    }

    unsigned int getNumberFreeProcessors() {
        int res = count_if(this->processors.begin(), this->processors.end(),
                           [](const shared_ptr<Processor>& i) { return !i->isBusy; });
        return res;
    }


    void setMemorySizes(vector<double> &memories) {
        //  memoryHomogeneous = false;
        if (processors.size() != memories.size()) {
            processors.resize(memories.size());
            for (unsigned long i = 0; i < memories.size(); i++) {
                processors.at(i) = make_shared<Processor>(memories.at(i));
            }
        } else {
            for (unsigned long i = 0; i < memories.size(); i++) {
                processors.at(i)->setMemorySize(memories.at(i));
            }
        }

    }

    void printProcessors() {
        for (auto iter = this->processors.begin(); iter < processors.end(); iter++) {
            cout << "Processor " << (*iter)->id<< "with memory " << (*iter)->getMemorySize() << ", speed " << (*iter)->getProcessorSpeed()
                 << " and busy? " << (*iter)->isBusy << "assigned " << ((*iter)->isBusy?(*iter)->getAssignedTaskId(): -1)
                 << " ready time compute " << (*iter)->readyTimeCompute
                 << " ready time read " << (*iter)->readyTimeRead
                 << " ready time write " << (*iter)->readyTimeWrite
                 //<< " ready time write soft " << (*iter)->softReadyTimeWrite
                 //<< " avail memory " << (*iter)->availableMemory
                 << " pending in memory "<<(*iter)->pendingMemories.size()<<" pcs: ";

            for (const auto &item: (*iter)->pendingMemories){
                print_edge(item);
            }
            cout<< endl;
        }
    }


    static void setFixedCluster(Cluster *cluster) {
        Cluster::fixedCluster = cluster;
    }

    static Cluster *getFixedCluster() {
        return Cluster::fixedCluster;
    }
    int getMaxSpeed(){
        if(maxSpeed==-1){
            sortProcessorsByProcSpeed();
            maxSpeed = processors.at(0)->getProcessorSpeed();
        }
        return maxSpeed;
    }


    shared_ptr<Processor>getMemBiggestFreeProcessor();
    shared_ptr<Processor>getFastestFreeProcessor();
    shared_ptr<Processor>getFastestProcessorFitting(double memReq);

    shared_ptr<Processor>getFirstFreeProcessorOrSmallest();

    shared_ptr<Processor> getProcessorById(int id){
        for ( auto &item: getProcessors()){
            if (item->id==id) return item;
        }
        throw new runtime_error("Processor not found by id "+ to_string(id));
    }

    shared_ptr<Processor> getOneProcessorByName(string name){
        for ( auto &item: getProcessors()){
            if (item->name==name) return item;
        }
        throw new runtime_error("Processor not found by name "+ name);
    }

    bool hasFreeProcessor();

    void clean();

    shared_ptr<Processor>smallestFreeProcessorFitting(double requiredMem);


    void freeAllBusyProcessors();
    void sortProcessorsByMemSize();
    void sortProcessorsByProcSpeed();

    void printAssignment();


};

vector<edge_t *> getBiggestPendingEdge(shared_ptr<Processor>pj);

enum eventType{
    OnTaskStart,
    OnTaskFinish,
    OnReadStart,
    OnReadFinish,
    OnWriteStart,
    OnWriteFinish
};
class Event{
public:
    vertex_t* task;
    edge_t* edge;
    eventType type;
    Processor * processor;
    double expectedTimeFire;
    double actualTimeFire;
    vector<Event> predecessors, successors;
    bool isEviction;

};
#endif
