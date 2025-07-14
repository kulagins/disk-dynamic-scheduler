#ifndef cluster_h
#define cluster_h

#include <cassert>
#include <iostream>
#include <memory>
#include <ostream>
#include <tuple>
#include <vector>

#include "graph.hpp"
#include <algorithm>
#include <functional>
#include <set>
#include <unordered_map>

enum ClusteringModes {
    treeDependent,
    staticClustering
};

class Event;

class Processor : public std::enable_shared_from_this<Processor> {
protected:
    double memorySize = 0;
    double processorSpeed = 1;
    vertex_t* assignedTask = nullptr;
    // std::unordered_map<string, std::weak_ptr<Event>> eventsOnProc;
    bool isKeptValid = true;

public:
    static bool comparePendingMemories(const edge_t* a, const edge_t* b)
    {
        assert(a);
        assert(b);
        // Compare:
        // 1. weight
        // 2. head task id
        // 3. tail task id
        return std::tie(a->weight, a->head->id, a->tail->id) > std::tie(b->weight, b->head->id, b->tail->id);
    }

    int id = -1;
    std::string name = {};
    bool isBusy = false;

    double softReadyTimeWrite = 0;

    double memoryOffloadingPenalty = 0;
    std::vector<edge_t*> writingQueue = {};

protected:
    double availableMemory = 0;
    std::set<edge_t*, std::function<bool(edge_t*, edge_t*)>> pendingMemories { comparePendingMemories };

    double afterAvailableMemory = 0;
    std::set<edge_t*, std::function<bool(edge_t*, edge_t*)>> afterPendingMemories { comparePendingMemories };

public:
    double readSpeedDisk = 0;
    double writeSpeedDisk = 0;
    double peakMemConsumption = 0;

    std::string assignment = {};

private:
    std::weak_ptr<Event> lastReadEvent = {};
    std::weak_ptr<Event> lastWriteEvent = {};
    std::weak_ptr<Event> lastComputeEvent = {};

    double readyTimeCompute = 0;
    double readyTimeRead = 0;
    double readyTimeWrite = 0;

public:
    Processor() = default;

    explicit Processor(const double memorySize, const int id = -1)
        : memorySize(memorySize)
        , id(id)
    {
    }

    Processor(const double memorySize, const double processorSpeed, const int id = -1)
        : memorySize(memorySize)
        , processorSpeed(processorSpeed)
        , id(id)
    {
    }

    double getMemorySize() const
    {
        return memorySize;
    }

    void setMemorySize(const double memory)
    {
        this->memorySize = memory;
    }

    double getProcessorSpeed() const
    {
        return processorSpeed;
    }

    void setProcessorSpeed(const double s)
    {
        this->processorSpeed = s;
    }

    double getAvailableMemory()
    {
        if (std::abs(availableMemory - memorySize) < 0.01) {
            availableMemory = memorySize;
        }
        assert(!isKeptValid || (availableMemory >= 0 && availableMemory <= memorySize));
        return this->availableMemory;
    }

    void setAvailableMemory(const double& mem)
    {
        // cout<<"set available memory of proc "<<this->id<<" to "<<mem<<endl;
        assert(mem >= 0);
        if (mem > memorySize && isKeptValid) {
            assert(std::abs(mem - memorySize) < 0.1);
        }

        this->availableMemory = mem;
    }

    void setReadyTimeCompute(const double newTime)
    {
        // cout<<"proc "<<this->id<<"now ready at "<<newTime<<endl;
        this->readyTimeCompute = newTime;
    }
    int getAssignedTaskId() const;

    void setIsKeptValid(const bool is)
    {
        this->isKeptValid = is;
    }

    bool getIsKeptValid() const
    {
        return this->isKeptValid;
    }

    vertex_t* getAssignedTask() const;

    void assignSubgraph(vertex_t* taskToBeAssigned);

