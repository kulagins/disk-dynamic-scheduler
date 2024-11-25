#include "fonda_scheduler/dynSched.hpp"

#include <iterator>




double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(const vertex_t *v, const shared_ptr<Processor>pj) {
    assert(pj->availableMemory >= 0);
    double Res = pj->availableMemory - peakMemoryRequirementOfVertex(v);
    return Res;
}

double new_heuristic(graph_t *graph, Cluster *cluster, bool isHeft){
    vector<pair<vertex_t *, double>> ranks = calculateBottomLevels(graph, 1);
    removeSourceAndTarget(graph, ranks);
    sort(ranks.begin(), ranks.end(),
         [](pair<vertex_t *, double> a, pair<vertex_t *, double> b) {
            return a.second > b.second;

    });
    double makespan=0;

    for (auto &pair: ranks){
        auto vertex = pair.first;
        cout<<"processing "<< vertex->name<<endl;
        vector<shared_ptr<Processor>> bestModifiedProcs;
        shared_ptr<Processor> bestProcessorToAssign;
        double bestFinishTime = std::numeric_limits<double>::max();
        double bestStartTime =0; int resultingVar;
        edge_t * besttoKick;
        for (const shared_ptr<Processor>& processor: cluster->getProcessors()){

            double finTime=0, startTime=0, peakMem=0;
            double ftBefore = processor->readyTimeCompute;
            int resultingVariant;
            auto ourModifiedProc = make_shared<Processor>(*processor);
            edge* toKick;
            vector<shared_ptr<Processor>> modifiedProcs = tentativeAssignment(vertex, ourModifiedProc,
                                                                                     finTime, startTime,
                                                                                     peakMem, resultingVariant, toKick, cluster, isHeft);
            cout<<"start "<<startTime<<" end "<<finTime<<endl;
            if(bestFinishTime> finTime){
                    bestModifiedProcs= modifiedProcs;
                    bestFinishTime= finTime; bestStartTime = startTime;
                    bestProcessorToAssign = ourModifiedProc;
                    resultingVar = resultingVariant;
                    besttoKick = toKick;
            }
            else{
                if(bestFinishTime==finTime){
                    if(bestProcessorToAssign && ourModifiedProc->getMemorySize()>bestProcessorToAssign->getMemorySize()){
                        cout<<"new best proc due to more mem"<<endl;
                        bestModifiedProcs= modifiedProcs;
                        bestFinishTime= finTime; bestStartTime = startTime;
                        bestProcessorToAssign = ourModifiedProc;
                        resultingVar = resultingVariant;
                        besttoKick = toKick;
                    }
                }
                assert(ftBefore == processor->readyTimeCompute);
            }
        }
        cout<<"best "<<bestStartTime<<" "<<bestFinishTime<<endl;

        if(bestModifiedProcs.empty()){
            cout<<"Invalid assignment"<<endl;
            return -1;
        }


        switch (resultingVar) {
            case 1:
                break;
            case 2:
                assert(besttoKick!=NULL);
                bestProcessorToAssign->delocateToDisk(besttoKick);
                break;
            case 3:
                for(auto it= bestProcessorToAssign->pendingMemories.begin();
                                            it!= bestProcessorToAssign->pendingMemories.end();){
                    it = bestProcessorToAssign->delocateToDisk(*it);
                    cout<<"end deloc"<<endl;
                }
                bestProcessorToAssign->pendingMemories.clear();
                break;
        }

        for (auto &modifiedProc: bestModifiedProcs){
            cout<<"modified proc "<<modifiedProc->id<<endl;
            auto it = std::find_if(cluster->processors.begin(), cluster->processors.end(),
                                   [&modifiedProc](std::shared_ptr<Processor> proc) {
                                       return proc->id == modifiedProc->id;
                                   });
            // If found, replace it with the modified version
            if (it != cluster->processors.end()) {
                if( (*it)->id!= bestProcessorToAssign->id &&it.operator*()->pendingMemories.size()!= modifiedProc->pendingMemories.size()){
                    auto processorFromCluster = *it;
                    cout<<it.operator*()->pendingMemories.size()<<" "<<modifiedProc->pendingMemories.size()<<endl;

                    cout<<"Pending on cluster proc: "<<endl;
                    for (auto pendInCl: (*it)->pendingMemories){
                        print_edge(pendInCl);
                    }

                    cout<<"Pending on modified proc: "<<endl;
                    for (auto pendInCl: modifiedProc->pendingMemories){
                        print_edge(pendInCl);
                    }


                    for(auto pendingInCluster= processorFromCluster->pendingMemories.begin();
                        pendingInCluster!= processorFromCluster->pendingMemories.end();){
                        print_edge(*pendingInCluster);
                        auto i = std::find_if(modifiedProc->pendingMemories.begin(),
                                              modifiedProc->pendingMemories.end(),
                                              [&pendingInCluster](auto pendInModif) {
                                                  return pendInModif==*pendingInCluster;
                                              });
                        if(i==modifiedProc->pendingMemories.end()){
                            pendingInCluster= processorFromCluster->delocateToDisk(*pendingInCluster);
                        }
                        else pendingInCluster++;
                    }

                }
                *it = modifiedProc;
            }
            else{
                cout<<"Not found proc by id "<<modifiedProc->id<<endl;
            }
        }
        vertex->assignedProcessorId= bestProcessorToAssign->id;
        for (int j = 0; j < vertex->in_degree; j++) {
            edge *ine = vertex->in_edges[j];
            cout<<"ine ";print_edge(ine);
            for (auto location: ine->locations){
                if(location.locationType==LocationType::OnProcessor ){ //&& location.processorId.value()!=bestProcessorToAssign->id
                    assert(location.processorId.has_value());
                    shared_ptr<Processor> otherProc = cluster->getProcessorById(location.processorId.value());
                    auto it = std::find_if(otherProc->pendingMemories.begin(), otherProc->pendingMemories.end(),
                                           [&ine](edge_t *e) {
                                               return e->head->id == ine->head->id && e->tail->id == ine->tail->id;
                                           });
                    if(it != otherProc->pendingMemories.end()){
                        otherProc->delocateToDisk(ine);
                    }
                    else{
                        cout<<"NOT FOUND ";
                        print_edge(ine);
                        cout<<"IN ";
                        for ( auto edge: otherProc->pendingMemories){
                            print_edge(edge);
                        }
                        cout<<endl;
                        cout<<"ON PROC "<<otherProc->id<<endl;
                        throw new runtime_error("not found in mems");
                    }
                }
            }
            ine->locations.clear();//.emplace_back(LocationType::OnProcessor, bestProcessorToAssign->id);
        }

        cout<<"emplacing out edges ";
        for(int i=0; i<vertex->out_degree; i++) {
            auto v1 = vertex->out_edges[i];
            print_edge(v1);
          //  bestProcessorToAssign->pendingMemories.emplace(v1);
            //v1->locations.emplace_back(LocationType::OnProcessor, bestProcessorToAssign->id);
            //locateToThisProcessorFromNowhere(v1, bestProcessorToAssign->id);
            cout<<bestProcessorToAssign->availableMemory<<endl;
            bestProcessorToAssign->loadFromNowhere(v1);
           // cout<<bestProcessorToAssign->availableMemory<<" ";
        }
        //for (int j = 0; j < vertex->out_degree; j++) {
       //     edge *oute = vertex->out_edges[j];
      //      oute->locations.emplace_back(LocationType::OnProcessor, bestProcessorToAssign->id);
     //   }
        vertex->makespan= bestFinishTime;
        assert(bestStartTime<bestFinishTime);
        //cout<<bestStartTime<<" "<<bestFinishTime<<endl;
        cluster->printProcessors();



        if(makespan<bestFinishTime)
            makespan= bestFinishTime;


    }
    return makespan;
}


