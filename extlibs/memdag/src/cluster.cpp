#include "cluster.hpp"
#include <numeric>

using namespace std;

Cluster *Cluster::fixedCluster = NULL;



//TODO CHECK!
shared_ptr<Processor>Cluster::getMemBiggestFreeProcessor() {
    sort(this->processors.begin(), this->processors.end(),
         [](const shared_ptr<Processor> &lhs, const shared_ptr<Processor> &rhs) { return lhs->getMemorySize() > rhs->getMemorySize(); });
    for (auto iter = this->processors.begin(); iter < this->processors.end(); iter++) {
        if (!(*iter)->isBusy)
            return (*iter);
    }
    return NULL;
}

shared_ptr<Processor> Cluster::getFastestProcessorFitting(double memReq) {
    sort(this->processors.begin(), this->processors.end(),
         [](const shared_ptr<Processor> &lhs, const shared_ptr<Processor> &rhs) { return lhs->getProcessorSpeed() < rhs->getProcessorSpeed(); });
    shared_ptr<Processor> fastest = *this->processors.begin();
    sort(this->processors.begin(), this->processors.end(),
         [](const shared_ptr<Processor> &lhs, const shared_ptr<Processor> &rhs) { return lhs->getMemorySize() > rhs->getMemorySize(); });
    for (auto iter = this->processors.begin(); iter < this->processors.end(); iter++) {
        if (!(*iter)->isBusy && (*iter)->getMemorySize()>= memReq && (*iter)->getProcessorSpeed()>= fastest->getProcessorSpeed())
            fastest=(*iter);
    }
    return fastest;
}

shared_ptr<Processor>Cluster::getFastestFreeProcessor() {
//todo make sure they are not resorted
    for (auto iter = this->processors.begin(); iter < this->processors.end(); iter++) {
        if (!(*iter)->isBusy)
            return (*iter);
    }
    throw std::out_of_range("No free processor available anymore!");
}

//todo rewrite with flag
bool Cluster::hasFreeProcessor() {
    for (auto iter = this->processors.begin(); iter < this->processors.end(); iter++) {
        if (!(*iter)->isBusy)
            return true;
    }
    return false;
}

shared_ptr<Processor> Cluster::getFirstFreeProcessorOrSmallest() {
  //  std::sort(this->processors.begin(), this->processors.end(), [](Processor *a, Processor *b) {
 //       if(a==NULL || b==NULL) return true;
//        return (a->getMemorySize() <= b->getMemorySize());
 //   });
    for (auto iter = this->processors.begin(); iter < this->processors.end(); iter++) {
        if (!(*iter)->isBusy)
            return (*iter);
    }
    return this->processors.back();
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
   cout<<"delocate  from "<<this->id<<" ";
   print_edge(edge);
    auto it = this->pendingMemories.find(edge); // Find the element by key
    if (it != this->pendingMemories.end()) {
        delocateFromThisProcessorToDisk(edge, this->id);
        this->availableMemory += edge->weight;
        assert(this->memorySize> this->availableMemory || abs( this->memorySize- this->availableMemory )<0.1);
        return this->pendingMemories.erase(it);
    }
        // If not found, return end()
        cout<<"not fnd1"<<endl;
        return this->pendingMemories.end();
}

void Processor::loadFromDisk(edge_t* edge) {
    this->pendingMemories.insert(edge);
    locateToThisProcessorFromDisk(edge, this->id);
    this->availableMemory-=edge->weight;
    assert(this->availableMemory<=this->memorySize);
    assert(this->availableMemory>0);
}

void Processor::loadFromNowhere(edge_t *edge) {
    cout<<"load onto proc "<<this->id<<" ";
    print_edge(edge);
    this->pendingMemories.insert(edge);
    locateToThisProcessorFromNowhere(edge, this->id);
    this->availableMemory-=edge->weight;
    cout<<"remains mem "<< this->availableMemory<<endl;
    assert(this->availableMemory<=this->memorySize);
    assert(this->availableMemory>0);
}



vertex_t *Processor::getAssignedTask() const {
    return assignedTask;
}

int Processor::getAssignedTaskId() const {
    return assignedTask->id;
}

void Cluster::clean() {
    processors.clear();
    delete fixedCluster;

}

shared_ptr<Processor>Cluster::smallestFreeProcessorFitting(double requiredMem) {
    //TODO method for only free processors
    int min = std::numeric_limits<int>::max();
    shared_ptr<Processor> minProc = nullptr;
    for (auto proc: (this->getProcessors())) {
        if (proc->getMemorySize() >= requiredMem && !proc->isBusy && min > proc->getMemorySize()) {
            min = proc->getMemorySize();
            minProc = proc;
        }
    }
    return minProc;
}

void Cluster::freeAllBusyProcessors() {
    for (auto item: this->getProcessors()) {
        if (item->isBusy) {
            if (item->getAssignedTask() != NULL) {
               //TODO
                // item->getAssignedTask()->assignedProcessor=NULL;
            }
            item->isBusy = false;
            item->assignSubgraph(NULL);
        }
    }
    assert(this->getNumberProcessors() ==
           this->getNumberFreeProcessors());

}


void Cluster::sortProcessorsByProcSpeed() {
    sort(this->processors.begin(), this->processors.end(),
         [](const shared_ptr<Processor>&lhs, const shared_ptr<Processor>&rhs) { return lhs->getProcessorSpeed() > rhs->getProcessorSpeed(); });
    assert(this->getProcessors().at(0)->getProcessorSpeed() >
           this->getProcessors().at(this->getNumberProcessors() - 1)->getProcessorSpeed());

}


Processor::Processor(const Processor & copy)= default;