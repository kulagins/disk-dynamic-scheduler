#include "cluster.hpp"
#include <numeric>

using namespace std;

shared_ptr<Processor>Cluster::getMemBiggestFreeProcessor() {

    shared_ptr<Processor> largestProcessor = nullptr;
    double largestMemory = 0;

    for (auto& [id, processor] : processors) {
        if (processor->getMemorySize() > largestMemory) {
            largestMemory = processor->getMemorySize();
            largestProcessor = processor;
        }
    }

    return largestProcessor;

}

shared_ptr<Processor> Cluster::getFastestProcessorFitting(double memReq) {

    shared_ptr<Processor> bestProcessor = nullptr;

    size_t fastestSpeed = 0;

    for (auto& [id, processor] : processors) {
        if (processor->getMemorySize() > memReq && processor->getProcessorSpeed()> fastestSpeed && !processor->isBusy) {
            fastestSpeed = processor->getProcessorSpeed();
            bestProcessor = processor;
        }
    }

    return bestProcessor;

}

shared_ptr<Processor>Cluster::getFastestFreeProcessor() {

    shared_ptr<Processor> fastestProcessor = nullptr;
    size_t bestSpeed = 0;

    for (auto& [id, processor] : processors) {
        if (processor->getProcessorSpeed() > bestSpeed && !processor->isBusy) {
            bestSpeed = processor->getProcessorSpeed();
            fastestProcessor = processor;
        }
    }
    if(fastestProcessor==nullptr){
        throw;
    }
    return fastestProcessor;
}



void Processor::assignSubgraph(vertex_t *taskToBeAssigned) {
    if (taskToBeAssigned != NULL) {
        this->assignedTask = taskToBeAssigned;
        //TODO
        //taskToBeAssigned->assignedProcessor= this;
       taskToBeAssigned->assignedProcessorId =  this->id;
        this->isBusy = true;
    } else{
        this->assignedTask = NULL;
        this->isBusy = false;
    }
}

//std::set<edge_t *, decltype(comparePendingMemories)*>::iterator
//unsigned long
std::set<edge_t *, decltype(Processor::comparePendingMemories)*>::iterator Processor::delocateToDisk(edge_t* edge, bool real) {
  // cout<<"delocate  from "<<this->id<<" "<<buildEdgeName(edge)<<endl;

    auto it = this->pendingMemories.find(edge); // Find the element by key
    if (it == this->pendingMemories.end()) {
        cout<<"not fnd1"<<endl;
    }
    if(!real){
        delocateFromThisProcessorToDisk(edge, this->id);
    }

    return removePendingMemory(edge);

}

std::set<edge_t *, decltype(Processor::comparePendingMemories)*>::iterator Processor::delocateToDiskOptionally(edge_t* edge, bool real) {
   // cout<<"delocate optionally  from "<<this->id<<" "<<buildEdgeName(edge)<<endl;

    auto it = this->pendingMemories.find(edge); // Find the element by key
    if (it == this->pendingMemories.end()) {
        return this->pendingMemories.end();
    }
    if(!real){
        delocateFromThisProcessorToDisk(edge, this->id);
    }
    return removePendingMemory(edge);

}

void Processor::loadFromDisk(edge_t* edge) {
    addPendingMemory(edge);
    locateToThisProcessorFromDisk(edge, this->id);
}

void Processor::loadFromNowhere(edge_t *edge) {
  //  cout<<"load onto proc "<<this->id<<" ";
  //  print_edge(edge);
    addPendingMemory(edge);
    locateToThisProcessorFromNowhere(edge, this->id);
  //  cout<<"remains mem "<< this->availableMemory<<endl;
}



vertex_t *Processor::getAssignedTask() const {
    return assignedTask;
}

int Processor::getAssignedTaskId() const {
    return assignedTask->id;
}

void Processor::setPendingMemories( set<edge_t *, std::function<bool(edge_t*, edge_t*)>> &newSet) {
  //  this->pendingMemories = pendingMemories;
    this->pendingMemories.swap(newSet);
}

double Processor::getAfterAvailableMemory() const {
    assert(afterAvailableMemory>=0);
    if(afterAvailableMemory>memorySize){
        assert(abs(afterAvailableMemory - memorySize)< 0.1);
    }

    return afterAvailableMemory;
}

void Processor::setAfterAvailableMemory(double d) {
    //cout<<"set after available memory of proc "<<this->id<<" to "<<d<<endl;
    if(abs(d-memorySize)<0.0001){
        d=memorySize;
    }

    assert(d>=0 && d<= (memorySize+0.001));
     this->afterAvailableMemory = d;
}

void Processor::setAfterPendingMemories(set<edge_t *,  std::function<bool(edge_t*, edge_t*)>> &memories) {
   // this->afterPendingMemories = memories;
    this->afterPendingMemories.swap(memories);
}



void Cluster::clean() {
    processors.clear();

}


//Processor::Processor(const Processor & copy)= default;
Processor::Processor(const Processor& copy)
        : memorySize(copy.memorySize),
          processorSpeed(copy.processorSpeed),
          assignedTask(copy.assignedTask),
          eventsOnProc(copy.eventsOnProc),
          id(copy.id),
          name(copy.name),
          isBusy(copy.isBusy),
          readyTimeCompute(copy.readyTimeCompute),
          readyTimeRead(copy.readyTimeRead),
          readyTimeWrite(copy.readyTimeWrite),
          softReadyTimeWrite(copy.softReadyTimeWrite),
          memoryOffloadingPenalty(copy.memoryOffloadingPenalty),
          availableMemory(copy.availableMemory),
          pendingMemories(copy.comparePendingMemories),  // Initialize with correct comparator
          afterAvailableMemory(copy.afterAvailableMemory),
          afterPendingMemories(copy.comparePendingMemories),  // Initialize with correct comparator
          readSpeedDisk(copy.readSpeedDisk),
          writeSpeedDisk(copy.writeSpeedDisk),
          peakMemConsumption(copy.peakMemConsumption),
          assignment(copy.assignment),
          lastReadEvent(copy.lastReadEvent),
          lastWriteEvent(copy.lastWriteEvent),
          lastComputeEvent(copy.lastComputeEvent)
{
    // Copy elements manually to preserve set integrity
    for (auto* mem : copy.pendingMemories) {
        pendingMemories.insert(mem);
    }

    for (auto* mem : copy.afterPendingMemories) {
        afterPendingMemories.insert(mem);
    }
}