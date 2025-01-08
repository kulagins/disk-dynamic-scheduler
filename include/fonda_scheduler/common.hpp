
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
          vector<Event>& predecessors,  vector<Event>& successors, bool isEviction):
                task(task),
                edge(edge),
                type(type),
                processor(processor),
                expectedTimeFire(expectedTimeFire),
                actualTimeFire(actualTimeFire),
                predecessors(predecessors),
                successors(successors),
                isEviction(isEviction) {}

    void fire(Cluster * cluster, queue<Event>& events);
    void fireTaskStart(Cluster * cluste, queue<Event>& events);
    void fireTaskFinish(Cluster * cluste, queue<Event>& events);
    void fireReadStart(Cluster * cluste, queue<Event>& events);
    void fireReadFinish(Cluster * cluste, queue<Event>& events);
    void fireWriteStart(Cluster * cluste, queue<Event>& events);
    void fireWriteFinish(Cluster * cluste, queue<Event>& events);

};



#endif