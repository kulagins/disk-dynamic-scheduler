
#ifndef FONDA_SCHED_COMMON_HPP
#define FONDA_SCHED_COMMON_HPP

#include "../../extlibs/memdag/src/graph.hpp"
#include "cluster.hpp"
#include "json.hpp"

#include <regex>
#include <unordered_set>
#include <utility>

class Assignment {

public:
    vertex_t* task;
    Processor* processor;
    double startTime;
    double finishTime;

    Assignment(vertex_t* t, Processor* p, const double st, const double ft)
    {
        this->task = t;
        this->processor = p;
        this->startTime = st;
        this->finishTime = ft;
    }

    [[nodiscard]] nlohmann::json toJson() const
    {
        std::string tn = task->name;
        std::transform(tn.begin(), tn.end(), tn.begin(),
            [](const auto c) { return std::tolower(c); });
        return nlohmann::json {
            { "task", tn },
            { "start", startTime },
            { "machine", processor->id },
            { "finish", finishTime }
        };
    }
};

void printDebug(const std::string& str);

void printInlineDebug(const std::string& str);

void checkForZeroMemories(graph_t* graph);

////void completeRecomputationOfSchedule(Http::ResponseWriter &resp, const json &bodyjson, double timestamp, vertex_t * vertexThatHasAProblem);
void removeSourceAndTarget(graph_t* graph, std::vector<std::pair<vertex_t*, double>>& ranks);

void clearGraph(const graph_t* graphMemTopology);

Cluster*
prepareClusterWithChangesAtTimestamp(const nlohmann::json& bodyjson, double timestamp, std::vector<Assignment*>& tempAssignments);

// void delayOneTask(Http::ResponseWriter &resp, const json &bodyjson, string &nameOfTaskWithProblem, double newStartTime,
//                  Assignment *assignmOfProblem);
void delayEverythingBy(const std::vector<Assignment*>& assignments, const Assignment* startingPoint, double delayTime);

void takeOverChangesFromRunningTasks(const nlohmann::json& bodyjson, graph_t* currentWorkflow, std::vector<Assignment*>& assignments);

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
    std::string id;
    vertex_t* task;
    edge_t* edge;
    eventType type;
    std::shared_ptr<Processor> processor;

    bool onlyPreemptive;
    bool isDone = false;
    int timesFired = 0;

private:
    double expectedTimeFire = -1;
    double actualTimeFire = -1;

    struct EventHasher {
        template <typename EventPtr>
        size_t operator()(const EventPtr& e) const
        {
            return std::hash<std::string>()(e->id); // Use unique id for hashing
        }

        size_t operator()(const std::weak_ptr<Event>& e_) const
        {
            const auto& e = e_.lock();
            if (!e) {
                throw std::runtime_error("Weak pointer to Event is expired or null in EventHasher");
            }
            return std::hash<std::string>()(e->id); // Use unique id for hashing
        }
    };

    struct EventEqual {
        template <class EventPtr>
        bool operator()(const EventPtr& lhs, const EventPtr& rhs) const
        {
            return lhs->id == rhs->id; // Compare by id for equality
        }

        bool operator()(const std::weak_ptr<Event>& lhs_, const std::weak_ptr<Event>& rhs_) const
        {
            const auto& lhs = lhs_.lock();
            const auto& rhs = rhs_.lock();
            return lhs->id == rhs->id; // Compare by id for equality
        }
    };

    std::unordered_set<std::shared_ptr<Event>, EventHasher, EventEqual> predecessors = {};
    std::unordered_set<std::weak_ptr<Event>, EventHasher, EventEqual> successors = {};

    void initialize(const std::vector<std::shared_ptr<Event>>& predecessors,
        const std::vector<std::weak_ptr<Event>>& successors)
    {
        for (auto& pred : predecessors) {
            this->addPredecessorInPlanning(pred);
        }
        for (auto& succ : successors) {
            this->addSuccessorInPlanning(succ);
        }
    }

