//
// Created by kulagins on 26.05.23.
//

#include "../include/fonda_scheduler/common.hpp"

#include "fonda_scheduler/SchedulerHeader.hpp"
#include "graph.hpp"

bool Debug;

void delayEverythingBy(std::vector<Assignment*>& assignments, Assignment* startingPoint, double delayTime)
{

    for (auto& assignment : assignments) {
        if (assignment->startTime >= startingPoint->startTime) {
            assignment->startTime += delayTime;
            assignment->finishTime += delayTime;
        }
    }
}

std::string trimQuotes(const std::string& str)
{
    if (str.empty()) {
        return str; // Return the empty string if input is empty
    }

    std::string result = str, prevResult;
    do {
        prevResult = result;
        size_t start = 0;
        size_t end = prevResult.length() - 1;

        // Check for leading quote
        if (prevResult[start] == '"' || prevResult[start] == '\\' || prevResult[start] == ' ') {
            start++;
        }

        // Check for trailing quote
        if (prevResult[end] == '"' || prevResult[end] == '\\' || prevResult[end] == ' ') {
            end--;
        }

        result = prevResult.substr(start, end - start + 1);
    } while (result != prevResult);
    return result;
}

void Cluster::printAssignment()
{
    int counter = 0;
    for (const auto& item : this->getProcessors()) {
        if (item.second->isBusy) {
            counter++;
            std::cout << "Processor" << counter << "." << '\n';
            std::cout << "\tMem: " << item.second->getMemorySize() << ", Proc: " << item.second->getProcessorSpeed() << '\n';

            vertex_t* assignedVertex = item.second->getAssignedTask();
            if (assignedVertex == NULL)
                std::cout << "No assignment." << '\n';
            else {
                std::cout << "Assigned subtree, id " << assignedVertex->id << ", leader " << -1 << ", memReq " << assignedVertex->memoryRequirement << '\n';
                for (vertex_t* u = assignedVertex->subgraph->source; u; u = next_vertex_in_topological_order(assignedVertex->subgraph, u)) {
                    std::cout << u->name << ", ";
                }
                std::cout << '\n';
            }
        }
    }
}

void printDebug(const std::string& str)
{
    if (Debug) {
        std::cout << str << '\n';
    }
}
void printInlineDebug(const std::string& str)
{
    if (Debug) {
        std::cout << str;
    }
}

void checkForZeroMemories(graph_t* graph)
{
    for (vertex_t* vertex = graph->source; vertex; vertex = next_vertex_in_topological_order(graph, vertex)) {
        if (vertex->memoryRequirement == 0) {
            printDebug("Found a vertex with 0 memory requirement, name: " + vertex->name);
        }
    }
}

void removeSourceAndTarget(graph_t* graph, std::vector<std::pair<vertex_t*, double>>& ranks)
{

    auto iterator = std::find_if(ranks.begin(), ranks.end(),
        [](std::pair<vertex_t*, int> pair1) { return pair1.first->name == "GRAPH_SOURCE"; });
    if (iterator != ranks.end()) {
        ranks.erase(iterator);
    }
    iterator = find_if(ranks.begin(), ranks.end(),
        [](std::pair<vertex_t*, int> pair1) { return pair1.first->name == "GRAPH_TARGET"; });
    if (iterator != ranks.end()) {
        ranks.erase(iterator);
    }

    vertex_t* startV = findVertexByName(graph, "GRAPH_SOURCE");
    vertex_t* targetV = findVertexByName(graph, "GRAPH_TARGET");
    if (startV != nullptr)
        remove_vertex(graph, startV);
    if (targetV != nullptr)
        remove_vertex(graph, targetV);
}

bool isLocatedNowhere(edge_t* edge, bool imaginary)
{

    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;
    auto it = std::find_if(locations.begin(), locations.end(),
        [](Location location) {
            return location.locationType == LocationType::Nowhere;
        });
    return locations.empty() || it != locations.end();
}

bool isLocatedOnDisk(edge_t* edge, bool imaginary)
{

    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;
    return std::find_if(locations.begin(), locations.end(),
               [](Location location) {
                   return location.locationType == LocationType::OnDisk;
               })
        != locations.end();
}

