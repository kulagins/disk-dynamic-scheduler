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
std::set<edge_t *, decltype(Processor::comparePendingMemories)*>::iterator Processor::delocateToDisk(edge_t* edge) {
   //cout<<"delocate  from "<<this->id<<" "; print_edge(edge);

    auto it = this->pendingMemories.find(edge); // Find the element by key
    if (it == this->pendingMemories.end()) {
        cout<<"not fnd1"<<endl;
    }
    delocateFromThisProcessorToDisk(edge, this->id);
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

void Cluster::clean() {
    processors.clear();

}


Processor::Processor(const Processor & copy)= default;