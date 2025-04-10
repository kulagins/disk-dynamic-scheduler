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
#include <functional>

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
    //std::unordered_map<string, std::weak_ptr<Event>> eventsOnProc;
    bool isKeptValid= true;


public:
    static auto comparePendingMemories(edge_t* a, edge_t*b) -> bool {
        //std::cout << "Comparing: " << a << " vs " << b << std::endl;
        if (!a || !b) {
            cout<<"a or b invalid in comparison on memories"<<endl;
            return false;  // Handle null pointers safely
        }
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

    double softReadyTimeWrite;

    double memoryOffloadingPenalty;
    vector<edge_t *> writingQueue;

protected:
    double availableMemory;
   // std::set<edge_t *, decltype(comparePendingMemories)*> pendingMemories{comparePendingMemories};
    std::set<edge_t*, std::function<bool(edge_t*, edge_t*)>> pendingMemories;

    double afterAvailableMemory;
   // std::set<edge_t *, decltype(comparePendingMemories)*> afterPendingMemories{comparePendingMemories};
    std::set<edge_t*, std::function<bool(edge_t*, edge_t*)>> afterPendingMemories;


public:
    double readSpeedDisk;
    double writeSpeedDisk;
    double peakMemConsumption=0;

    string assignment;

private:
    std::weak_ptr<Event> lastReadEvent;
    std::weak_ptr<Event> lastWriteEvent;
    std::weak_ptr<Event> lastComputeEvent;

    double readyTimeCompute;
    double readyTimeRead;
    double readyTimeWrite;

public:

    Processor() : pendingMemories(comparePendingMemories){
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

    explicit Processor(double memorySize, int id=-1): pendingMemories(comparePendingMemories) {
        this->memorySize = memorySize;
        this->availableMemory = memorySize;
        this->afterAvailableMemory = memorySize;
        this->processorSpeed = 1;
        isBusy = false;
        assignedTask = nullptr;
        this->id = id;
        this->readyTimeCompute=0;
        this->readyTimeRead=0;
        this->readyTimeWrite=0;
        this->softReadyTimeWrite=0;
    }

    Processor(double memorySize, double processorSpeed, int id=-1): pendingMemories(comparePendingMemories) {
        this->memorySize = memorySize;
        this->availableMemory = memorySize;
        this->afterAvailableMemory = memorySize;
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
        writingQueue.clear();
        pendingMemories.clear();
        afterPendingMemories.clear();
        assignedTask = nullptr; // just nullify borrowed pointer
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
        if(abs(availableMemory - memorySize) <0.01){
            availableMemory= memorySize;
        }
        assert(!isKeptValid || (availableMemory>=0 && availableMemory<= memorySize));
        return this->availableMemory;
    }
    void setAvailableMemory(double mem){
       // cout<<"set available memory of proc "<<this->id<<" to "<<mem<<endl;
        assert(mem>=0);
        if(mem>memorySize && isKeptValid){
            assert(abs(mem - memorySize)< 0.1);
        }

        this->availableMemory = mem;
    }

    void setReadyTimeCompute(double newTime){
       // cout<<"proc "<<this->id<<"now ready at "<<newTime<<endl;
        this->readyTimeCompute = newTime;
    }
    int getAssignedTaskId() const;

    void setIsKeptValid(bool is){
        this->isKeptValid= is;
    }

    bool getIsKeptValid(){
       return this->isKeptValid;
    }

      vertex_t *getAssignedTask() const;

    void assignSubgraph(vertex_t *taskToBeAssigned);

    std::set<edge_t *, decltype(comparePendingMemories)*>::iterator delocateToDisk(edge_t* edge, bool shouldUseImaginary);
    void loadFromDisk(edge_t* edge, bool shouldUseImaginary);
    void loadFromNowhere(edge_t* edge, bool shouldUseImaginary, double afterWhen);
    std::set<edge_t *, decltype(Processor::comparePendingMemories)*>::iterator delocateToDiskOptionally(edge_t* edge, bool shouldUseImaginary);

    std::set<edge_t *, decltype(Processor::comparePendingMemories)*>::iterator
    //bool
    removePendingMemory(edge_t * edgeToRemove){
       // cout<<"removing pending memory "<<buildEdgeName(edgeToRemove)<<" from proc "<<this->id<<endl;

        auto it = pendingMemories.find(edgeToRemove);
        if (it == pendingMemories.end() ) {
            if(isKeptValid){
                throw std::runtime_error("not found edge in pending "+ buildEdgeName(edgeToRemove));
            }
            else{
                return pendingMemories.end();
            }

        }

        auto nextIt = std::next(it);  // Get the next iterator before erasing
        pendingMemories.erase(it);  // Now erase safely

        availableMemory += edgeToRemove->weight;
        assert(!isKeptValid ||availableMemory < memorySize || std::abs(availableMemory - memorySize) < 0.1);

        return nextIt;  // Return the next iterator, which is safe
     }

    void addPendingMemory(edge_t * edge){
      //  cout<<"Add pending memory "<<buildEdgeName(edge)<<endl;
        if(isKeptValid) {
            if (!edge) {
                throw std::runtime_error("Edge is null!");
            }
            if (!edge->head || !edge->tail) {
                throw std::runtime_error("Edge has uninitialized fields!");
            }
            auto it = pendingMemories.find(edge);
            if (it != pendingMemories.end()) {
                throw runtime_error(" found edge in pending");
            }
        }
        pendingMemories.emplace(edge);
        availableMemory-= edge->weight;
        assert(!isKeptValid || availableMemory>= 0);
    }

    void addPendingMemoryAfter(edge_t * edge){
        auto it = afterPendingMemories.find(edge);
        if (it != afterPendingMemories.end()) {
            throw runtime_error(" found edge in pending");
        }
        else{
            afterPendingMemories.emplace(edge);
        }
        afterAvailableMemory-= edge->weight;
        if(afterAvailableMemory< 0){
            throw runtime_error("After avail mem <0");
        }
    }

    std::set<edge_t *, decltype(Processor::comparePendingMemories)*>::iterator
    //bool
    removePendingMemoryAfter(edge_t * edgeToRemove){

        auto it = afterPendingMemories.find(edgeToRemove);
        if (it == afterPendingMemories.end()) {
            throw std::runtime_error("not found edge in pending "+ buildEdgeName(edgeToRemove));
        }

        auto nextIt = std::next(it);  // Get the next iterator before erasing
        afterPendingMemories.erase(it);  // Now erase safely

        afterAvailableMemory += edgeToRemove->weight;
        assert(afterAvailableMemory < memorySize || std::abs(afterAvailableMemory - memorySize) < 0.1);

        return nextIt;  // Return the next iterator, which is safe
    }

    void addEvent(std::shared_ptr<Event> event);

   // std::unordered_map<string, std::weak_ptr<Event>>  getEvents(){
   //     return this->eventsOnProc;
   // }
    void updateFrom(const Processor& other);

    std::set<edge_t *,  std::function<bool(edge_t*, edge_t*)>> &getPendingMemories() {
        return  pendingMemories;
    }

    void setPendingMemories( set<edge_t *,    std::function<bool(edge_t*, edge_t*)>> &pendingMemories);
    void resetPendingMemories( ){
        this->pendingMemories.clear();
    }
    void resetAfterPendingMemories( ){
        this->afterPendingMemories.clear();
    }

    double getAfterAvailableMemory() const;

    void setAfterAvailableMemory(double d);

    std::set<edge_t *, std::function<bool(edge_t*, edge_t*)>> &getAfterPendingMemories(){
        return afterPendingMemories;
    }

    void setAfterPendingMemories( set<edge_t *,  std::function<bool(edge_t*, edge_t*)>> &memories);

    edge_t * getBiggestPendingEdgeThatIsNotIncomingOf(vertex_t* v){
        auto it = pendingMemories.begin();
        while(it!= pendingMemories.end()){
            if((*it)->head->name!=v->name){
                return *it;
            }
            else{
                it++;
            }
        }
        //throw runtime_error("No pending memories that are not incoming edges of task "+v->name);
        return nullptr;
    }

    void setLastWriteEvent(shared_ptr<Event> lwe);
    void setLastReadEvent(shared_ptr<Event> lwe);
    void setLastComputeEvent(shared_ptr<Event> lwe);

    weak_ptr<Event> getLastWriteEvent(){
        return this->lastWriteEvent;
    }
    weak_ptr<Event> getLastReadEvent(){
        return this->lastReadEvent;
    }
    weak_ptr<Event> getLastComputeEvent(){
        return this->lastComputeEvent;
    }
    double getReadyTimeWrite() ;
    double getReadyTimeRead() ;
    double getReadyTimeCompute();

    double getExpectedOrActualReadyTimeWrite() ;
    double getExpectedOrActualReadyTimeRead() ;
    double getExpectedOrActualReadyTimeCompute();


    void setReadyTimeWrite(double rtw){
        this->readyTimeWrite= rtw;
    }
    void setReadyTimeRead(double rtr){
        this->readyTimeRead= rtr;
    }



};

class Cluster {
protected:
    vector<edge_t *> filesOnDisk;
    int maxSpeed = -1;
    bool isKeptValid= true;
public:
    std::unordered_map<int, std::shared_ptr<Processor>> processors;

    ~Cluster(){
        filesOnDisk.resize(0);
        processors.clear();
    }

    Cluster() {
        processors.clear();

    }



public:

    void addProcessor(const shared_ptr<Processor>& p){
        if( processors[p->id]!=NULL){
            throw new runtime_error("non-unique processor id "+ to_string(p->id));
        }
        processors[p->id] = p;

    }


    std::unordered_map<int, std::shared_ptr<Processor>>& getProcessors() {
        return this->processors;
    }

    [[nodiscard]] unsigned int getNumberProcessors() const {
        return this->processors.size();
    }



    void printProcessors() {

        for (const auto& [key, value] : this->processors) {
            cout << "Processor " << value->id<< "with memory " << value->getMemorySize() << ", speed " << value->getProcessorSpeed()
                 << " and busy? " << value->isBusy << "assigned " << (value->isBusy?value->getAssignedTaskId(): -1)
                << " ready time compute " << value->getReadyTimeCompute()
                 << " ready time read " << value->getReadyTimeRead()
                 << " ready time write " << value->getReadyTimeWrite()
                 //<< " ready time write soft " << value->softReadyTimeWrite
                 //<< " avail memory " << value->availableMemory
                 << " pending in memory "<<value->getPendingMemories().size()<<" pcs: ";

            for (const auto &item: value->getPendingMemories()){
                print_edge(item);
            }
            cout<< endl;
        }
    }

    void printProcessorsEvents() ;

    void mayBecomeInvalid(){
        this->isKeptValid=false;
        for (auto &item: processors){
            item.second->setIsKeptValid(false);
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