bool isLocatedOnThisProcessor(edge_t* edge, int id, bool imaginary)
{

    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;
    return std::find_if(locations.begin(), locations.end(),
               [id](Location location) {
                   return location.locationType == LocationType::OnProcessor && location.processorId == id;
               })
        != locations.end();
}

bool isLocatedOnAnyProcessor(edge_t* edge, bool imaginary)
{

    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;
    return std::find_if(locations.begin(), locations.end(),
               [](Location location) {
                   return location.locationType == LocationType::OnProcessor;
               })
        != locations.end();
}

int whatProcessorIsLocatedOn(edge_t* edge, bool imaginary)
{

    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;
    auto locationOnProcessor = std::find_if(locations.begin(), locations.end(),
        [](Location location) {
            return location.locationType == LocationType::OnProcessor;
        });
    return (locationOnProcessor != locations.end()) ? locationOnProcessor->processorId.value() : -1;
}

std::string buildEdgeName(edge_t* edge)
{
    return edge->tail->name + "-" + edge->head->name;
}

void delocateFromThisProcessorToDisk(edge_t* edge, int id, bool imaginary, double afterWhen)
{

    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;
    auto locationOnThisProcessor = std::find_if(locations.begin(), locations.end(),
        [id](Location location) {
            return location.locationType == LocationType::OnProcessor && location.processorId == id;
        });
    // cout<<"delocating "; print_edge(edge);
    // assert(locationOnThisProcessor  != edge->locations.end());
    if (locationOnThisProcessor == edge->locations.end()) {
        locations.erase(locationOnThisProcessor);
        if (!isLocatedOnDisk(edge, imaginary))
            locations.emplace_back(LocationType::OnDisk, std::nullopt, afterWhen);

        throw std::runtime_error("not located on proc " + buildEdgeName(edge));
    }
    locations.erase(locationOnThisProcessor);
    if (!isLocatedOnDisk(edge, imaginary))
        locations.emplace_back(LocationType::OnDisk, std::nullopt, afterWhen);
}

void delocateFromThisProcessorToNowhere(edge_t* edge, int id, bool imaginary, double afterWhen)
{

    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;
    auto locationOnThisProcessor = std::find_if(locations.begin(), locations.end(),
        [id](Location location) {
            return location.locationType == LocationType::OnProcessor && location.processorId == id;
        });
    // cout<<"delocating "; print_edge(edge);
    // assert(locationOnThisProcessor  != edge->locations.end());
    if (locationOnThisProcessor == edge->locations.end()) {
        throw std::runtime_error("not located on proc " + buildEdgeName(edge));
    }
    locations.erase(locationOnThisProcessor);
}

void locateToThisProcessorFromDisk(edge_t* edge, int id, bool imaginary, double afterWhen)
{

    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;
    //  cout<<"locating to proc "<<id <<" edge "; print_edge(edge);
    if (!isLocatedOnDisk(edge, imaginary)) {
        std::cout << "NOT located on disk yet! Write&Read? " << buildEdgeName(edge) << '\n';
    }
    // assert(isLocatedOnDisk(edge));
    auto locationOnDisk = std::find_if(locations.begin(), locations.end(),
        [](Location location) {
            return location.locationType == LocationType::OnDisk;
        });
    if (!isLocatedOnThisProcessor(edge, id, imaginary))
        locations.emplace_back(LocationType::OnProcessor, id, afterWhen);
}
Location& getLocationOnProcessor(edge_t* edge, int id, bool imaginary)
{
    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;
    return *std::find_if(locations.begin(), locations.end(),
        [id](Location& location) {
            return location.locationType == LocationType::OnProcessor && location.processorId == id;
        });
}

/*Location &getLocationOnDisk(edge_t* edge, bool imaginary){
    vector<Location> &locations = imaginary? edge->imaginedLocations: edge->locations;
    return *std::find_if(locations.begin(), locations.end(),
                         [](Location location) {
                             return location.locationType == LocationType::OnDisk;
                         });
} */
Location& getLocationOnDisk(edge_t* edge, bool imaginary)
{
    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;

    auto it = std::find_if(locations.begin(), locations.end(),
        [](Location& location) { // Pass by reference ✅
            return location.locationType == LocationType::OnDisk;
        });

    if (it == locations.end()) {
        throw std::runtime_error("No OnDisk location found");
    }

    return *it; // ✅ Safe: Returns reference to actual Location in vector
}