vector<shared_ptr<Processor>>
tentativeAssignment(vertex_t *v, shared_ptr<Processor> ourModifiedProc,  double &finishTime, double &startTime,
                    double &peakMem, int& resultingvariant, edge * &toKick, Cluster * cluster, bool isThisBaseline) {

    //TODO CHECK SETTING AVAIL MEM and such
    cout<<"tent on proc "<<ourModifiedProc->id<< " ";
    resultingvariant=1;

    double sumOut=0;
    for (int i = 0; i < v->out_degree; i++) {
        sumOut += v->out_edges[i]->weight;
    }

    if(ourModifiedProc->getMemorySize()<sumOut){
        finishTime= std::numeric_limits<double>::max();
        return {};
    }
    vector<std::shared_ptr<Processor>  > modifiedProcs;

    modifiedProcs.emplace_back(ourModifiedProc);


    processIncomingEdges(v, ourModifiedProc, modifiedProcs, startTime, cluster);

    startTime = ourModifiedProc->readyTimeCompute> startTime? ourModifiedProc->readyTimeCompute: startTime;

    double Res = howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(v, ourModifiedProc);
    peakMem = (Res<0)? 1:(ourModifiedProc->getMemorySize()-Res)/ourModifiedProc->getMemorySize();

    if(Res <0){
        //try finish times with and without memory overflow
        double amountToOffload = -Res;
        double shortestFT= std::numeric_limits<double>::max();

        double timeToFinishNoEvicted = startTime+ v->time/ ourModifiedProc->getProcessorSpeed() + amountToOffload / ourModifiedProc->memoryOffloadingPenalty;
        assert(timeToFinishNoEvicted>startTime);
        double timeToFinishBiggestEvicted = std::numeric_limits<double>::max(),
        timeToFinishAllEvicted = std::numeric_limits<double>::max() ;
        double timeToWriteAllPending = 0;

        vector<edge_t *> penMemsAsVector = getBiggestPendingEdge(ourModifiedProc);
        if(!penMemsAsVector.empty()) {
            auto biggestFileWeight = (*penMemsAsVector.begin())->weight;
            double amountToOffloadWithoutBiggestFile = (amountToOffload - biggestFileWeight) > 0 ? (amountToOffload -
                                                                                                    biggestFileWeight) : 0 ;
            startTime = ourModifiedProc->readyTimeWrite> startTime?
                        ourModifiedProc->readyTimeWrite: startTime;
            double finishTimeToWrite = ourModifiedProc->readyTimeWrite +
                                       biggestFileWeight / ourModifiedProc->writeSpeedDisk;
            startTime = max(startTime, finishTimeToWrite);
            timeToFinishBiggestEvicted =
                    startTime
                    + v->time / ourModifiedProc->getProcessorSpeed() +
                    amountToOffloadWithoutBiggestFile / ourModifiedProc->memoryOffloadingPenalty;
            assert(timeToFinishBiggestEvicted>startTime);



            double sumWeightsOfAllPending=0;
            for (const auto &item: ourModifiedProc->pendingMemories) {
                timeToWriteAllPending += item->weight / ourModifiedProc->writeSpeedDisk;
                sumWeightsOfAllPending+= item->weight;
            }

            double amountToOffloadWithoutAllFiles = (ourModifiedProc->getMemorySize() - biggestFileWeight > 0) ? 0 :
                                                    biggestFileWeight - ourModifiedProc->getMemorySize();

            finishTimeToWrite = ourModifiedProc->readyTimeWrite +
                                timeToWriteAllPending;
            startTime = max(startTime, finishTimeToWrite);
            timeToFinishAllEvicted = startTime + v->time / ourModifiedProc->getProcessorSpeed() +
                                     amountToOffloadWithoutAllFiles / ourModifiedProc->memoryOffloadingPenalty;
            assert(timeToFinishAllEvicted>startTime);
        }

        double minTTF = min(timeToFinishNoEvicted, min(timeToFinishBiggestEvicted, timeToFinishAllEvicted));
        assert(minTTF>startTime);
        ourModifiedProc->readyTimeCompute = minTTF;


        if(timeToFinishBiggestEvicted == minTTF){
            toKick = (*penMemsAsVector.begin());
            cout<<"best tentative with biggest Evicted "; print_edge(toKick);
            resultingvariant=2;
            ourModifiedProc->readyTimeWrite +=
                    (*penMemsAsVector.begin())->weight / ourModifiedProc->writeSpeedDisk;
            // ourModifiedProc->pendingMemories.erase()
            //penMemsAsVector.erase(penMemsAsVector.begin());

            assert(toKick!=nullptr);
            assert(!toKick->locations.empty());
            assert(isLocatedOnThisProcessor(toKick, ourModifiedProc->id));
        }
        if(timeToFinishAllEvicted==minTTF){
            resultingvariant=3;
            cout<<"best tentative with all Evicted ";
            ourModifiedProc->readyTimeWrite += timeToWriteAllPending;
            //penMemsAsVector.resize(0);
        }

        if(isThisBaseline){
            ourModifiedProc->readyTimeCompute = timeToFinishNoEvicted;
        }
        finishTime= ourModifiedProc->readyTimeCompute;
    }
    else{
        //startTime =  ourModifiedProc->readyTimeCompute;
        // printInlineDebug("should be successful");
        ourModifiedProc->readyTimeCompute= startTime + v->time/ ourModifiedProc->getProcessorSpeed();
        finishTime= ourModifiedProc->readyTimeCompute;

    }
    assert(finishTime> startTime);
    return modifiedProcs;
}


