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


class Processor {
protected:
    double memorySize;
    double processorSpeed;
    vertex_t *assignedTask;
    int assignedLeaderId;
    static auto comparePendingMemories(edge_t* a, edge_t*b) -> bool {
        if(a->weight==b->weight){
            if(a->head->id==b->head->id)
                return a->tail->id< b->tail->id;
            else return a->head->id>b->head->id;
        }
        else
        return a->weight<b->weight;
    }

public:
    int id;
    bool isBusy;
    bool visited;
    double communicationBuffer;
    double readyTime;
    double availableMemory;
    double availableBuffer;
    std::set<edge_t *, decltype(comparePendingMemories)*> pendingMemories{comparePendingMemories};
    std::set<edge_t*> pendingInBuffer;
    double peakMemConsumption=0;


    Processor() {
        this->memorySize = 0;
        this->processorSpeed = 1;
        isBusy = false;
        assignedTask = nullptr;
        id = -1;
        this->communicationBuffer =0;
        this->readyTime=0;
        this->availableBuffer =0;

    }

    explicit Processor(double memorySize, int id=-1) {
        this->memorySize = memorySize;
        this->availableMemory = memorySize;
        this->processorSpeed = 1;
        isBusy = false;
        assignedTask = nullptr;
        this->id = id;
        this->communicationBuffer =memorySize*10;
        this->readyTime=0;
        this->availableBuffer =communicationBuffer;
    }

    Processor(double memorySize, double processorSpeed, int id=-1) {
        this->memorySize = memorySize;
        this->availableMemory = memorySize;
        this->processorSpeed = processorSpeed;
        isBusy = false;
        assignedTask = nullptr;
        this->id = id;
        this->communicationBuffer =memorySize*10;
        this->readyTime=0;
        this->availableBuffer =communicationBuffer;
    }

    Processor(const Processor * copy);

    //TODO impelement
    ~Processor() {
        //  if (assignedTask != nullptr) delete assignedTask;
        pendingMemories.clear();
        pendingInBuffer.clear();
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

    double getAvailableMemory(){
        return this->availableMemory;
    }
    void setAvailableMemory(double mem){
        this->availableMemory = mem;
    }

    int getAssignedTaskId() const;

      vertex_t *getAssignedTask() const;

    void assignSubgraph(vertex_t *taskToBeAssigned);


};

class Cluster {
protected:
    vector<Processor *> processors;
    vector<vector<double>> bandwidths;
    static Cluster *fixedCluster;
    int maxSpeed = -1;
public:
    vector<vector<double>> readyTimesBuffers;

    Cluster() {
        processors.resize(0);
        bandwidths.resize(0);
        for (unsigned long i = 0; i < bandwidths.size(); i++) {
            bandwidths.at(i).resize(0, 0);
        }

        for (unsigned long i = 0; i < readyTimesBuffers.size(); i++) {
            readyTimesBuffers.at(i).resize(0, 0);
        }

    }

    Cluster(unsigned int clusterSize) {
        processors.resize(clusterSize);
        for (unsigned long i = 0; i < clusterSize; i++) {
            processors.at(i) = new Processor();
        }
        initHomogeneousBandwidth(clusterSize);
        for (unsigned long i = 0; i < readyTimesBuffers.size(); i++) {
            readyTimesBuffers.at(i).resize(0, 0);
        }

    }

    Cluster(vector<unsigned int> *groupSizes, vector<double> *memories, vector<double> *speeds) {
        for (int i = 0; i < groupSizes->size(); i++) {
            unsigned int groupSize = groupSizes->at(i);
            for (unsigned int j = 0; j < groupSize; j++) {
                this->processors.push_back(new Processor(memories->at(i), speeds->at(i)));
            }
        }
        initHomogeneousBandwidth(memories->size(), 1);
    }

    Cluster(const Cluster * copy) ;


    ~Cluster() {
        bandwidths.resize(0);
        readyTimesBuffers.resize(0);
        for (unsigned long i = 0; i < processors.size(); i++) {
            delete processors.at(i);
        }
    }

public:

    void addProcessor(Processor * p){
        if(std::find_if(processors.begin(), processors.end(),[p](Processor* p1){
            return p->id==p1->id;
        }) != processors.end()){
            throw new runtime_error("non-unique processor id "+ to_string(p->id));
        }
        this->processors.emplace_back(p);
        resizeReadyTimesBuffers(this->processors.size());
    }

