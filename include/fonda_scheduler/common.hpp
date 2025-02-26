
#ifndef FONDA_SCHED_COMMON_HPP
#define FONDA_SCHED_COMMON_HPP


#include "../../extlibs/memdag/src/graph.hpp"
#include "json.hpp"
#include "cluster.hpp"
#include <queue>


class Assignment {


public:
    vertex_t *task;
    Processor *processor;
    double startTime;
    double finishTime;

    Assignment(vertex_t *t, Processor *p, double st, double ft) {
        this->task = t;
        this->processor = p;
        this->startTime = st;
        this->finishTime = ft;
    }

    ~Assignment() {

    }

    nlohmann::json toJson() const {
        string tn = task->name;
        std::transform(tn.begin(), tn.end(), tn.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return nlohmann::json{
                {"task",    tn},
                {"start",   startTime},
                {"machine", processor->id},
                {"finish",  finishTime}};
    }
};


using json = nlohmann::json;

using namespace std;

void printDebug(string str);

void printInlineDebug(string str);

void checkForZeroMemories(graph_t *graph);

std::string trimQuotes(const std::string &str);

////void completeRecomputationOfSchedule(Http::ResponseWriter &resp, const json &bodyjson, double timestamp, vertex_t * vertexThatHasAProblem);
void removeSourceAndTarget(graph_t *graph, vector<pair<vertex_t *, double>> &ranks);

Cluster *
prepareClusterWithChangesAtTimestamp(const json &bodyjson, double timestamp, vector<Assignment *> &tempAssignments);


//void delayOneTask(Http::ResponseWriter &resp, const json &bodyjson, string &nameOfTaskWithProblem, double newStartTime,
//                 Assignment *assignmOfProblem);
void delayEverythingBy(vector<Assignment *> &assignments, Assignment *startingPoint, double delayTime);

void takeOverChangesFromRunningTasks(json bodyjson, graph_t *currentWorkflow, vector<Assignment *> &assignments);

class EventManager;

enum eventType {
    OnTaskStart,
    OnTaskFinish,
    OnReadStart,
    OnReadFinish,
    OnWriteStart,
    OnWriteFinish
};

class Event : public std::enable_shared_from_this<Event> {
public:
    string id;
    vertex_t *task;
    edge_t *edge;
    eventType type;
    shared_ptr<Processor> processor;

    vector<shared_ptr<Event>> predecessors, successors;
    bool isEviction;
    bool isDone = false;
private:
    double expectedTimeFire = -1;
    double actualTimeFire = -1;

    Event(vertex_t *task, edge_t *edge,
          eventType type, shared_ptr<Processor> processor, double expectedTimeFire, double actualTimeFire,
            // vector<shared_ptr<Event>>& predecessors,  vector<shared_ptr<Event>>& successors,
          bool isEviction, string idN) :
            task(task),
            edge(edge),
            type(type),
            processor(processor),
            expectedTimeFire(expectedTimeFire),
            actualTimeFire(actualTimeFire),
            predecessors({}),
            successors({}),
            isEviction(isEviction),
            id(idN) {
        // cout<<"creating event "<<id<<endl;
    }


    void initialize(const std::vector<std::shared_ptr<Event>> &predecessors,
                    const std::vector<std::shared_ptr<Event>> &successors) {
        for (const auto &pred: predecessors) {
            this->addPredecessor(pred);
        }
        for (const auto &succ: successors) {
            this->addSuccessor(succ);
        }
    }

public:
    static std::shared_ptr<Event> createEvent(vertex_t *task, edge_t *edge,
                                              eventType type, std::shared_ptr<Processor> processor,
                                              double expectedTimeFire, double actualTimeFire,
                                              const std::vector<std::shared_ptr<Event>> &predecessors,
                                              const std::vector<std::shared_ptr<Event>> &successors,
                                              bool isEviction, const std::string &id) {
        auto event = std::shared_ptr<Event>(
                new Event(task, edge, type, processor, expectedTimeFire, actualTimeFire, isEviction, id));
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

    void addPredecessor(shared_ptr<Event> pred) {
        if (pred->id == this->id) {
            throw runtime_error("ADDING OURSELVES AS PREDECESSOR!");
        }
        //cout<<" ADDING pred "<<pred->id<<" -> "<<this->id<<endl;
        this->predecessors.emplace_back(pred);
        if (pred->actualTimeFire > this->actualTimeFire) {
            double diff = pred->actualTimeFire - this->actualTimeFire;
            this->setActualTimeFire(pred->actualTimeFire);
            this->setExpectedTimeFire(pred->actualTimeFire);

            propagateChain(shared_from_this(), diff);
        }

        pred->addSuccessor(shared_from_this());
    }

    void propagateChain(shared_ptr<Event> event, double add) {
        for (auto &successor: event->successors) {
            double newTime = successor->getActualTimeFire() + add;
            successor->setActualTimeFire(newTime);
            successor->setExpectedTimeFire(newTime);

            propagateChain(successor, add);
        }
    }

    void addSuccessor(shared_ptr<Event> succ) {
        assert(succ != nullptr);
        if (succ->id == this->id) {
            throw runtime_error("ADDING OURSELVES AS SUCCESSOR!");
        }
        // cout<<"add successor "<<succ->id<<" to event "<<this->id<<endl;
        this->successors.emplace_back(succ);
        if (succ->actualTimeFire < this->actualTimeFire) {
            double diff = abs(succ->actualTimeFire - this->actualTimeFire);
            succ->setActualTimeFire(this->actualTimeFire);
            succ->setExpectedTimeFire(this->actualTimeFire);

            propagateChain(succ, diff);
        }
        string thisId = this->id;
        auto it = find_if(succ->predecessors.begin(), succ->predecessors.end(),
                          [&thisId](const shared_ptr<Event> obj) { return obj->id == thisId; });

        if (it == succ->predecessors.end()) {
            succ->addPredecessor(shared_from_this());
        }
    }

    void setExpectedTimeFire(double d) {
        this->expectedTimeFire = d;
    }

    double getExpectedTimeFire() const {
        return this->expectedTimeFire;
    }

    void setActualTimeFire(double d) {
        if (d != this->actualTimeFire) {
            //   cout << "changing actual time fire for " << this->id << " from " << this->actualTimeFire << " to " << d
            //       << endl;
            this->actualTimeFire = d;
        }
    }

    double getActualTimeFire() {
        return this->actualTimeFire;
    }

    void setBothTimesFire(double d) {
        setActualTimeFire(d);
        setExpectedTimeFire(d);
    }

};


struct CompareByTimestamp {
    bool customCharCompare(char a, char b) const {
        if (tolower(a) == tolower(b)) {
            return islower(a) && isupper(b);  // Uppercase is "greater" if letters are same
        }
        if (a == 's' && b == 'f')
            return true;
        if (b == 's' && a == 'f') {
            return false;
        }
        return tolower(a) < tolower(b);
    }