void
processIncomingEdges(const vertex_t *v, shared_ptr<Processor> &ourModifiedProc, vector<std::shared_ptr<Processor>> &modifiedProcs,
                     double &earliestStartingTimeToComputeVertex, Cluster * cluster) {
    earliestStartingTimeToComputeVertex = ourModifiedProc->readyTimeCompute;
    for (int j = 0; j < v->in_degree; j++) {
        edge *incomingEdge = v->in_edges[j];
        vertex_t *predecessor = incomingEdge->tail;

        if (predecessor->assignedProcessorId == ourModifiedProc->id) {
            if(!isLocatedOnThisProcessor(incomingEdge, ourModifiedProc->id)){
                assert(isLocatedOnDisk(incomingEdge));
                ourModifiedProc->readyTimeRead += incomingEdge->weight / ourModifiedProc->readSpeedDisk;
                earliestStartingTimeToComputeVertex =  ourModifiedProc->readyTimeRead>earliestStartingTimeToComputeVertex?
                                                       ourModifiedProc->readyTimeRead: earliestStartingTimeToComputeVertex;
            }

        }
        else{
            if (isLocatedOnDisk(incomingEdge)) {
                //we need to schedule read
                ourModifiedProc->readyTimeRead += incomingEdge->weight / ourModifiedProc->readSpeedDisk;
                earliestStartingTimeToComputeVertex =  ourModifiedProc->readyTimeRead>earliestStartingTimeToComputeVertex?
                                                       ourModifiedProc->readyTimeRead: earliestStartingTimeToComputeVertex;
                //TODO evict??
            } else {
                auto predecessorsProcessorsId = predecessor->assignedProcessorId;
                assert(isLocatedOnThisProcessor(incomingEdge, predecessorsProcessorsId));
                shared_ptr<Processor>  addedProc;
                auto it = std::find_if(modifiedProcs.begin(), modifiedProcs.end(),
                                      [predecessorsProcessorsId](shared_ptr<Processor> p) {
                                          return p->id == predecessorsProcessorsId;
                                      });
                if(it==modifiedProcs.end()){
                   addedProc = make_shared<Processor>(*cluster->getProcessorById(predecessorsProcessorsId));
                }
                else{
                    addedProc = *it;
                }

                modifiedProcs.emplace_back(addedProc);

                double timeToStartWriting= max(predecessor->makespan, addedProc->readyTimeWrite);
                addedProc->readyTimeWrite= timeToStartWriting+ incomingEdge->weight / addedProc->writeSpeedDisk;
                double startTimeOfRead = max(addedProc->readyTimeWrite, ourModifiedProc->readyTimeRead);
                ourModifiedProc->readyTimeRead = startTimeOfRead + incomingEdge->weight / ourModifiedProc->readSpeedDisk;
                earliestStartingTimeToComputeVertex =  ourModifiedProc->readyTimeRead>earliestStartingTimeToComputeVertex?
                                                       ourModifiedProc->readyTimeRead: earliestStartingTimeToComputeVertex;
                int addpl  = addedProc->pendingMemories.size();
                addedProc->pendingMemories.erase(incomingEdge);
                cout<<"other proc "<<addedProc->id<<" evicting edge "; print_edge(incomingEdge);
                assert(addpl> addedProc->pendingMemories.size());
            }
        }
    }
}