    void resizeReadyTimesBuffers(int newsize){
        auto buf = readyTimesBuffers;
        for (unsigned long i = 0; i < readyTimesBuffers.size(); i++) {
            readyTimesBuffers.at(i).resize(0, 0);
        }

        readyTimesBuffers.resize(newsize);
        for (unsigned long i = 0; i < readyTimesBuffers.size(); i++) {
            readyTimesBuffers.at(i).resize(newsize, 0);
        }

        for(int a=0; a<buf.size(); a++){
            for(int b=0; b<buf[a].size();b++){
                readyTimesBuffers[a][b] = buf[a][b];
            }
        }

    }

    void removeProcessor(Processor * toErase){
        const vector<Processor *>::iterator &iterator = std::find_if(this->processors.begin(), this->processors.end(),
                                                                     [toErase](Processor *p) {
                                                                         return toErase->getMemorySize() ==
                                                                                p->getMemorySize() &&
                                                                                toErase->getProcessorSpeed() ==
                                                                                p->getProcessorSpeed() && !p->isBusy;
                                                                     });
        this->processors.erase(iterator);
        delete toErase;
    }

    vector<Processor *> getProcessors() const{
        return this->processors;
    }

    unsigned int getNumberProcessors() const {
        return this->processors.size();
    }

    unsigned int getNumberFreeProcessors() {
        int res = count_if(this->processors.begin(), this->processors.end(),
                           [](const Processor *i) { return !i->isBusy; });
        return res;
    }


    void initHomogeneousBandwidth(int bandwidthsNumber, double bandwidth = 1) {
        bandwidths.resize(bandwidthsNumber);
        setHomogeneousBandwidth(bandwidth);
    }

    void setHomogeneousBandwidth(double bandwidth) {
        for (unsigned long i = 0; i < bandwidths.size(); i++) {
            //TODO init only upper half
            bandwidths.at(i).resize(bandwidths.size(), bandwidth);
            for (unsigned long j = 0; j < bandwidths.size(); j++) {
                bandwidths.at(i).at(j) = bandwidth;
            }
        }
    }

    double getBandwidth() const{
        return this->bandwidths.at(0).at(0);
    }

    void setMemorySizes(vector<double> &memories) {
        //  memoryHomogeneous = false;
        if (processors.size() != memories.size()) {
            processors.resize(memories.size());
            for (unsigned long i = 0; i < memories.size(); i++) {
                processors.at(i) = new Processor(memories.at(i));
            }
        } else {
            for (unsigned long i = 0; i < memories.size(); i++) {
                processors.at(i)->setMemorySize(memories.at(i));
            }
        }

    }

    void printProcessors() {
        for (vector<Processor *>::iterator iter = this->processors.begin(); iter < processors.end(); iter++) {
            cout << "Processor with memory " << (*iter)->getMemorySize() << ", speed " << (*iter)->getProcessorSpeed()
                 << " and busy? " << (*iter)->isBusy << "assigned " << ((*iter)->isBusy?(*iter)->getAssignedTaskId(): -1)
                 << " ready time "<<(*iter)->readyTime<<" avail memory "<<(*iter)->availableMemory<<
                 endl;
        }
    }

    string getPrettyClusterString() {
        string out = "Cluster:\n";
        int numProcessors = this->processors.size();
        out += "#Nodes: " + to_string(numProcessors) + ", ";
       //TODO
        // out += "MinMemory: " + to_string(this->getLastProcessorMem()) + ", ";
        out += "MaxMemory: " + to_string(this->getProcessors().at(0)->getMemorySize()) + ", ";
        out += "CumulativeMemory: " + to_string(this->getCumulativeMemory());

        return out;
    }

    long getCumulativeMemory() {
        long sum = 0;
        for (Processor *proc: (this->processors)) {
            sum += proc->getMemorySize();
        }
        return sum;
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


    Processor *getMemBiggestFreeProcessor();
    Processor *getFastestFreeProcessor();
    Processor *getFastestProcessorFitting(double memReq);

    Processor *getFirstFreeProcessorOrSmallest();

    Processor * getProcessorById(int id){
        for ( auto &item: getProcessors()){
            if (item->id==id) return item;
        }
        throw new runtime_error("Processor not found by id "+ to_string(id));
    }

    bool hasFreeProcessor();

    void clean();

    Processor *smallestFreeProcessorFitting(double requiredMem);


    void freeAllBusyProcessors();
    void sortProcessorsByMemSize();
    void sortProcessorsByProcSpeed();

    void printAssignment();



};


#endif