    std::set<edge_t*, decltype(comparePendingMemories)*>::iterator delocateToDisk(edge_t* edge, bool shouldUseImaginary, double afterWhen);
    std::set<edge_t*, decltype(comparePendingMemories)*>::iterator delocateToNowhere(edge_t* edge, bool shouldUseImaginary, double afterWhen);
    void loadFromDisk(edge_t* edge, bool shouldUseImaginary, double afterWhen);
    void loadFromNowhere(edge_t* edge, bool shouldUseImaginary, double afterWhen);
    std::set<edge_t*, decltype(comparePendingMemories)*>::iterator delocateToDiskOptionally(edge_t* edge, bool shouldUseImaginary, double afterWhen);
    std::set<edge_t*, decltype(comparePendingMemories)*>::iterator delocateToNowhereOptionally(edge_t* edge, bool shouldUseImaginary, double afterWhen);

    std::set<edge_t*, decltype(comparePendingMemories)*>::iterator
    removePendingMemory(edge_t* edgeToRemove)
    {
        // cout<<"removing pending memory "<<buildEdgeName(edgeToRemove)<<" from proc "<<this->id<<endl;

        const auto it = pendingMemories.find(edgeToRemove);
        if (it == pendingMemories.end()) {
            if (isKeptValid) {
                throw std::runtime_error("not found edge in pending " + buildEdgeName(edgeToRemove));
            } else {
                return pendingMemories.end();
            }
        }

        const auto nextIt = std::next(it); // Get the next iterator before erasing
        pendingMemories.erase(it); // Now erase safely

        availableMemory += edgeToRemove->weight;
        assert(!isKeptValid || availableMemory < memorySize || std::abs(availableMemory - memorySize) < 0.1);

        return nextIt; // Return the next iterator, which is safe
    }

    void addPendingMemory(edge_t* edge)
    {
        //  cout<<"Add pending memory "<<buildEdgeName(edge)<<endl;
        if (isKeptValid) {
            if (!edge) {
                throw std::runtime_error("Edge is null!");
            }
            if (!edge->head || !edge->tail) {
                throw std::runtime_error("Edge has uninitialized fields!");
            }
            if (pendingMemories.find(edge) != pendingMemories.end()) {
                throw std::runtime_error(" found edge in pending");
            }
        }
        pendingMemories.emplace(edge);
        availableMemory -= edge->weight;
        assert(!isKeptValid || availableMemory >= 0);
    }

    void addPendingMemoryAfter(edge_t* edge)
    {
        if (afterPendingMemories.find(edge) != afterPendingMemories.end()) {
            throw std::runtime_error("Found edge in pending");
        }

        afterPendingMemories.emplace(edge);
        afterAvailableMemory -= edge->weight;
        if (afterAvailableMemory < 0) {
            throw std::runtime_error("After avail mem <0");
        }
    }

    std::set<edge_t*, decltype(Processor::comparePendingMemories)*>::iterator
    // bool
    removePendingMemoryAfter(edge_t* edgeToRemove)
    {

        auto it = afterPendingMemories.find(edgeToRemove);
        if (it == afterPendingMemories.end()) {
            throw std::runtime_error("not found edge in pending " + buildEdgeName(edgeToRemove));
        }

        auto nextIt = std::next(it); // Get the next iterator before erasing
        afterPendingMemories.erase(it); // Now erase safely

        afterAvailableMemory += edgeToRemove->weight;
        assert(afterAvailableMemory < memorySize || std::abs(afterAvailableMemory - memorySize) < 0.1);

        return nextIt; // Return the next iterator, which is safe
    }

    void addEvent(const std::shared_ptr<Event>& event);

    // std::unordered_map<string, std::weak_ptr<Event>>  getEvents(){
    //     return this->eventsOnProc;
    // }
    void updateFrom(const Processor& other);

    std::set<edge_t*, std::function<bool(edge_t*, edge_t*)>>& getPendingMemories()
    {
        return pendingMemories;
    }

    void setPendingMemories(std::set<edge_t*, std::function<bool(edge_t*, edge_t*)>>& pendingMemories);

    void resetPendingMemories()
    {
        this->pendingMemories.clear();
    }
    void resetAfterPendingMemories()
    {
        this->afterPendingMemories.clear();
    }

    double getAfterAvailableMemory() const;

    void setAfterAvailableMemory(double d);

    std::set<edge_t*, std::function<bool(edge_t*, edge_t*)>>& getAfterPendingMemories()
    {
        return afterPendingMemories;
    }

    void setAfterPendingMemories(std::set<edge_t*, std::function<bool(edge_t*, edge_t*)>>& memories);

