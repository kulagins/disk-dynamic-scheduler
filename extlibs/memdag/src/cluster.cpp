#include "cluster.hpp"
#include <numeric>

using namespace std;

Cluster *Cluster::fixedCluster = NULL;



//TODO CHECK!
Processor *Cluster::getMemBiggestFreeProcessor() {
    sort(this->processors.begin(), this->processors.end(),
         [](Processor *lhs, Processor *rhs) { return lhs->getMemorySize() > rhs->getMemorySize(); });
    for (vector<Processor *>::iterator iter = this->processors.begin(); iter < this->processors.end(); iter++) {
        if (!(*iter)->isBusy)
            return (*iter);
    }
    return NULL;
}

Processor *Cluster::getFastestProcessorFitting(double memReq) {
    sort(this->processors.begin(), this->processors.end(),
         [](Processor *lhs, Processor *rhs) { return lhs->getProcessorSpeed() < rhs->getProcessorSpeed(); });
    Processor * fastest = *this->processors.begin();
    sort(this->processors.begin(), this->processors.end(),
         [](Processor *lhs, Processor *rhs) { return lhs->getMemorySize() > rhs->getMemorySize(); });
    for (vector<Processor *>::iterator iter = this->processors.begin(); iter < this->processors.end(); iter++) {
        if (!(*iter)->isBusy && (*iter)->getMemorySize()>= memReq && (*iter)->getProcessorSpeed()>= fastest->getProcessorSpeed())
            fastest=(*iter);
    }
    return fastest;
}

Processor *Cluster::getFastestFreeProcessor() {
//todo make sure they are not resorted
    for (vector<Processor *>::iterator iter = this->processors.begin(); iter < this->processors.end(); iter++) {
        if (!(*iter)->isBusy)
            return (*iter);
    }
    throw std::out_of_range("No free processor available anymore!");
}

//todo rewrite with flag
bool Cluster::hasFreeProcessor() {
    for (vector<Processor *>::iterator iter = this->processors.begin(); iter < this->processors.end(); iter++) {
        if (!(*iter)->isBusy)
            return true;
    }
    return false;
}

Processor *Cluster::getFirstFreeProcessorOrSmallest() {
  //  std::sort(this->processors.begin(), this->processors.end(), [](Processor *a, Processor *b) {
 //       if(a==NULL || b==NULL) return true;
//        return (a->getMemorySize() <= b->getMemorySize());
 //   });
    for (vector<Processor *>::iterator iter = this->processors.begin(); iter < this->processors.end(); iter++) {
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
       this->assignedLeaderId = taskToBeAssigned->leader;
       taskToBeAssigned->assignedProcessor = this;
        this->isBusy = true;
    } else{
        this->assignedTask = NULL;
        this->isBusy = false;
    }
}



vertex_t *Processor::getAssignedTask() const {
    return assignedTask;
}

int Processor::getAssignedTaskId() const {
    return assignedTask->id;
}

void Cluster::clean() {
    for (Processor *p: getProcessors()) {
        delete p;
    }
    processors.resize(0);
    bandwidths.resize(0);
    delete fixedCluster;

}