    bool customIDCompare(const std::string &a, const std::string &b) const {
        size_t min_len = std::min(a.size(), b.size());
        for (size_t i = 0; i < min_len; ++i) {
            if (a[i] != b[i])
                return customCharCompare(a[i], b[i]);
        }
        return a.size() < b.size();  // Compare by length if all chars are equal
    }

    bool operator()(const shared_ptr<Event> a, const shared_ptr<Event> b) const {
        if (std::any_of(a->predecessors.begin(), a->predecessors.end(),
                        [&b](const shared_ptr<Event> &pred) { return pred->id == b->id; })) {
            cout << "Event " << a->id << " is a successor of " << b->id << " => a > b (return false)" << endl;
            return false;
        }
        if (std::any_of(b->predecessors.begin(), b->predecessors.end(),
                        [&a](const shared_ptr<Event> &pred) { return pred->id == a->id; })) {
            cout << "Event " << a->id << " is a predecessor of " << b->id << " => a < b (return true)" << endl;
            return true;
        }
        if (a->getActualTimeFire() != b->getActualTimeFire()) {
            return a->getActualTimeFire() < b->getActualTimeFire();
        }
        bool compare = customIDCompare(a->id, b->id);
        return compare;
    }
};


class EventManager {
private:
    std::multiset<shared_ptr<Event>, CompareByTimestamp> eventSet; // Sorted by timestamp
    std::unordered_map<string, std::multiset<shared_ptr<Event>>::iterator> eventMap; // Fast lookup by ID
public:
    // Insert a new event
    void insert(const shared_ptr<Event> &event) {
       // cout << "inserting " << event->id << " ";
        auto foundIterator = eventMap.find(event->id);
        if (foundIterator != eventMap.end()) {
        //    cout << "updating " << event->id << " from " << foundIterator->second->get()->getActualTimeFire() << " to "
         //        << event->getActualTimeFire() << endl;
            //if (foundIterator->second->get()->getActualTimeFire() < event->getActualTimeFire()) {
            //         cout<<" updated";
            //    update(event->id, event->getActualTimeFire());
            // }
            //    cout <<endl;

            remove(event->id);
        } else {
            //cout << "Inserting event: " << event->id << endl;
            for (const auto &pred: event->predecessors) {
                if (eventMap.find(pred->id) == eventMap.end()) {
                   // cout << "WARNING: Predecessor " << pred->id << " is missing!" << endl;
                }
            }
        }
        auto it = eventSet.insert(event);
        eventMap[event->id] = it;
        // cout<<endl;

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
        if (it != eventMap.end() && (*it->second)->getActualTimeFire() != newTimestamp) {
            //  cout<<"update event "<<id <<" to time "<<newTimestamp<<endl;
            // Remove from multiset
            shared_ptr<Event> updatedEvent = *(it->second);
            eventSet.erase(it->second);

            // Update the timestamp and reinsert
            updatedEvent->setActualTimeFire(newTimestamp);
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
        for (const auto &event: eventSet) {
            std::cout << "ID: " << event->id << " at" << event->getActualTimeFire() << ",\t";
            //            << ", Timestamp: " << event.timestamp
            //             << ", Name: " << event.name << "\n";
        }
        cout << endl << "-------";
        cout << endl;

        //  for (const auto& event : eventMap) {
        //     std::cout << "ID: " << event.first<<" to "<< event.second->get()->id<< ",\t";
        //            << ", Timestamp: " << event.timestamp
        //             << ", Name: " << event.name << "\n";
        //}
        //cout<<endl;
        // cout<<"-------------------------------------------";
        //cout<<endl;
    }

    void forAll(shared_ptr<Processor> newProc) {
        for (auto &event: eventSet) {
            if (event->processor->id == newProc->id) {
                event->processor = newProc;
            }
        }
    }

    bool empty() {
        return eventSet.empty();
    }
};

#endif