void locateToThisProcessorFromNowhere(edge_t* edge, int id, bool imaginary, double afterWhen)
{

    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;
    //  cout<<"locating from nowhere to proc "<<id <<" edge "; print_edge(edge);
    if (!isLocatedOnThisProcessor(edge, id, imaginary))
        locations.emplace_back(LocationType::OnProcessor, id, afterWhen);
}
void locateToDisk(edge_t* edge, bool imaginary, double afterWhen)
{
    std::vector<Location>& locations = imaginary ? edge->imaginedLocations : edge->locations;
    if (!isLocatedOnDisk(edge, imaginary))
        locations.emplace_back(LocationType::OnDisk, afterWhen);
}

double getSumOut(vertex_t* v)
{
    double sumOut = 0;
    for (int i = 0; i < v->out_degree; i++) {
        sumOut += v->out_edges[i]->weight;
        //      cout<<sumOut<<" by "<<v->out_edges[i]->weight<<endl;
    }
    return sumOut;
}

double getSumIn(vertex_t* v)
{
    double sumIn = 0;
    for (int i = 0; i < v->in_degree; i++) {
        sumIn += v->in_edges[i]->weight;
        //      cout<<sumOut<<" by "<<v->out_edges[i]->weight<<endl;
    }
    return sumIn;
}

void Event::fire()
{
    switch (this->type) {
    case eventType::OnTaskStart:
        fireTaskStart();
        break;
    case eventType::OnTaskFinish:
        fireTaskFinish();
        break;
    case eventType::OnReadStart:
        fireReadStart();
        break;
    case eventType::OnReadFinish:
        fireReadFinish();
        break;
    case eventType::OnWriteStart:
        fireWriteStart();
        break;
    case eventType::OnWriteFinish:
        fireWriteFinish();
        break;
    }
    this->timesFired++;
}

void Processor::updateFrom(const Processor& other)
{

    assert(this->assignedTask == nullptr || this->assignedTask->id == other.assignedTask->id);

    std::unordered_map<std::string, std::weak_ptr<Event>> updatedEvents;

    this->readyTimeCompute = other.readyTimeCompute;
    this->readyTimeRead = other.readyTimeRead;
    this->readyTimeWrite = other.readyTimeWrite;

    assert(other.availableMemory <= other.getMemorySize() || abs(other.availableMemory - other.getMemorySize()) < 1);
    this->availableMemory = other.availableMemory;
    std::set<edge_t*, std::function<bool(edge_t*, edge_t*)>> updatedMemories(comparePendingMemories);
    // First, add elements that exist in both and new ones from 'other'
    for (auto* mem : other.pendingMemories) {
        updatedMemories.insert(mem); // Only inserts new ones, duplicates are ignored
    }
    // Swap the updated set into place
    pendingMemories.swap(updatedMemories);

    assert(other.afterAvailableMemory < other.getMemorySize() || abs(other.afterAvailableMemory - other.getMemorySize()) < 0.01);
    this->afterAvailableMemory = other.afterAvailableMemory;
    updatedMemories.clear();
    // First, add elements that exist in both and new ones from 'other'
    for (auto* mem : other.afterPendingMemories) {
        updatedMemories.insert(mem); // Only inserts new ones, duplicates are ignored
    }
    // Swap the updated set into place
    afterPendingMemories.swap(updatedMemories);

    this->lastReadEvent = other.lastReadEvent;
    this->lastWriteEvent = other.lastWriteEvent;
    this->lastComputeEvent = other.lastComputeEvent;

    this->writingQueue.clear();
    for (edge_t* e : other.writingQueue) {
        this->writingQueue.emplace_back(e);
    }
}

void clearGraph(graph_t* graphMemTopology)
{
    vertex_t* vertex = graphMemTopology->first_vertex;
    while (vertex != nullptr) {
        vertex->makespan = vertex->makespanPerceived = -1;
        vertex->visited = false;
        vertex->status = Status::Unscheduled;
        vertex->actuallyUsedMemory = -1;
        vertex->rank = -1;
        vertex->bottom_level = -1;
        vertex = vertex->next;
    }

    edge_t* edge = graphMemTopology->first_edge;
    while (edge != nullptr) {
        edge->locations.clear();
        edge = edge->next;
    }
}