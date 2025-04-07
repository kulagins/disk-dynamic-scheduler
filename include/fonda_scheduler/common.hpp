
#ifndef FONDA_SCHED_COMMON_HPP
#define FONDA_SCHED_COMMON_HPP


#include "../../extlibs/memdag/src/graph.hpp"
#include "json.hpp"
#include "cluster.hpp"
#include <queue>
#include <unordered_set>
#include <regex>
#include <utility>


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
void clearGraph(graph_t* graphMemTopology);

Cluster *
prepareClusterWithChangesAtTimestamp(const json &bodyjson, double timestamp, vector<Assignment *> &tempAssignments);


//void delayOneTask(Http::ResponseWriter &resp, const json &bodyjson, string &nameOfTaskWithProblem, double newStartTime,
//                 Assignment *assignmOfProblem);
void delayEverythingBy(vector<Assignment *> &assignments, Assignment *startingPoint, double delayTime);

void takeOverChangesFromRunningTasks(json bodyjson, graph_t *currentWorkflow, vector<Assignment *> &assignments);

class EventManager;

enum eventType {
    OnWriteStart = 0,
    OnWriteFinish = 1,
    OnReadStart = 2,
    OnReadFinish = 3,
    OnTaskStart = 4,
    OnTaskFinish = 5
};

class Event : public std::enable_shared_from_this<Event> {
public:
    string id;
    vertex_t *task;
    edge_t *edge;
    eventType type;
    shared_ptr<Processor> processor;

    set<shared_ptr<Event>> predecessors, successors;
    bool isEviction;
    bool isDone = false;
    int timesFired = 0;
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
            processor(std::move(processor)),
            expectedTimeFire(expectedTimeFire),
            actualTimeFire(actualTimeFire),
            predecessors({}),
            successors({}),
            isEviction(isEviction),
            id(std::move(idN)) {
        // cout<<"creating event "<<id<<endl;
    }