bool isLocatedNowhere(edge_t* edge){
    auto it = std::find_if(edge->locations.begin(), edge->locations.end(),
                           [](Location location) {
                               return location.locationType == LocationType::Nowhere;
                           });
    return edge->locations.empty() || it != edge->locations.end();
}

bool isLocatedOnDisk(edge_t* edge){
    cout<<"is loc on disk"<<endl;
    return std::find_if(edge->locations.begin(), edge->locations.end(),
                 [](Location location) {
                     return location.locationType== LocationType::OnDisk;
                 })
    != edge->locations.end();
}
bool isLocatedOnThisProcessor(edge_t* edge, int id){
    return std::find_if(edge->locations.begin(), edge->locations.end(),
                        [id](Location location) {
                            return location.locationType== LocationType::OnProcessor && location.processorId==id;
                        })
           != edge->locations.end();
}

void delocateFromThisProcessorToDisk(edge_t* edge, int id){
    auto locationOnThisProcessor = std::find_if(edge->locations.begin(), edge->locations.end(),
                                                [id](Location location) {
                                                    return location.locationType == LocationType::OnProcessor &&
                                                           location.processorId == id;
                                                });
   cout<<"delocating "; print_edge(edge);
   assert(locationOnThisProcessor  != edge->locations.end());
   edge->locations.erase(locationOnThisProcessor);
   if(!isLocatedOnDisk(edge))
        edge->locations.emplace_back(Location(LocationType::OnDisk));


}