public:
    Event(vertex_t* task, edge_t* edge,
        const eventType type, const std::shared_ptr<Processor>& processor,
        const double expectedTimeFire, const double actualTimeFire,
        const bool isEviction, std::string idN,
        const std::vector<std::shared_ptr<Event>>& predecessors = {},
        const std::vector<std::weak_ptr<Event>>& successors = {})
        : id(std::move(idN))
        , task(task)
        , edge(edge)
        , type(type)
        , processor(processor) // shared ownership
        , onlyPreemptive(isEviction)
        , expectedTimeFire(expectedTimeFire)
        , actualTimeFire(actualTimeFire)
    {
        initialize(predecessors, successors);
    }

    ~Event()
    {
        removeFromSuccessors(this);
        removeFromPredecessors(this);
    }

    static std::shared_ptr<Event> createEvent(vertex_t* task, edge_t* edge,
        eventType type, const std::shared_ptr<Processor>& processor,
        double expectedTimeFire, double actualTimeFire,
        const std::vector<std::shared_ptr<Event>>& predecessors,
        const std::vector<std::weak_ptr<Event>>& successors,
        bool isEviction, const std::string& id)
    {
        return std::make_shared<Event>(task, edge, type, processor, expectedTimeFire,
            actualTimeFire, isEviction, id, predecessors, successors);
    }

    static std::shared_ptr<Event> createEvent(vertex_t* task, edge_t* edge,
        eventType type, const std::shared_ptr<Processor>& processor,
        double expectedTimeFire, double actualTimeFire,
        bool isEviction, const std::string& id)
    {
        return std::make_shared<Event>(task, edge, type, processor, expectedTimeFire,
            actualTimeFire, isEviction, id);
    }

    auto& getPredecessors()
    {
        return predecessors;
    }

    const auto& getPredecessors() const
    {
        return predecessors;
    }

    auto& getSuccessors()
    {
        return successors;
    }

    const auto& getSuccessors() const
    {
        return successors;
    }

    void fire();

    void fireTaskStart();

    void fireTaskFinish();

    void fireReadStart();

    void fireReadFinish();

    void fireWriteStart();

    void fireWriteFinish();

    static void removeFromSuccessors(Event* us);

    static void removeFromPredecessors(Event* us);

    static void propagateChainInPlanning(const std::shared_ptr<Event>& event, const double add, std::unordered_set<std::shared_ptr<Event>>& visited)
    {
        if (visited.count(event)) {
            return;
        }

        visited.insert(event);

        for (auto& succ_ : event->successors) {
            const auto& successor = succ_.lock();
            if (!successor) {
                throw std::runtime_error("Successor is expired or null in propagateChainInPlanning");
            }
            const double newTime = successor->getVisibleTimeFireForPlanning() + add;
            successor->setActualTimeFire(newTime);
            successor->setExpectedTimeFire(newTime);

            propagateChainInPlanning(successor, add, visited);
        }
    }

    void addPredecessorInPlanning(const std::shared_ptr<Event>& pred)
    {
        if (pred->id == this->id) {
            throw std::runtime_error("ADDING OURSELVES AS PREDECESSOR!");
        }

        if (predecessors.find(pred) != predecessors.end()) {
            // Already a predecessor, no need to add again
            return;
        }

        this->predecessors.insert(pred);

        const double predsVisibleTime = pred->getVisibleTimeFireForPlanning();
        if (predsVisibleTime > this->actualTimeFire) {
            const double diff = predsVisibleTime - this->actualTimeFire;
            this->setActualTimeFire(predsVisibleTime);
            this->setExpectedTimeFire(predsVisibleTime);

            std::unordered_set<std::shared_ptr<Event>> visited;
            propagateChainInPlanning(shared_from_this(), diff, visited);
        }

        pred->addSuccessorInPlanning(shared_from_this());
    }

    // Modified addSuccessorInPlanning to use unordered_set
    void addSuccessorInPlanning(const std::weak_ptr<Event>& succ_)
    {
        const auto& succ = succ_.lock();
        if (!succ) {
            throw std::runtime_error("Successor is expired or null in addSuccessorInPlanning");
        }

        if (succ->id == this->id) {
            throw std::runtime_error("ADDING OURSELVES AS SUCCESSOR!");
        }

        this->successors.insert(succ); // Always insert (either first time or replacing one with same ID)

        // Adjust successor timing if needed
        const double succsVisibleTime = succ->getVisibleTimeFireForPlanning();
        if (succsVisibleTime < this->expectedTimeFire) {
            const double diff = this->expectedTimeFire - succsVisibleTime;
            succ->setActualTimeFire(this->expectedTimeFire);
            succ->setExpectedTimeFire(this->expectedTimeFire);

            std::unordered_set<std::shared_ptr<Event>> visited;
            propagateChainInPlanning(succ, diff, visited);
        }

        // Add this as a predecessor of succ if not already present
        const bool alreadyPredecessor = succ->predecessors.find(shared_from_this()) != succ->predecessors.end();
        if (!alreadyPredecessor) {
            succ->addPredecessorInPlanning(shared_from_this());
        }
    }

    void setExpectedTimeFire(const double d)
    {
        this->expectedTimeFire = d;
    }

    double getExpectedTimeFire() const
    {
        return this->expectedTimeFire;
    }

    void setActualTimeFire(const double d)
    {
        this->actualTimeFire = d;
    }

    double getActualTimeFire() const
    {
        return this->actualTimeFire;
    }

    double getVisibleTimeFireForPlanning() const
    {
        return this->isDone ? this->getActualTimeFire() : this->getExpectedTimeFire();
    }

    void setBothTimesFire(const double d)
    {
        setActualTimeFire(d);
        setExpectedTimeFire(d);
    }

    static bool hasCycleFrom(const std::shared_ptr<Event>& event, std::unordered_set<std::string>& visited, std::unordered_set<std::string>& recStack,
        const bool checkPredecessors)
    {
        if (recStack.find(event->id) != recStack.end()) {
            std::cout << "Cycle detected at event: " << event->id << '\n';
            return true; // Cycle detected!
        }

        if (visited.find(event->id) != visited.end()) {
            return false; // Already checked, no cycle found
        }

        visited.insert(event->id);
        recStack.insert(event->id);

        // Choose to check either predecessors or successors
        if (checkPredecessors) {
            for (const auto& predecessor : event->predecessors) {
                if (hasCycleFrom(predecessor, visited, recStack, checkPredecessors)) {
                    // If a cycle is detected, remove the event from the predecessor/successor list
                    predecessor->successors.erase(event);
                    event->predecessors.erase(predecessor);
                    return true;
                }
            }
        } else {
            for (const auto& successor : event->successors) {
                if (hasCycleFrom(successor.lock(), visited, recStack, checkPredecessors)) {
                    // If a cycle is detected, remove the event from the predecessor/successor list
                    successor.lock()->predecessors.erase(event);
                    event->successors.erase(successor);
                    return true;
                }
            }
        }

        recStack.erase(event->id); // Remove from recursion stack after processing
        return false;
    }

    bool checkCycleFromEvent()
    {
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> recStack; // Tracks the current path

        return hasCycleFrom(shared_from_this(), visited, recStack, true) || hasCycleFrom(shared_from_this(), visited, recStack, false);
    }
};