    void initialize(const std::vector<std::shared_ptr<Event>> &predecessors,
                    const std::vector<std::shared_ptr<Event>> &successors) {
        for (const auto &pred: predecessors) {
            this->addPredecessorInPlanning(pred);
        }
        for (const auto &succ: successors) {
            this->addSuccessorInPlanning(succ);
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
                new Event(task, edge, type, std::move(processor), expectedTimeFire, actualTimeFire, isEviction, id));
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

    void addPredecessorInPlanning(const shared_ptr<Event>& pred) {
        if (pred->id == this->id) {
            throw runtime_error("ADDING OURSELVES AS PREDECESSOR!");
        }
        //cout<<" ADDING pred "<<pred->id<<" -> "<<this->id<<endl;
        this->predecessors.insert(pred);
        double predsVisibleTime = pred->getVisibleTimeFireForPlanning();
        if (predsVisibleTime > this->actualTimeFire) {
            double diff = predsVisibleTime - this->actualTimeFire;
            this->setActualTimeFire(predsVisibleTime);
            this->setExpectedTimeFire(predsVisibleTime);

            propagateChainInPlanning(shared_from_this(), diff);
        }

        pred->addSuccessorInPlanning(shared_from_this());
    }

    void propagateChainInPlanning(const shared_ptr<Event>& event, double add) {
        for (auto &successor: event->successors) {
            if(successor->getExpectedTimeFire()!= successor->getActualTimeFire()){
                cout<<"!!!!!!!!!!!!!propagate chain - successor different expected and actual times!!!"<<endl;
            }
           
            
            double newTime = successor->getVisibleTimeFireForPlanning() + add;
            successor->setActualTimeFire(newTime);
            successor->setExpectedTimeFire(newTime);

            propagateChainInPlanning(successor, add);
        }
    }

    void addSuccessorInPlanning(const shared_ptr<Event>& succ) {
        assert(succ != nullptr);
        if (succ->id == this->id) {
            throw runtime_error("ADDING OURSELVES AS SUCCESSOR!");
        }
        // cout<<"add successor "<<succ->id<<" to event "<<this->id<<endl;
        this->successors.insert(succ);
        double succsVisibleTime = succ->getVisibleTimeFireForPlanning();
        if (succsVisibleTime < this->expectedTimeFire) {
            double diff = abs(succsVisibleTime - this->expectedTimeFire);
            succ->setActualTimeFire(this->expectedTimeFire);
            succ->setExpectedTimeFire(this->expectedTimeFire);

            propagateChainInPlanning(succ, diff);
        }
        string thisId = this->id;
        auto it = find_if(succ->predecessors.begin(), succ->predecessors.end(),
                          [&thisId](const shared_ptr<Event> obj) { return obj->id == thisId; });

        if (it == succ->predecessors.end()) {
            succ->addPredecessorInPlanning(shared_from_this());
        }
    }

    void setExpectedTimeFire(double d) {
        this->expectedTimeFire = d;
    }

    double getExpectedTimeFire() const {
        return this->expectedTimeFire;
    }

    void setActualTimeFire(double d) {
        const double EPSILON = 1e-9;  // Adjust this if needed
        if (fabs(d - 34783261.252439588) < EPSILON){
            cout<<"";
        }
        if (d != this->actualTimeFire) {
            //   cout << "changing actual time fire for " << this->id << " from " << this->actualTimeFire << " to " << d
            //       << endl;
            this->actualTimeFire = d;
        }
    }

    double getActualTimeFire() {
        return this->actualTimeFire;
    }
    
    double getVisibleTimeFireForPlanning(){
        return this->isDone? this->getActualTimeFire() : this->getExpectedTimeFire();
    }

    void setBothTimesFire(double d) {
        setActualTimeFire(d);
        setExpectedTimeFire(d);
    }

    bool hasCycleFrom(shared_ptr<Event> event, unordered_set<string> &visited, unordered_set<string> &recStack,
                      bool checkPredecessors) {
        if (recStack.find(event->id) != recStack.end()) {
            cout << "Cycle detected at event: " << event->id << endl;
            return true; // Cycle detected!
        }

        if (visited.find(event->id) != visited.end()) {
            return false; // Already checked, no cycle found
        }

        visited.insert(event->id);
        recStack.insert(event->id);

        // Choose to check either predecessors or successors
        const auto &nextEvents = checkPredecessors ? event->predecessors : event->successors;

        for (const auto &next: nextEvents) {
            if (hasCycleFrom(next, visited, recStack, checkPredecessors)) {
                return true;
            }
        }

        recStack.erase(event->id); // Remove from recursion stack after processing
        return false;
    }

    bool checkCycleFromEvent() {
        unordered_set<string> visited;
        unordered_set<string> recStack; // Tracks the current path

        return hasCycleFrom(shared_from_this(), visited, recStack, true) ||
               hasCycleFrom(shared_from_this(), visited, recStack, false);
    }
};


/*struct CompareByTimestamp {
    bool customCharCompare(char a, char b) const {
        if (tolower(a) == tolower(b)) {
            return islower(a) && isupper(b);  // Uppercase is "greater" if letters are same
        }
        if (a == 's' && b == 'f')
            return true;
        if (b == 's' && a == 'f') {
            return false;
        }
        if (a == 'w' && b == 'r') {
            return true;
        }
        if (b == 'w' && a == 'r') {
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
        cout<<"compare "<<a->id<<" and "<< b->id<<endl;
        if (a->getActualTimeFire() != b->getActualTimeFire()) {
            return a->getActualTimeFire() < b->getActualTimeFire();
        }
        if (std::any_of(a->predecessors.begin(), a->predecessors.end(),
                        [&b](const shared_ptr<Event> &pred) { return pred->id == b->id; })) {
           // cout << "Event " << a->id << " is a successor of " << b->id << " => a > b (return false)" << endl;
            return false;
        }
        if (std::any_of(b->predecessors.begin(), b->predecessors.end(),
                        [&a](const shared_ptr<Event> &pred) { return pred->id == a->id; })) {
            //cout << "Event " << a->id << " is a predecessor of " << b->id << " => a < b (return true)" << endl;
            return true;
        }

        bool compare = customIDCompare(a->id, b->id);
        return compare;
    }
}; */


struct CompareByTimestamp {
    int extractTaskNumber(const std::string &taskName) const {
        std::regex rgx("\\d+$");  // Matches digits at the end of the string
        std::smatch match;
        if (std::regex_search(taskName, match, rgx)) {
            return std::stoi(match.str());  // Convert matched string to an integer
        }
        return -1;  // Return -1 if no valid task number is found (shouldn't happen if task names are well-formed)
    }


