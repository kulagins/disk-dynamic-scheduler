
#ifndef FONDA_SCHED_COMMON_HPP
#define FONDA_SCHED_COMMON_HPP



#include "../../extlibs/memdag/src/graph.hpp"
#include "json.hpp"
#include "cluster.hpp"
#include <queue>


class Assignment{


public:
    vertex_t * task;
    Processor * processor;
    double startTime;
    double finishTime;

    Assignment(vertex_t * t, Processor * p, double st, double ft){
        this->task =t;
        this->processor =p;
        this->startTime = st;
        this->finishTime = ft;
    }
    ~Assignment(){

    }

    nlohmann::json toJson() const {
        string tn = task->name;
        std::transform(tn.begin(), tn.end(), tn.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return nlohmann::json{
                {"task", tn},
                {"start", startTime}, {"machine", processor->id}, {"finish", finishTime}};
    }
};



using json = nlohmann::json;

using namespace std;
void printDebug(string str);
void printInlineDebug(string str);
void checkForZeroMemories(graph_t *graph);
std::string trimQuotes(const std::string& str);

////void completeRecomputationOfSchedule(Http::ResponseWriter &resp, const json &bodyjson, double timestamp, vertex_t * vertexThatHasAProblem);
void removeSourceAndTarget(graph_t *graph, vector<pair<vertex_t *, double>> &ranks);

Cluster *
prepareClusterWithChangesAtTimestamp(const json &bodyjson, double timestamp, vector<Assignment *> &tempAssignments);


//void delayOneTask(Http::ResponseWriter &resp, const json &bodyjson, string &nameOfTaskWithProblem, double newStartTime,
 //                 Assignment *assignmOfProblem);
void delayEverythingBy(vector<Assignment*> &assignments, Assignment * startingPoint, double delayTime);
void takeOverChangesFromRunningTasks(json bodyjson, graph_t* currentWorkflow, vector<Assignment *> & assignments);

class EventManager;

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
    string id;
    vertex_t* task;
    edge_t* edge;
    eventType type;
    shared_ptr<Processor> processor;
    double expectedTimeFire;
    double actualTimeFire;
    vector<Event> predecessors, successors;
    bool isEviction;

    Event(vertex_t* task, edge_t* edge,
          eventType type,  shared_ptr<Processor> processor,  double expectedTimeFire, double actualTimeFire,
          vector<Event>& predecessors,  vector<Event>& successors, bool isEviction, string id):
                task(task),
                edge(edge),
                type(type),
                processor(processor),
                expectedTimeFire(expectedTimeFire),
                actualTimeFire(actualTimeFire),
                predecessors(predecessors),
                successors(successors),
                isEviction(isEviction),
                id(id){}

    void fire(Cluster * cluster, EventManager& events);
    void fireTaskStart(Cluster * cluste, EventManager& events);
    void fireTaskFinish(Cluster * cluste, EventManager& events);
    void fireReadStart(Cluster * cluste, EventManager& events);
    void fireReadFinish(Cluster * cluste, EventManager& events);
    void fireWriteStart(Cluster * cluste, EventManager& events);
    void fireWriteFinish(Cluster * cluste, EventManager& events);

};

struct CompareByTimestamp {
    bool operator()(const Event& a, const Event& b) const {
        return a.actualTimeFire < b.actualTimeFire ||
               (a.actualTimeFire == b.actualTimeFire && a.id < b.id);
    }
};


class EventManager {
private:
    std::multiset<Event, CompareByTimestamp> eventSet; // Sorted by timestamp
    std::unordered_map<string, std::multiset<Event>::iterator> eventMap; // Fast lookup by ID
public:
    // Insert a new event
    void insert(const Event& event) {
        cout<<"inserting "<<event.id<<" ";
        auto foundIterator = eventMap.find(event.id);
        if (foundIterator != eventMap.end()) {
            cout<<"updating "<<event.id<<" from "<< foundIterator->second->actualTimeFire<<" to "<<event.actualTimeFire<<endl;
            if(foundIterator->second->actualTimeFire<event.actualTimeFire){
                update(event.id, event.actualTimeFire);
            }

        }
        else{
            auto it = eventSet.insert(event);
            eventMap[event.id] = it;
        }
    }

    Event* find(string id) {
        if (eventMap.find(id) != eventMap.end()) {
            return const_cast<Event *>(&(*eventMap[id]));
        }
        return nullptr;
    }


    // Update an event's timestamp
    bool update(string id, double newTimestamp) {
        auto it = eventMap.find(id);
        if (it != eventMap.end()) {
            // Remove from multiset
            Event updatedEvent = *(it->second);
            eventSet.erase(it->second);

            // Update the timestamp and reinsert
            updatedEvent.actualTimeFire = newTimestamp;
            auto newIt = eventSet.insert(updatedEvent);

            // Update map entry
            eventMap[id] = newIt;
            return true;
        }
        return false; // Event not found
    }

    // Remove an event by ID
    bool remove(string id) {
        cout<<"removing "<<id<<endl;
        auto it = eventMap.find(id);
        if (it != eventMap.end()) {
            // Remove from multiset and map
            eventSet.erase(it->second);
            eventMap.erase(it);
            return true;
        }
        return false; // Event not found
    }

    // Get the earliest event (smallest timestamp)
    Event* getEarliest() const {
        if (!eventSet.empty()) {
            return const_cast<Event *>(&(*eventSet.begin()));
        }
        return nullptr; // Empty set
    }

    // Print all events (for debugging)
    void printAll() const {
        for (const auto& event : eventSet) {
            std::cout << "ID: " << event.id<<",\t";
          //            << ", Timestamp: " << event.timestamp
         //             << ", Name: " << event.name << "\n";
        }
        cout<<endl<<"-------";cout<<endl;

        for (const auto& event : eventMap) {
            std::cout << "ID: " << event.first<<" to "<< event.second->id<< ",\t";
            //            << ", Timestamp: " << event.timestamp
            //             << ", Name: " << event.name << "\n";
        }
        cout<<endl;
        cout<<"-------------------------------------------";
        cout<<endl;
    }

    bool empty(){
        return eventSet.empty();
    }
};

#endif