struct CompareByTimestamp {
    static int extractTaskNumber(const std::string& taskName)
    {
        const std::regex rgx("\\d+$"); // Matches digits at the end of the string
        if (std::smatch match; std::regex_search(taskName, match, rgx)) {
            return std::stoi(match.str()); // Convert matched string to an integer
        }
        return -1; // Return -1 if no valid task number is found (shouldn't happen if task names are well-formed)
    }

    bool operator()(const std::shared_ptr<Event>& a, const std::shared_ptr<Event>& b) const
    {
        static constexpr double EPSILON = 1e-9; // Adjust this if needed
        if (fabs(a->getActualTimeFire() - b->getActualTimeFire()) > EPSILON) {
            return a->getActualTimeFire() < b->getActualTimeFire();
        }

        // Direct predecessor/successor check
        if (std::any_of(a->getPredecessors().begin(), a->getPredecessors().end(),
                [&b](const auto& pred) { return pred->id == b->id; })) {
            return false; // a is a successor of b => a should come later
        }

        if (std::any_of(b->getPredecessors().begin(), b->getPredecessors().end(),
                [&a](const auto& pred) { return pred->id == a->id; })) {
            return true; // b is a successor of a => b should come later
        }

        if (a->getPredecessors().empty() && !b->getPredecessors().empty()) {
            return true;
        }
        if (!a->getPredecessors().empty() && b->getPredecessors().empty()) {
            return false;
        }

        if (a->type != b->type) {
            return a->type < b->type;
        }

        if (a->id == b->id) {
            // cout << "compare with myself " << a->id << endl;
            return true;
        }
        //   cout<<"ID COMPARE "<<a->id<<" "<<b->id<<endl;
        //  return a->id < b->id;
        if (a->task != nullptr && b->task != nullptr) {
            if (a->task->top_level != b->task->top_level) {
                return a->task->top_level > b->task->top_level;
            }
            if (a->task->bottom_level != b->task->bottom_level) {
                return a->task->bottom_level > b->task->bottom_level;
            }
        }
        if (a->task != nullptr && b->edge != nullptr) {
            // Case 2: a is task-related, b is edge-related
            if (a->task->top_level != b->edge->tail->top_level) {
                return a->task->top_level < b->edge->tail->top_level;
            }
        }
        if (a->edge != nullptr && b->task != nullptr) {
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
                return a->edge->tail->bottom_level > b->edge->tail->bottom_level; // larger bottom level comes first
            }

            // If both top levels and head tails are equal, compare by bottom level
            if (a->edge->head->bottom_level != b->edge->head->bottom_level) {
                return a->edge->head->bottom_level > b->edge->head->bottom_level; // larger bottom level comes first
            }
        }