   /* bool isDeepSuccessor(const shared_ptr<Event> &a, const shared_ptr<Event> &b) const {
        // cout << "isDeepSuccessor " << a->id << " of " << b->id << endl;
        if (std::any_of(a->predecessors.begin(), a->predecessors.end(),
                        [&b](const shared_ptr<Event> &pred) { return pred->id == b->id; })) {
            //   cout << "true" << endl;
            return true;
        }
        for (const auto &pred: a->predecessors) {
            if (isDeepSuccessor(pred, b)) {
                //        cout << "true" << endl;
                return true;
            }
        }
        // cout << "false" << endl;
        return false;
    } */

   bool isDeepSuccessor(const shared_ptr<Event> &a, const shared_ptr<Event> &b, unordered_set<string> &visited) const {
       if (a->id == b->id) {
           return true; // Direct match
       }

       if (visited.count(a->id)) {
           return false; // Already checked this node, no need to repeat work
       }

       visited.insert(a->id); // Mark as visited

       for (const auto &pred : a->predecessors) {
           if (isDeepSuccessor(pred, b, visited)) {
               return true;
           }
       }

       return false;
   }
    bool isDeepSuccessor(const shared_ptr<Event> &a, const shared_ptr<Event> &b) const {
        unordered_set<string> visited;
        return isDeepSuccessor(a, b, visited);
    }


    int compareByTopAndBottomLevels(const shared_ptr<Event> &a, const shared_ptr<Event> &b) const {

    }