void locateToThisProcessorFromDisk(edge_t* edge, int id){
    cout<<"locating to proc "<<id <<" edge "; print_edge(edge);
    assert(isLocatedOnDisk(edge));
    auto locationOnDisk = std::find_if(edge->locations.begin(), edge->locations.end(),
                                                [](Location location) {
                                                    return location.locationType == LocationType::OnDisk;
                                                });
    edge->locations.erase(locationOnDisk);
    if(!isLocatedOnThisProcessor(edge, id))
        edge->locations.emplace_back(LocationType::OnProcessor, id);
}

void locateToThisProcessorFromNowhere(edge_t* edge, int id){
    cout<<"locating from nowhere to proc "<<id <<" edge "; print_edge(edge);
    if(!isLocatedOnThisProcessor(edge, id))
        edge->locations.emplace_back(LocationType::OnProcessor, id);
}





vector<edge_t *> getBiggestPendingEdge(shared_ptr<Processor> pj) {
    vector<edge_t*> penMemsAsVector;//(pj->pendingMemories.size());
    penMemsAsVector.reserve(pj->pendingMemories.size());
    for (edge_t * e: pj->pendingMemories){
        penMemsAsVector.emplace_back(e);
    }
    sort(penMemsAsVector.begin(), penMemsAsVector.end(),[](edge_t* a, edge_t*b) {
            if(a->weight==b->weight){
                if(a->head->id==b->head->id)
                    return a->tail->id< b->tail->id;
                else return a->head->id>b->head->id;
            }
            else
            return a->weight>b->weight;
    });
    return penMemsAsVector;
}



graph_t *convertToNonMemRepresentation(graph_t *withMemories, map<int, int> &noMemToWithMem) {
    enforce_single_source_and_target(withMemories);
    graph_t *noNodeMemories = new_graph();

    for (vertex_t *vertex = withMemories->source; vertex; vertex = next_vertex_in_sorted_topological_order(withMemories,
                                                                                                           vertex,
                                                                                                           &sort_by_increasing_top_level)) {
        vertex_t *invtx = new_vertex(noNodeMemories, vertex->name + "-in", vertex->time, NULL);
        noMemToWithMem.insert({invtx->id, vertex->id});
        if (!noNodeMemories->source) {
            noNodeMemories->source = invtx;
        }
        vertex_t *outvtx = new_vertex(noNodeMemories, vertex->name + "-out", 0.0, NULL);
        noMemToWithMem.insert({outvtx->id, vertex->id});
        edge_t *e = new_edge(noNodeMemories, invtx, outvtx, vertex->memoryRequirement, NULL);
        noNodeMemories->target = outvtx;

        for (int i = 0; i < vertex->in_degree; i++) {
            edge *inEdgeOriginal = vertex->in_edges[i];
            string expectedName = inEdgeOriginal->tail->name + "-out";
            vertex_t *outVtxOfCopiedInVtxOfEdge = findVertexByName(noNodeMemories, expectedName);

            if (outVtxOfCopiedInVtxOfEdge == NULL) {
                print_graph_to_cout(noNodeMemories);
                outVtxOfCopiedInVtxOfEdge = findVertexByName(noNodeMemories, expectedName);
                cout << "expected: " << expectedName << endl;
                throw std::invalid_argument(" no vertex found for expected name.");

            }
            edge_t *e_new = new_edge(noNodeMemories, outVtxOfCopiedInVtxOfEdge, invtx, inEdgeOriginal->weight, NULL);
        }
    }

    return noNodeMemories;
}