        // **Fallback to Task Number Comparison**
        // For task-related events, compare task numbers
        if (a->task && b->task) {
            const int taskA = extractTaskNumber(a->task->name);
            const int taskB = extractTaskNumber(b->task->name);
            if (taskA != taskB) {
                return taskA < taskB; // Smaller task number comes first
            }
            return a->id < b->id;
        }

        if (a->task != nullptr && b->edge != nullptr) {
            // Case 2: a is task-related, b is edge-related
            const int taskA = extractTaskNumber(a->task->name);
            const int tailB = extractTaskNumber(b->edge->tail->name);
            if (taskA != tailB) {
                return taskA < tailB; // Compare based on tail task number
            }
        }
        if (a->edge != nullptr && b->task != nullptr) {
            // Case 3: a is edge-related, b is task-related
            const int tailA = extractTaskNumber(a->edge->tail->name);
            const int taskB = extractTaskNumber(b->task->name);
            if (tailA != taskB) {
                return tailA < taskB; // Compare based on tail task number
            }
        }

        // For edge-related events, compare tail's task number first, then head's task number
        if (a->edge && b->edge) {
            const int tailA = extractTaskNumber(a->edge->tail->name);
            const int tailB = extractTaskNumber(b->edge->tail->name);
            if (tailA != tailB) {
                return tailA < tailB; // Compare based on tail task number
            }

            // If tail numbers are the same, compare based on head task number
            const int headA = extractTaskNumber(a->edge->head->name);
            const int headB = extractTaskNumber(b->edge->head->name);
            if (headA != headB) {
                return headA < headB; // Compare based on head task number
            }
            return a->id < b->id;
        }
        return a->id < b->id;
        // return customIDCompare(a->id, b->id);
    }
};

class EventManager {
private:
    std::set<std::shared_ptr<Event>, CompareByTimestamp> eventSet; // Sorted by timestamp
    std::unordered_map<std::string, std::set<std::shared_ptr<Event>>::iterator> eventByIdMap; // Fast lookup by ID
    std::unordered_map<int, std::set<std::shared_ptr<Event>, CompareByTimestamp>> eventsByProcessorIdMap;

public:
    // Insert a new event
    void insert(const Event* event_ptr)
    {
        const auto event = std::shared_ptr<Event>(const_cast<Event*>(event_ptr), [](const Event* evt) { delete evt; });
        insert(event);
    }

