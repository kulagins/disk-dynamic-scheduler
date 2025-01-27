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
#include <unordered_map>


enum ClusteringModes {
    treeDependent, staticClustering
};
using namespace std;

class Event;

class Processor  : public std::enable_shared_from_this<Processor> {
protected:
    double memorySize;
    double processorSpeed;
    vertex_t *assignedTask;


public:
    static auto comparePendingMemories(edge_t* a, edge_t*b) -> bool {
        if(a->weight==b->weight){
            if(a->head->id==b->head->id)
                return a->tail->id> b->tail->id;
            else return a->head->id>b->head->id;
        }
        else
            return a->weight>b->weight;
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

    string assignment;
    std::vector<std::weak_ptr<Event>> events; // Processor does not "own" the Events

    std::weak_ptr<Event> lastReadEvent;
    std::weak_ptr<Event> lastWriteEvent;
    std::weak_ptr<Event> lastComputeEvent;



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

    void setReadyTimeCompute(double newTime){
        cout<<"proc "<<this->id<<"now ready at "<<newTime<<endl;
        this->readyTimeCompute = newTime;
    }
    int getAssignedTaskId() const;

      vertex_t *getAssignedTask() const;

    void assignSubgraph(vertex_t *taskToBeAssigned);

    std::set<edge_t *, decltype(comparePendingMemories)*>::iterator delocateToDisk(edge_t* edge);
    void loadFromDisk(edge_t* edge);
    void loadFromNowhere(edge_t* edge);
    std::set<edge_t *, decltype(Processor::comparePendingMemories)*>::iterator removePendingMemory(edge_t * edgeToRemove){

        auto it = pendingMemories.find(edgeToRemove);
        auto iterator = it;
        if (it != pendingMemories.end()) {
            iterator = pendingMemories.erase(it);
        }
        else{
           throw new runtime_error("not found edge in pending");
        }
        availableMemory+= edgeToRemove->weight;
        assert(availableMemory< memorySize || abs(availableMemory- memorySize)<0.1);
        return iterator;
     }

    void addPendingMemory(edge_t * edge){
        auto it = pendingMemories.find(edge);
        if (it != pendingMemories.end()) {
            throw new runtime_error(" found edge in pending");
        }
        else{
            pendingMemories.emplace(edge);
        }
        availableMemory-= edge->weight;
        assert(availableMemory>= 0);
    }

    void addEvent(std::shared_ptr<Event> event) {
        events.push_back(event); // Add weak reference to avoid circular ownership
    }
    vector<weak_ptr<Event>>  getEvents(){
        return this->events;
    }

};

class Cluster {
protected:
    vector<edge_t *> filesOnDisk;
    int maxSpeed = -1;
public:
    std::unordered_map<int, std::shared_ptr<Processor>> processors;



    Cluster() {
        processors.clear();

    }

    Cluster(unsigned int clusterSize) {
        processors.reserve(clusterSize);
        //for (unsigned long i = 0; i < clusterSize; i++) {
     //       processors.at(i) = make_shared<Processor>();
      //  }



    }



public:

    void addProcessor(const shared_ptr<Processor>& p){
        if( processors[p->id]!=NULL){
            throw new runtime_error("non-unique processor id "+ to_string(p->id));
        }
        processors[p->id] = p;

    }
    void replaceProcessor(const shared_ptr<Processor>& p){
        processors[p->id] = p;
    }


    void removeProcessor(Processor * toErase){
        this->processors.erase(toErase->id);
    }

    std::unordered_map<int, std::shared_ptr<Processor>> getProcessors() {
        return this->processors;
    }

    [[nodiscard]] unsigned int getNumberProcessors() const {
        return this->processors.size();
    }



    void printProcessors() {

        for (const auto& [key, value] : this->processors) {
            cout << "Processor " << value->id<< "with memory " << value->getMemorySize() << ", speed " << value->getProcessorSpeed()
                 << " and busy? " << value->isBusy << "assigned " << (value->isBusy?value->getAssignedTaskId(): -1)
                << " ready time compute " << value->readyTimeCompute
                 << " ready time read " << value->readyTimeRead
                 << " ready time write " << value->readyTimeWrite
                 //<< " ready time write soft " << value->softReadyTimeWrite
                 //<< " avail memory " << value->availableMemory
                 << " pending in memory "<<value->pendingMemories.size()<<" pcs: ";

            for (const auto &item: value->pendingMemories){
                print_edge(item);
            }
            cout<< endl;
        }
    }




    shared_ptr<Processor>getMemBiggestFreeProcessor();
    shared_ptr<Processor>getFastestFreeProcessor();
    shared_ptr<Processor>getFastestProcessorFitting(double memReq);

    shared_ptr<Processor>getFirstFreeProcessorOrSmallest();

    shared_ptr<Processor> getProcessorById(int id){
        if(processors[id]==NULL){
            throw new runtime_error("Processor not found by id "+ to_string(id));
        }
        return processors[id];


    }

    shared_ptr<Processor> getOneProcessorByName(string name){
        for (const auto& [key, value] : this->processors) {
            if (value->name==name) return value;
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

#endif