double calculateSimpleBottomUpRank(vertex_t *task) {
//    cout<<"rank for "<<task->name<<" ";



    double maxCost = 0.0;
    for (int j = 0; j < task->out_degree; j++) {
        double communicationCost = task->out_edges[j]->weight;
        // cout<<communicationCost<<" ";
        if(task->out_edges[j]->head->bottom_level==-1){
            //cout<<"-1"<<endl;
            task->out_edges[j]->head->bottom_level = calculateSimpleBottomUpRank(task->out_edges[j]->head);
            // cout<<"then "<<task->out_edges[j]->head->bottom_level<<endl;
        }
        double successorCost = task->out_edges[j]->head->bottom_level; //calculateSimpleBottomUpRank(task->out_edges[j]->head);
        double cost = communicationCost + successorCost;
        maxCost = max(maxCost, cost);
    }
    //cout<<endl;
    double retur = (task->time + maxCost);
    task->bottom_level=retur;
    // cout<<"result "<<retur<<endl;
    return retur;
}

double calculateBLCBottomUpRank(vertex_t *task) {

    double maxCost = 0.0;
    for (int j = 0; j < task->out_degree; j++) {
        double communicationCost = task->out_edges[j]->weight;
        double successorCost = calculateBLCBottomUpRank(task->out_edges[j]->head);
        double cost = communicationCost + successorCost;
        maxCost = max(maxCost, cost);
    }
    double simpleBl = task->time + maxCost;

    double maxInputCost=0.0;
    for (int j = 0; j < task->in_degree; j++) {
        double communicationCost = task->in_edges[j]->weight;
        maxInputCost = max(maxInputCost, communicationCost);
    }
    double retur = simpleBl + maxInputCost;
    return retur;
}

std::vector < std::pair< vertex_t *, double> >  calculateMMBottomUpRank(graph_t * graphWMems){

    map<int, int> noMemToWithMem;
    graph_t *graph = convertToNonMemRepresentation(graphWMems, noMemToWithMem);
    // print_graph_to_cout(graph);

    SP_tree_t *sp_tree = NULL;
    graph_t *sp_graph = NULL;

    enforce_single_source_and_target(graph);
    sp_tree = build_SP_decomposition_tree(graph);
    if (sp_tree) {
        sp_graph = graph;
    } else {
        sp_graph = graph_sp_ization(graph);
        sp_tree = build_SP_decomposition_tree(sp_graph);
    }


    std::vector < std::pair< vertex_t *, int> > scheduleOnOriginal;

    if (sp_tree) {
        vertex_t **schedule = compute_optimal_SP_traversal(sp_graph, sp_tree);

        for(int i=0; i<sp_graph->number_of_vertices; i++){
            vertex_t *vInSp = schedule[i];
            //cout<<vInSp->name<<endl;
            const map<int, int>::iterator &it = noMemToWithMem.find(vInSp->id);
            if (it != noMemToWithMem.end()) {
                vertex_t *vertexWithMem = graphWMems->vertices_by_id[(*it).second];
                if (std::find_if(scheduleOnOriginal.begin(), scheduleOnOriginal.end(),
                                 [vertexWithMem](std::pair<vertex_t *, int> p) {
                                     return p.first->name == vertexWithMem->name;
                                 }) == scheduleOnOriginal.end()) {
                    scheduleOnOriginal.emplace_back(vertexWithMem, sp_graph->number_of_vertices-i);// TODO: #vertices - i?
                }
            }
        }

    } else {
        throw new runtime_error("No tree decomposition");
    }
    delete sp_tree;
    delete sp_graph;
    //delete graph;


    std::vector<std::pair<vertex_t*, double>> double_vector;

    // Convert each pair from (vertex_t*, int) to (vertex_t*, double)
    for (const auto& pair : scheduleOnOriginal) {
        double_vector.push_back({pair.first, static_cast<double>(pair.second)});
    }

    return double_vector;
}

vector<pair<vertex_t *, double>> calculateBottomLevels(graph_t *graph, int bottomLevelVariant) {
    vector<pair<vertex_t *, double > > ranks;
    switch (bottomLevelVariant) {
        case 1: {
            vertex_t *vertex = graph->first_vertex;
            while (vertex != NULL) {
                double rank = calculateSimpleBottomUpRank(vertex);
                //cout<<"rank for "<<vertex->name<<" is "<<rank<<endl;
                ranks.push_back({vertex, rank});
                vertex = vertex->next;
            }
            break;
        }
        case 2: {
            vertex_t *vertex = graph->first_vertex;
            while (vertex != NULL) {
                double rank = calculateBLCBottomUpRank(vertex);
                ranks.push_back({vertex, rank});
                vertex = vertex->next;
            }
            break;
        }
        case 3:
            ranks = calculateMMBottomUpRank(graph);
            break;
    }
    return ranks;
}