    void insert(const std::shared_ptr<Event>& event)
    {
        // cout << "inserting event " << event->id << endl;
        const auto foundIterator = eventByIdMap.find(event->id);
        if (foundIterator != eventByIdMap.end()) {
            remove(event->id);
            return;
        }

        // auto it = eventSet.insert(event);
        auto [it, inserted] = eventSet.insert(event);
        if (!inserted) {
            std::cout << "Event " << event->id << " was NOT inserted. Conflicted with: " << (*it)->id << '\n';
        }
        eventByIdMap[event->id] = it;
        eventsByProcessorIdMap[event->processor->id].insert(*it);
    }

    std::shared_ptr<Event> findByEventId(const std::string& id)
    {
        const auto it = eventByIdMap.find(id);
        return (it != eventByIdMap.end()) ? *(it->second) : nullptr; // Return the event if found, else nullptr
    }

    const std::set<std::shared_ptr<Event>, CompareByTimestamp>& findByProcessorId(const int processorId)
    {
        static const std::set<std::shared_ptr<Event>, CompareByTimestamp> emptySet; // Constant empty set with the same comparator
        const auto it = eventsByProcessorIdMap.find(processorId);
        if (it != eventsByProcessorIdMap.end()) {
            return it->second; // Return the const set of events for that processor
        }
        return emptySet; // Return an empty const set with the same comparator
    }

    bool update(const std::string& id, const double newTimestamp)
    {
        const auto it = eventByIdMap.find(id);
        if (it == eventByIdMap.end()) {
            return false;
        }

        const auto event = *(it->second);

        if (event->getExpectedTimeFire() == newTimestamp) {
            return false;
        }

        // Remove from eventSet
        eventSet.erase(it->second);

        // Remove from processor map
        eventsByProcessorIdMap[event->processor->id].erase(event);

        // Update the timestamp
        event->setActualTimeFire(newTimestamp);

        // Reinsert updated event
        auto [newIt, inserted] = eventSet.insert(event);
        if (!inserted) {
            std::cerr << "Failed to reinsert updated event: " << id << '\n';
            return false;
        }

        // Update the ID map and processor map
        eventByIdMap[id] = newIt;
        eventsByProcessorIdMap[event->processor->id].insert(event);
        return true;
    }

    bool remove(const std::string& id)
    {
        const auto it = eventByIdMap.find(id);
        if (it == eventByIdMap.end()) {
            return false;
        }

        const auto event = *(it->second);
        const int pid = event->processor->id;

        const auto processorIt = eventsByProcessorIdMap.find(pid);
        if (processorIt != eventsByProcessorIdMap.end()) {
            processorIt->second.erase(event);
            if (processorIt->second.empty()) {
                eventsByProcessorIdMap.erase(pid); // optional cleanup
            }
        }

        eventSet.erase(it->second);
        eventByIdMap.erase(it);

        return true;
    }

    // Get the earliest event (smallest timestamp)
    [[nodiscard]] std::shared_ptr<Event> getEarliest() const
    {
        if (!eventSet.empty()) {
            return *eventSet.begin();
        }
        return nullptr; // Empty set
    }

    // Print all events (for debugging)
    void printAll(int until = -1) const
    {
        std::cout << '\n';
        int cntr = 0;
        for (const auto& event : eventSet) {
            // if(until!=-1 && cntr<until)
            std::cout << "\tID: " << event->id << " at " << event->getActualTimeFire() << '\n'; // ",\t";
            cntr++;
        }
        std::cout << '\n'
                  << "-------";
        std::cout << '\n';
    }

    [[nodiscard]] bool empty() const
    {
        return eventSet.empty();
    }