    edge_t* getBiggestPendingEdgeThatIsNotIncomingOfAndLocatedOnProc(const vertex_t* v) const
    {
        for (const auto pendingMemory : pendingMemories) {
            if (pendingMemory->head->name != v->name) {
                return pendingMemory;
            }
        }
        // throw runtime_error("No pending memories that are not incoming edges of task "+v->name);
        return nullptr;
    }

    void setLastWriteEvent(const std::shared_ptr<Event>& lwe);
    void setLastReadEvent(const std::shared_ptr<Event>& lwe);
    void setLastComputeEvent(const std::shared_ptr<Event>& lwe);

    std::weak_ptr<Event> getLastWriteEvent()
    {
        return this->lastWriteEvent;
    }
    std::weak_ptr<Event> getLastReadEvent()
    {
        return this->lastReadEvent;
    }
    std::weak_ptr<Event> getLastComputeEvent()
    {
        return this->lastComputeEvent;
    }
    double getReadyTimeWrite();
    double getReadyTimeRead();
    double getReadyTimeCompute();

    double getExpectedOrActualReadyTimeWrite() const;
    double getExpectedOrActualReadyTimeRead() const;
    double getExpectedOrActualReadyTimeCompute() const;

    void setReadyTimeWrite(const double rtw)
    {
        this->readyTimeWrite = rtw;
    }
    void setReadyTimeRead(const double rtr)
    {
        this->readyTimeRead = rtr;
    }
};

class Cluster {
protected:
    std::vector<edge_t*> filesOnDisk;
    int maxSpeed = -1;
    bool isKeptValid = true;

public:
    std::unordered_map<int, std::shared_ptr<Processor>> processors;

    ~Cluster()
    {
        filesOnDisk.resize(0);
        processors.clear();
    }

    Cluster()
    {
        processors.clear();
    }

public:
    void addProcessor(const std::shared_ptr<Processor>& p)
    {
        if (processors[p->id] != nullptr) {
            throw std::runtime_error("non-unique processor id " + std::to_string(p->id));
        }
        processors[p->id] = p;
    }

    std::unordered_map<int, std::shared_ptr<Processor>>& getProcessors()
    {
        return this->processors;
    }

    [[nodiscard]] unsigned int getNumberProcessors() const
    {
        return this->processors.size();
    }

    void printProcessors()
    {

        for (const auto& [key, value] : this->processors) {
            std::cout << "Processor " << value->id << "with memory " << value->getMemorySize() << ", speed " << value->getProcessorSpeed()
                      << " and busy? " << value->isBusy //<< " assigned " << (value->isBusy?value->getAssignedTaskId(): -1)
                      << " ready time compute " << value->getReadyTimeCompute()
                      << " ready time read " << value->getReadyTimeRead()
                      << " ready time write " << value->getReadyTimeWrite()
                      //<< " ready time write soft " << value->softReadyTimeWrite
                      //<< " avail memory " << value->availableMemory
                      << " writing queue size " << value->writingQueue.size()
                      << " pending in memory " << value->getPendingMemories().size() << " pcs: "

                ;

            for (const auto& item : value->getPendingMemories()) {
                std::cout << buildEdgeName(item) << ", ";
            }
            std::cout << '\n';
        }
    }

    void printProcessorsEvents();

    void mayBecomeInvalid()
    {
        this->isKeptValid = false;
        for (const auto& [id, processor] : processors) {
            processor->setIsKeptValid(false);
        }
    }

    std::shared_ptr<Processor> getMemBiggestFreeProcessor();
    std::shared_ptr<Processor> getFastestFreeProcessor();
    std::shared_ptr<Processor> getFastestProcessorFitting(double memReq);

    std::shared_ptr<Processor> getProcessorById(const int id)
    {
        return processors.at(id);
    }

    std::shared_ptr<Processor> getOneProcessorByName(const std::string& name)
    {
        for (const auto& [key, value] : this->processors) {
            if (value->name == name)
                return value;
        }
        throw std::runtime_error("Processor not found by name " + name);
    }

    void clean();

    void printAssignment();
};

std::vector<edge_t*> getBiggestPendingEdge(std::shared_ptr<Processor> pj);

#endif