Processor *Cluster::smallestFreeProcessorFitting(double requiredMem) {
    //TODO method for only free processors
    int min = std::numeric_limits<int>::max();
    Processor *minProc = nullptr;
    for (Processor *proc: (this->getProcessors())) {
        if (proc->getMemorySize() >= requiredMem && !proc->isBusy && min > proc->getMemorySize()) {
            min = proc->getMemorySize();
            minProc = proc;
        }
    }
    return minProc;
}
/*
Processor *Cluster::findSmallestFittingProcessorForMerge(Task *currentQNode, const Tree *tree, double requiredMemory) {
    Processor *optimalProcessor = nullptr;
    double optimalMemorySize = std::numeric_limits<double>::max();
    vector<Task *> *childrenvector = currentQNode->getParent()->getChildren();
    bool mergeThreeNodes = (currentQNode->getChildren()->empty()) & (childrenvector->size() == 2);
    vector<Processor *> eligibleProcessors;
    if (mergeThreeNodes) {
        eligibleProcessors.push_back(tree->getTask(childrenvector->front()->getOtherSideId())->getAssignedProcessor());
        eligibleProcessors.push_back(tree->getTask(childrenvector->back()->getOtherSideId())->getAssignedProcessor());
        eligibleProcessors.push_back(
                tree->getTask(currentQNode->getParent()->getOtherSideId())->getAssignedProcessor());
    } else {
        eligibleProcessors.push_back(tree->getTask(currentQNode->getOtherSideId())->getAssignedProcessor());
        eligibleProcessors.push_back(
                tree->getTask(currentQNode->getParent()->getOtherSideId())->getAssignedProcessor());
    }
    eligibleProcessors.erase(std::remove_if(eligibleProcessors.begin(),
                                            eligibleProcessors.end(),
                                            [](Processor *proc) { return proc == NULL; }),
                             eligibleProcessors.end());
    for (auto eligibleProcessor: eligibleProcessors) {
        if (eligibleProcessor != NULL && eligibleProcessor->getMemorySize() > requiredMemory &&
            eligibleProcessor->getMemorySize() < optimalMemorySize) {
            optimalProcessor = eligibleProcessor;
            optimalMemorySize = eligibleProcessor->getMemorySize();
        }
    }
    return optimalProcessor;
}
 */

void Cluster::freeAllBusyProcessors() {
    for (Processor *item: this->getProcessors()) {
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

bool cmp_processors_memsize(Processor *a, Processor *b) {
    return (a->getMemorySize() >= b->getMemorySize());
};

void Cluster::sortProcessorsByMemSize() {
  //  std::sort(this->processors.begin(), this->processors.end(), cmp_processors_memsize);
    //assert(this->getProcessors().at(0)->getMemorySize() >
     //      this->getProcessors().at(this->getNumberProcessors() - 1)->getMemorySize());

}

void Cluster::sortProcessorsByProcSpeed() {
    sort(this->processors.begin(), this->processors.end(),
         [](Processor *lhs, Processor *rhs) { return lhs->getProcessorSpeed() > rhs->getProcessorSpeed(); });
    assert(this->getProcessors().at(0)->getProcessorSpeed() >
           this->getProcessors().at(this->getNumberProcessors() - 1)->getProcessorSpeed());

}

Cluster::Cluster(const Cluster * copy){
    this->initHomogeneousBandwidth(copy->getNumberProcessors());

    for (const auto &item: copy->getProcessors()){
        //deep copy processor
        this->addProcessor(new Processor(item));
    }

    this->readyTimesBuffers.resize(copy->readyTimesBuffers.size());
    for (int i=0; i< copy->readyTimesBuffers.size(); i++){
        this->readyTimesBuffers.at(i).resize(copy->readyTimesBuffers.at(i).size());
        for (int j=0; j< copy->readyTimesBuffers.at(i).size(); j++){
            this->readyTimesBuffers.at(i).at(j) = copy->readyTimesBuffers.at(i).at(j);
        }
    }

    this->setHomogeneousBandwidth(copy->getBandwidth());


}

Processor::Processor(const Processor * copy){
    this->memorySize = copy->memorySize;
    this->processorSpeed = copy->getProcessorSpeed();
    this->id = copy->id;
   this-> isBusy = false;
    this-> assignedTask = nullptr;

   this->assignedTask = copy->assignedTask;
   this->assignedLeaderId = copy->assignedLeaderId;

   this->isBusy = copy->isBusy;
   this->visited = copy->visited;

   this->communicationBuffer = copy->communicationBuffer;
   this -> readyTime = copy->readyTime;
   this->availableMemory = copy->availableMemory;
   this->availableBuffer = copy-> availableBuffer;

    for (auto &item: copy->pendingMemories){
        this->pendingMemories.insert(item);
    }
    for (auto &item: copy->pendingInBuffer){
        this->pendingInBuffer.insert(item);
    }
   
}