    static void
    printHelper(const std::set<std::shared_ptr<Event>>::const_iterator begin, const std::set<std::shared_ptr<Event>>::const_iterator end,
        const int depth)
    {
        if (begin == end)
            return;

        auto middle = begin;
        std::advance(middle, std::distance(begin, end) / 2);

        // Print the current middle node
        for (int i = 0; i < depth; ++i)
            std::cout << "\t"; // Indentation
        std::cout << (*middle)->id << '\n';

        // Print left subtree
        printHelper(begin, middle, depth + 1);
        // Print right subtree
        printHelper(std::next(middle), end, depth + 1);
    }

    void reinsertChainForwardFrom(const std::shared_ptr<Event>& event, std::unordered_set<std::string>& visited)
    {
        if (visited.find(event->id) != visited.end()) {
            return; // Already processed, avoid redundant updates
        }

        visited.insert(event->id); // Mark as visited

        for (auto& succ_ : event->getSuccessors()) {
            const auto& successor = succ_.lock();
            if (!successor) {
                throw std::runtime_error("Successor is expired or null in reinsertChainForwardFrom");
            }
            successor->setActualTimeFire(std::max(event->getActualTimeFire() + std::numeric_limits<double>::epsilon() * event->getActualTimeFire(),
                successor->getActualTimeFire()));
            update(successor->id, successor->getActualTimeFire());
            reinsertChainForwardFrom(successor, visited);
        }
    }

    void reinsertChainBackwardFrom(const std::shared_ptr<Event>& event, std::unordered_set<std::string>& visited)
    {
        if (visited.find(event->id) != visited.end()) {
            return; // Already processed, avoid redundant updates
        }

        visited.insert(event->id); // Mark as visited

        for (const auto& predecessor : event->getPredecessors()) {
            if (!predecessor) {
                throw std::runtime_error("Predecessor is expired or null in reinsertChainBackwardFrom");
            }
            remove(predecessor->id);
            insert(predecessor);
            reinsertChainBackwardFrom(predecessor, visited);
        }
    }

    void deleteAll()
    {
        eventSet.clear();
        eventByIdMap.clear();
        eventsByProcessorIdMap.clear();
    }

    void checkAllEvents()
    {
        static constexpr double EPSILON = 1e-9; // Adjust this as needed
        for (const auto& event : eventSet) {
            auto it_event = eventSet.find(event);
            for (const auto& pred : event->getPredecessors()) {
                auto it_pred = eventSet.find(pred);
                // assert(it_pred != eventSet.end() && it_event != eventSet.end() &&
                //       "ERROR: A predecessor is missing in eventSet!");
                if (it_pred == eventSet.end()) {
                    insert(pred);
                }

                if (fabs((*it_event)->getActualTimeFire() - (*it_pred)->getActualTimeFire()) <= EPSILON) {
                    const std::string error = "ERROR: A predecessor " + (*it_pred)->id + " at " + std::to_string((*it_pred)->getActualTimeFire()) + " appears after its dependent event! " + (*it_event)->id + " at " + std::to_string((*it_event)->getActualTimeFire());
                    if (std::distance(eventSet.begin(), it_pred) > std::distance(eventSet.begin(), it_event)) {
                        remove((*it_pred)->id);
                        remove((*it_event)->id);
                        // if(!(*it_pred)){
                        // it_pred = eventSet.find(pred);
                        insert(pred);
                        // }
                        // else insert()

                        insert(event);
                        printHelper(eventSet.begin(), eventSet.end(), 0);
                        it_pred = eventSet.find(pred);
                        it_event = eventSet.find(event);
                        if (std::distance(eventSet.begin(), it_pred) > std::distance(eventSet.begin(), it_event)) {
                            //     throw runtime_error(error);
                            std::cout << error << '\n';
                            return;
                        }
                    }
                }
            }
        }
    }
};

struct CompareByRank {
    bool operator()(const vertex_t* a, const vertex_t* b) const
    {
        assert(a->rank != -1);
        assert(b->rank != -1);
        return a->rank > b->rank;
    }
};

class ReadyQueue {
public:
    std::set<vertex_t*, CompareByRank> readyTasks;
};

#endif