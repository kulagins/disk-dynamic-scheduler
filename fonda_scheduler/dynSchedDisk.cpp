
#include "fonda_scheduler/dynSchedDisk.hpp"

void onTaskFinish(Event event){

    assert(event.type==eventType::OnTaskFinish);
    assert(event.task!=NULL);

    //free memory - not being done

    //incoming edges are nowhere
    for(int j=0; j<event.task->in_degree; j++){
        event.task->in_edges[j]->locations= { Location(LocationType::Nowhere)};
    }
    //set event to ready
    event.task->status = Status::Finished;

    for (auto &successor: event.successors){
        successor.predecessors.erase(
                std::remove_if(
                        successor.predecessors.begin(),
                        successor.predecessors.end(),
                        [event](const Event& event1) {
                            return event1.task->name == event.task->name && event1.expectedTimeFire == event.expectedTimeFire;
                        }
                ),
                successor.predecessors.end()
        );
    }



}