    bool operator()(const shared_ptr<Event> a, const shared_ptr<Event> b) const {
        // cout << "compare " << a->id << " and " << b->id << endl;

        // if (a->getActualTimeFire() != b->getActualTimeFire()) {
        //    cout<<"a time "<<a->getActualTimeFire()<<" b time "<<b->getActualTimeFire()<<endl;
        //   double diff = a->getActualTimeFire() - b->getActualTimeFire();
        //  cout << "Time difference: " << diff << endl;
        // return a->getActualTimeFire() < b->getActualTimeFire();
        // }
        if (a->id == "MERGED_BAM_FILTER_00000037-MACS2_00000028-r-f" ||
            b->id == "MERGED_BAM_FILTER_00000037-MACS2_00000028-r-f") {
            cout << "";
        }
        if (a->id == "MACS2_ANNOTATE_00000134-MACS2_QC_00000135-w-f" ||
            b->id == "MACS2_ANNOTATE_00000134-MACS2_QC_00000135-w-f") {
            cout << "";
        }
        if (a->id == "MACS2_ANNOTATE_00000134-f" || b->id == "MACS2_ANNOTATE_00000134-f") {
            cout << "";
        }


        if (a->id == "CHECK_DESIGN_00000049-MACS2_00000028-r-s" ||
            b->id == "CHECK_DESIGN_00000049-MACS2_00000028-r-s") {
            cout << "Comparing " << a->id << " and " << b->id << endl;
            cout << "Deep Successor (a -> b): " << isDeepSuccessor(a, b) << endl;
            cout << "Deep Successor (b -> a): " << isDeepSuccessor(b, a) << endl;
        }


        const double EPSILON = 1e-9;  // Adjust this if needed
        if (fabs(a->getActualTimeFire() - b->getActualTimeFire()) > EPSILON) {
            return a->getActualTimeFire() < b->getActualTimeFire();
        }


        // Direct predecessor/successor check
        if (std::any_of(a->predecessors.begin(), a->predecessors.end(),
                        [&b](const shared_ptr<Event> &pred) { return pred->id == b->id; })) {
            return false;  // a is a successor of b => a should come later
        }

        if (std::any_of(b->predecessors.begin(), b->predecessors.end(),
                        [&a](const shared_ptr<Event> &pred) { return pred->id == a->id; })) {
            return true;  // b is a successor of a => b should come later
        }

        if(a->predecessors.empty() && !b->predecessors.empty()){
            return true;
        }
        if(!a->predecessors.empty() && b->predecessors.empty()){
            return false;
        }


     /*   if (isDeepSuccessor(a, b)) {
            return false;  // a is a deep successor of b, so b must come first
        }

        if (isDeepSuccessor(b, a)) {
            return true;  // b is a deep successor of a, so a must come first
        }
        assert(!(isDeepSuccessor(a, b) && isDeepSuccessor(b, a)) && "Cycle detected in event dependency graph!");
        if (isDeepSuccessor(a, b) && (a->type < b->type)) {
            cout << "Warning: Successor ordering conflicts with type sorting for "
                 << a->id << " and " << b->id << endl;
        } */

        if (a->type != b->type) {
            return a->type < b->type;
        } else {
            if (a->id == b->id) {
                cout << "compare with myself " << a->id << endl;
                return true;
            }
            //   cout<<"ID COMPARE "<<a->id<<" "<<b->id<<endl;
            //  return a->id < b->id;
            if (a->task != NULL && b->task != NULL) {
                if (a->task->top_level != b->task->top_level) {
                    return a->task->top_level > b->task->top_level;
                }
                if (a->task->bottom_level != b->task->bottom_level) {
                    return a->task->bottom_level > b->task->bottom_level;
                }
            }
            if (a->task != NULL && b->edge != NULL) {
                // Case 2: a is task-related, b is edge-related
                if (a->task->top_level != b->edge->tail->top_level) {
                    return a->task->top_level < b->edge->tail->top_level;
                }

            }
            if (a->edge != NULL && b->task != NULL) {
                // Case 3: a is edge-related, b is task-related
                if (a->edge->tail->top_level != b->task->top_level) {
                    return a->edge->tail->top_level < b->task->top_level;
                }
            } else if (a->edge && b->edge) {
                // Case 4: Both are edge-related, compare their top levels via their tails
                if (a->edge->tail->top_level != b->edge->tail->top_level) {
                    return a->edge->tail->top_level < b->edge->tail->top_level;
                }
                // If tails are the same, compare their head tasks
                if (a->edge->head->top_level != b->edge->head->top_level) {
                    return a->edge->head->top_level < b->edge->head->top_level;
                }

                if (a->edge->tail->top_level != b->edge->tail->top_level) {
                    return a->edge->tail->top_level < b->edge->tail->top_level;
                }
                // If tails are the same, compare their head tasks
                if (a->edge->head->top_level != b->edge->head->top_level) {
                    return a->edge->head->top_level < b->edge->head->top_level;
                }
                // If both top levels and head tails are equal, compare by bottom level
                if (a->edge->tail->bottom_level != b->edge->tail->bottom_level) {
                    return a->edge->tail->bottom_level >
                           b->edge->tail->bottom_level;  // larger bottom level comes first
                }

                // If both top levels and head tails are equal, compare by bottom level
                if (a->edge->head->bottom_level != b->edge->head->bottom_level) {
                    return a->edge->head->bottom_level >
                           b->edge->head->bottom_level;  // larger bottom level comes first
                }

            }

            // **Fallback to Task Number Comparison**
            // For task-related events, compare task numbers
            if (a->task && b->task) {
                int taskA = extractTaskNumber(a->task->name);
                int taskB = extractTaskNumber(b->task->name);
                return taskA < taskB;  // Smaller task number comes first
            }

            if (a->task != NULL && b->edge != NULL) {
                // Case 2: a is task-related, b is edge-related
                int taskA = extractTaskNumber(a->task->name);
                int tailB = extractTaskNumber(b->edge->tail->name);
                if (taskA != tailB) {
                    return taskA < tailB;  // Compare based on tail task number
                }

            }
            if (a->edge != NULL && b->task != NULL) {
                // Case 3: a is edge-related, b is task-related
                int tailA = extractTaskNumber(a->edge->tail->name);
                int taskB = extractTaskNumber(b->task->name);
                if (tailA != taskB) {
                    return tailA < taskB;  // Compare based on tail task number
                }
            }


            // For edge-related events, compare tail's task number first, then head's task number
            if (a->edge && b->edge) {
                int tailA = extractTaskNumber(a->edge->tail->name);
                int tailB = extractTaskNumber(b->edge->tail->name);
                if (tailA != tailB) {
                    return tailA < tailB;  // Compare based on tail task number
                }

                // If tail numbers are the same, compare based on head task number
                int headA = extractTaskNumber(a->edge->head->name);
                int headB = extractTaskNumber(b->edge->head->name);
                return headA < headB;  // Compare based on head task number
            }

        }

        cout << "could NOT COMPARE!! " << a->id << " " << b->id << endl;
        return false;
        // return customIDCompare(a->id, b->id);
    }
};


class EventManager {
private:
    std::set<shared_ptr<Event>, CompareByTimestamp> eventSet; // Sorted by timestamp
    std::unordered_map<string, std::set<shared_ptr<Event>>::iterator> eventMap; // Fast lookup by ID
public:
    // Insert a new event
    void insert(const shared_ptr<Event> &event) {
        auto foundIterator = eventMap.find(event->id);
        if (foundIterator != eventMap.end()) {
            remove(event->id);
        } else {
            for (const auto &pred: event->predecessors) {
                if (eventMap.find(pred->id) == eventMap.end()) {
                }
            }
        }
        //printAll();
        auto it = eventSet.insert(event);
        //printAll();
        eventMap[event->id] = it.first;
    }

