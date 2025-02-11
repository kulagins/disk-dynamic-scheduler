
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
    bool isDone= false;
private:
    Event(vertex_t* task, edge_t* edge,
          eventType type,  shared_ptr<Processor> processor,  double expectedTimeFire, double actualTimeFire,
         // vector<shared_ptr<Event>>& predecessors,  vector<shared_ptr<Event>>& successors,
          bool isEviction, string id):
                task(task),
                edge(edge),
                type(type),
                processor(processor),
                expectedTimeFire(expectedTimeFire),
                actualTimeFire(actualTimeFire),
               // predecessors(predecessors),
               // successors(successors),
                isEviction(isEviction),
                id(id){
       // cout<<"creating event "<<id<<endl;
    }


    void initialize(const std::vector<std::shared_ptr<Event>>& predecessors,
                    const std::vector<std::shared_ptr<Event>>& successors) {
        for (const auto& pred : predecessors) {
            this->addPredecessor(pred);
        }
        for (const auto& succ : successors) {
            this->addSuccessor(succ);
        }
    }
public:
    static std::shared_ptr<Event> createEvent(vertex_t* task, edge_t* edge,
                                              eventType type, std::shared_ptr<Processor> processor,
                                              double expectedTimeFire, double actualTimeFire,
                                              const std::vector<std::shared_ptr<Event>>& predecessors,
                                              const std::vector<std::shared_ptr<Event>>& successors,
                                              bool isEviction, const std::string& id) {
        auto event = std::shared_ptr<Event>(new Event(task, edge, type, processor, expectedTimeFire, actualTimeFire, isEviction, id));
        event->initialize(predecessors, successors);
        return event;
    }
    void fire();
    void fireTaskStart();
    void fireTaskFinish();
    void fireReadStart();
    void fireReadFinish();
    void fireWriteStart();
    void fireWriteFinish();

    void removeOurselfFromSuccessors(Event *us);

    void addPredecessor(shared_ptr<Event> pred){
        if(pred->id == this->id){
            throw runtime_error("ADDING OURSELVES AS PREDECESSOR!");
        }
        //cout<<"pred "<<pred->id<<" -> "<<this->id<<endl;
        this->predecessors.emplace_back(pred);
        if(pred->actualTimeFire> this->actualTimeFire){
            this->actualTimeFire= pred->actualTimeFire;
            this->expectedTimeFire= pred->actualTimeFire;
        }
        pred->addSuccessor(shared_from_this());
    }

    void addSuccessor(shared_ptr<Event> succ){
        assert(succ!= nullptr);
       // cout<<"add successor "<<succ->id<<" to event "<<this->id<<endl;
        this->successors.emplace_back(succ);
        if(succ->actualTimeFire< this->actualTimeFire){
            succ->actualTimeFire= this->actualTimeFire;
            succ->expectedTimeFire= this->actualTimeFire;
        }
        string thisId=this->id;
        auto it = find_if(succ->predecessors.begin(), succ->predecessors.end(), [&thisId](const shared_ptr<Event> obj) {return obj->id == thisId;});

        if(it==succ->predecessors.end()) {
            succ->addPredecessor(shared_from_this());
        }
    }

};





struct CompareByTimestamp {
    bool customCharCompare(char a, char b) const {
      /*  if(a=='s'&& b=='f'){
            //cout<<a<<" "<<b<<endl;
            return (b > a);
        }
        if(b=='s'&& a=='f'){
           // cout<<a<<" "<<b<<" "<< (a < b)<<endl;
            return -1 ;//(a < b);
        }
        if(b=='w'&& a=='r'){
            // cout<<a<<" "<<b<<" "<< (a < b)<<endl;
            return -1 ;//(a < b);
        }
        if(a=='w'&& b=='r'){
            // cout<<a<<" "<<b<<" "<< (a < b)<<endl;
            return false;//true;//-1;//-1 ;//(a < b);
        } */
        if (tolower(a) == tolower(b)) {
            return islower(a) && isupper(b);  // Uppercase is "greater" if letters are same
        }
        return tolower(a) < tolower(b);
    }

    bool customIDCompare(const std::string& a, const std::string& b) const {
        size_t min_len = std::min(a.size(), b.size());
        for (size_t i = 0; i < min_len; ++i) {
            if (a[i] != b[i])
                return customCharCompare(a[i], b[i]);
        }
        return a.size() < b.size();  // Compare by length if all chars are equal
    }
    bool operator()(const shared_ptr<Event> a, const shared_ptr<Event> b) const {
     //   cout<<"compare by timestamp "<<a->id<<" and "<<b-> id<<" "<< ((a->id < b->id)?"less": "not less")<<endl;
        //return a->actualTimeFire < b->actualTimeFire ||
         //      (a->actualTimeFire == b->actualTimeFire && a->id < b->id);
        if (a->actualTimeFire != b->actualTimeFire) {
            return a->actualTimeFire < b->actualTimeFire;
        }
        if(std::find(a->predecessors.begin(), a->predecessors.end(), b)!= a->predecessors.end()){
            //b is predecessor of a
            return false;
        }
        else if(std::find(b->predecessors.begin(), b->predecessors.end(), a)!= b->predecessors.end()) {
            //a is predecessor of b
            return true;
        }
        bool compare = customIDCompare(a->id, b->id);
        return compare;
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
        //cout<<"inserting "<<event->id<<" ";
        auto foundIterator = eventMap.find(event->id);
        if (foundIterator != eventMap.end()) {
        //    cout<<"updating "<<event->id<<" from "<< foundIterator->second->get()->actualTimeFire<<" to "<<event->actualTimeFire;
            if(foundIterator->second->get()->actualTimeFire<event->actualTimeFire){
          //      cout<<" updated";
                update(event->id, event->actualTimeFire);
            }
       //     cout <<endl;

        }
        else{
            auto it = eventSet.insert(event);
            eventMap[event->id] = it;
            eventByProcessorMap[event->processor->id].emplace_back( it);
           // cout<<endl;
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
        cout<<"update event "<<id <<" to time "<<newTimestamp<<endl;
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
      //  cout<<"removing "<<id<<endl;
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
            std::cout << "ID: " << event->id<< " at"<< event->actualTimeFire <<",\t";
          //            << ", Timestamp: " << event.timestamp
         //             << ", Name: " << event.name << "\n";
        }
        cout<<endl<<"-------";cout<<endl;

      //  for (const auto& event : eventMap) {
       //     std::cout << "ID: " << event.first<<" to "<< event.second->get()->id<< ",\t";
            //            << ", Timestamp: " << event.timestamp
            //             << ", Name: " << event.name << "\n";
        //}
        //cout<<endl;
       // cout<<"-------------------------------------------";
        //cout<<endl;
    }

    void forAll(shared_ptr<Processor> newProc){
        for (auto& event : eventSet) {
           if(event->processor->id==newProc->id) {
               event->processor= newProc;
           }
        }
    }

    bool empty(){
        return eventSet.empty();
    }
};

#endif