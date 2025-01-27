
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
class Event  : public std::enable_shared_from_this<Event> {
public:
    string id;
    vertex_t* task;
    edge_t* edge;
    eventType type;
    shared_ptr<Processor> processor;
    double expectedTimeFire;
    double actualTimeFire;
    vector<shared_ptr<Event>> predecessors, successors;
    bool isEviction;

    Event(vertex_t* task, edge_t* edge,
          eventType type,  shared_ptr<Processor> processor,  double expectedTimeFire, double actualTimeFire,
          vector<shared_ptr<Event>>& predecessors,  vector<shared_ptr<Event>>& successors, bool isEviction, string id):
                task(task),
                edge(edge),
                type(type),
                processor(processor),
                expectedTimeFire(expectedTimeFire),
                actualTimeFire(actualTimeFire),
                predecessors(predecessors),
                successors(successors),
                isEviction(isEviction),
                id(id){
        cout<<"creating event "<<id<<endl;
    }

    void fire();
    void fireTaskStart();
    void fireTaskFinish();
    void fireReadStart();
    void fireReadFinish();
    void fireWriteStart();
    void fireWriteFinish();

    void removeOurselfFromSuccessors(Event *us);
};

struct CompareByTimestamp {
    bool operator()(const shared_ptr<Event> a, const shared_ptr<Event> b) const {
        return a->actualTimeFire < b->actualTimeFire ||
               (a->actualTimeFire == b->actualTimeFire && a->id < b->id);
    }
};


class EventManager {
private:
    std::multiset<shared_ptr<Event>, CompareByTimestamp> eventSet; // Sorted by timestamp
    std::unordered_map<string, std::multiset<shared_ptr<Event>>::iterator> eventMap; // Fast lookup by ID
    std::unordered_map<int, std::vector<std::multiset<shared_ptr<Event>>::iterator>> eventByProcessorMap; // Fast lookup by ID
public:
    // Insert a new event
    void insert(const shared_ptr<Event>& event) {
        cout<<"inserting "<<event->id<<" ";
        auto foundIterator = eventMap.find(event->id);
        if (foundIterator != eventMap.end()) {
            cout<<"updating "<<event->id<<" from "<< foundIterator->second->get()->actualTimeFire<<" to "<<event->actualTimeFire<<endl;
            if(foundIterator->second->get()->actualTimeFire<event->actualTimeFire){
                update(event->id, event->actualTimeFire);
            }

        }
        else{
            auto it = eventSet.insert(event);
            eventMap[event->id] = it;
            eventByProcessorMap[event->processor->id].emplace_back( it);
        }
    }

    shared_ptr<Event> find(string id) {
        if (eventMap.find(id) != eventMap.end()) {
            return *eventMap[id];
        }
        return nullptr;
    }


    // Update an event's timestamp
    bool update(string id, double newTimestamp) {
        auto it = eventMap.find(id);
        if (it != eventMap.end()) {
            // Remove from multiset
            shared_ptr<Event> updatedEvent = *(it->second);
            eventSet.erase(it->second);

            // Update the timestamp and reinsert
            updatedEvent->actualTimeFire = newTimestamp;
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
    shared_ptr<Event> getEarliest() const {
        if (!eventSet.empty()) {
            return *eventSet.begin();
        }
        return nullptr; // Empty set
    }

    // Print all events (for debugging)
    void printAll() const {
        for (const auto& event : eventSet) {
            std::cout << "ID: " << event->id<<",\t";
          //            << ", Timestamp: " << event.timestamp
         //             << ", Name: " << event.name << "\n";
        }
        cout<<endl<<"-------";cout<<endl;

        for (const auto& event : eventMap) {
            std::cout << "ID: " << event.first<<" to "<< event.second->get()->id<< ",\t";
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