    shared_ptr<Event> find(string id) {
        if (eventMap.find(id) != eventMap.end()) {
            return *eventMap[id];
        }
        return nullptr;
    }


    // Update an event's timestamp
    bool update(const string &id, double newTimestamp) {
        auto it = eventMap.find(id);
        if (it != eventMap.end() && (*it->second)->getActualTimeFire() != newTimestamp) {
            shared_ptr<Event> updatedEvent = *(it->second);
            eventSet.erase(it->second);

            // Update the timestamp and reinsert
            updatedEvent->setActualTimeFire(newTimestamp);
            auto newIt = eventSet.insert(updatedEvent);

            // Update map entry
            eventMap[id] = newIt.first;
            // checkAllEvents();
            return true;
        }
        //checkAllEvents();
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
    void printAll(int until=-1) const {
        cout << endl;
        int cntr=0;
        for (const auto &event: eventSet) {
           // if(until!=-1 && cntr<until)
            std::cout << "\tID: " << event->id << " at " << event->getActualTimeFire() << endl;// ",\t";
            cntr++;
        }
        cout << endl << "-------";
        cout << endl;
    }

    void printTree() const {
        printHelper(eventSet.begin(), eventSet.end(), 0);
    }


    bool empty() {
        return eventSet.empty();
    }

    void reinsertAll() {
        std::vector<std::shared_ptr<Event>> tempEvents(eventSet.begin(), eventSet.end());
        eventSet.clear();
        eventMap.clear();

        for (const auto &event: tempEvents) {
            auto it = eventSet.insert(event);
            eventMap[event->id] = it.first;
        }
    }

    static void
    printHelper(std::set<shared_ptr<Event>>::const_iterator begin, _Rb_tree_const_iterator<shared_ptr<Event>> end,
                int depth) {
        if (begin == end) return;

        auto middle = begin;
        std::advance(middle, std::distance(begin, end) / 2);

        // Print the current middle node
        for (int i = 0; i < depth; ++i) std::cout << "\t"; // Indentation
        std::cout << (*middle)->id << std::endl;

        // Print left subtree
        printHelper(begin, middle, depth + 1);
        // Print right subtree
        printHelper(std::next(middle), end, depth + 1);
    }


    void reinsertChainForwardFrom(shared_ptr<Event> event, unordered_set<string> &visited) {
        if (visited.find(event->id) != visited.end()) {
            return; // Already processed, avoid redundant updates
        }

        visited.insert(event->id); // Mark as visited

        for (auto &successor: event->successors) {
            successor->setActualTimeFire(max(event->getActualTimeFire() +
                                             std::numeric_limits<double>::epsilon() * event->getActualTimeFire(),
                                             successor->getActualTimeFire()));
            update(successor->id, successor->getActualTimeFire());
            reinsertChainForwardFrom(successor, visited);
        }
    }

    void reinsertChainBackwardFrom(shared_ptr<Event> event, unordered_set<string> &visited) {
        if (visited.find(event->id) != visited.end()) {
            return; // Already processed, avoid redundant updates
        }

        visited.insert(event->id); // Mark as visited

        for (auto &predecessor: event->predecessors) {
            remove(predecessor->id);
            insert(predecessor);
            reinsertChainBackwardFrom(predecessor, visited);

        }
    }
    void deleteAll(){
        eventSet.clear();
        eventMap.clear();
    }

    void checkAllEvents() {
        const double EPSILON = 1e-9;  // Adjust this as needed
        for (const auto &event: eventSet) {
            auto it_event = eventSet.find(event);
            for (const auto &pred: event->predecessors) {
                auto it_pred = eventSet.find(pred);
                // assert(it_pred != eventSet.end() && it_event != eventSet.end() &&
                //       "ERROR: A predecessor is missing in eventSet!");
                if (it_pred == eventSet.end()) {
                    insert(pred);
                }

                if (fabs((*it_event)->getActualTimeFire() - (*it_pred)->getActualTimeFire()) <= EPSILON) {
                    string error = "ERROR: A predecessor " + (*it_pred)->id + " at " +
                                   to_string((*it_pred)->getActualTimeFire()) + " appears after its dependent event! " +
                                   (*it_event)->id + " at " +
                                   to_string((*it_event)->getActualTimeFire());
                    if (std::distance(eventSet.begin(), it_pred) > std::distance(eventSet.begin(), it_event)) {
                        remove((*it_pred)->id);
                        remove((*it_event)->id);
                        // if(!(*it_pred)){
                        // it_pred = eventSet.find(pred);
                        insert(pred);
                        // }
                        //else insert()

                        insert(event);
                        printHelper(eventSet.begin(), eventSet.end(), 0);
                        it_pred = eventSet.find(pred);
                        it_event = eventSet.find(event);
                        if (std::distance(eventSet.begin(), it_pred) > std::distance(eventSet.begin(), it_event)) {
                            //     throw runtime_error(error);
                            cout << error << endl;
                            return;
                        }
                    }
                }

            }

        }
    }
};



struct CompareByRank {
    bool operator()(vertex_t * a,vertex_t * b) const {
        assert(a->rank!=-1);
        assert(b->rank!=-1);
        return a->rank > b->rank;
    }
};

class ReadyQueue{
public:
    std::set<vertex_t*, CompareByRank> readyTasks;
};


#endif