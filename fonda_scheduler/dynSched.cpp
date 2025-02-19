#include "fonda_scheduler/dynSched.hpp"

#include <iterator>




double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(const vertex_t *v, const shared_ptr<Processor>&pj) {
    assert(pj->getAvailableMemory()>= 0);
    double Res = pj->getAvailableMemory() - peakMemoryRequirementOfVertex(v);
    for (int i = 0; i < v->in_degree; i++) {
        auto inEdge = v->in_edges[i];
        if(pj->getPendingMemories().find(inEdge)!= pj->getPendingMemories().end()){
            //incoming edge occupied memory
            Res+= inEdge->weight;
        }
    }
    return Res;
}

double new_heuristic(graph_t *graph, Cluster *cluster, int algoNum, bool isHeft){
    algoNum = isHeft? 1: algoNum;
    vector<pair<vertex_t *, double>> ranks = calculateBottomLevels(graph, algoNum);
    removeSourceAndTarget(graph, ranks);
    sort(ranks.begin(), ranks.end(),
         [](pair<vertex_t *, double> a, pair<vertex_t *, double> b) {
            return a.second > b.second;

    });
    double makespan=0;
    int numberWithEvictedCases=0;
    for (auto &pair: ranks){
        auto vertex = pair.first;
       // cout<<"processing "<< vertex->name<<endl;
        vector<shared_ptr<Processor>> bestModifiedProcs;
        shared_ptr<Processor> bestProcessorToAssign;
        double bestFinishTime;
        double bestStartTime;
        int resultingVar;
        edge_t *besttoKick;
        bestTentativeAssignment(cluster, isHeft, vertex, bestModifiedProcs, bestProcessorToAssign, bestFinishTime,
                                bestStartTime,
                                resultingVar,
                                besttoKick, numberWithEvictedCases);


        if(bestModifiedProcs.empty()){
            cout<<"Invalid assignment of "<<vertex->name;
            return -1;
        }
        else{
            cout<< " for "<< vertex->name<<" best "<<bestStartTime<<" "<<bestFinishTime<<" on proc "<<bestProcessorToAssign->id<<endl;//<<" with av mem "<<bestProcessorToAssign->availableMemory<<endl;
        }


        switch (resultingVar) {
            case 1:
                break;
            case 2:
                //cout<<"best with 1 kick"<<endl;
                assert(besttoKick!=nullptr);
                bestProcessorToAssign->delocateToDisk(besttoKick);
                numberWithEvictedCases++;
                checkIfPendingMemoryCorrect(bestProcessorToAssign);
                break;
            case 3:
                //cout<<"best with all kick"<<endl;
                for(auto it= bestProcessorToAssign->getPendingMemories().begin();
                                            it!= bestProcessorToAssign->getPendingMemories().end();){
                    it = bestProcessorToAssign->delocateToDisk(*it);
                    //cout<<"end deloc"<<endl;
                }
                assert(bestProcessorToAssign->getPendingMemories().empty());
                numberWithEvictedCases++;
                checkIfPendingMemoryCorrect(bestProcessorToAssign);
                break;
            default:
                throw runtime_error("");
        }
       // checkIfPendingMemoryCorrect(bestProcessorToAssign);

      //  cout<<"delocating from other procs according to tentative "<<endl;
       // bestProcessorToAssign->assignment+="started at "+ to_string(bestStartTime)+"; ";
        for (auto &modifiedProc: bestModifiedProcs){
           // cout<<" from "<<modifiedProc->id<<endl;
            auto procInClusterWithId = cluster->getProcessorById(modifiedProc->id);
            if( (procInClusterWithId->id!= bestProcessorToAssign->id || isHeft ) &&
            procInClusterWithId->getPendingMemories().size()!= modifiedProc->getPendingMemories().size()){

                    for(auto pendingInCluster= procInClusterWithId->getPendingMemories().begin();
                        pendingInCluster!= procInClusterWithId->getPendingMemories().end();){
                      //  print_edge(*pendingInCluster);
                        auto i = std::find_if(modifiedProc->getPendingMemories().begin(),
                                              modifiedProc->getPendingMemories().end(),
                                              [&pendingInCluster](auto pendInModif) {
                                                  return pendInModif==*pendingInCluster;
                                              });
                        if(i==modifiedProc->getPendingMemories().end()){
                            delocateFromThisProcessorToDisk(*pendingInCluster, modifiedProc->id);
                            //if((*pendingInCluster)->head->name== vertex->name || !(isHeft && procInClusterWithId->id== bestProcessorToAssign->id ) ) {
                            //    cout<<"increased available memory from "<< modifiedProc->availableMemory;
                            //    modifiedProc->availableMemory += (*pendingInCluster)->weight;
                            //    cout<<" to "<<modifiedProc->availableMemory<<endl;
                            //}
                        }
                         pendingInCluster++;
                    }

                }
                assert(modifiedProc->getReadyTimeCompute()<  std::numeric_limits<double>::max());
              //  cout<<"erplacing "<<modifiedProc->id<<endl;
                checkIfPendingMemoryCorrect(modifiedProc);
             //   modifiedProc->assignment+= " ready time "+ to_string(modifiedProc->readyTimeCompute)+" write "+
              //          to_string(modifiedProc->readyTimeWrite) + " read "+ to_string(modifiedProc->readyTimeRead)+ "; ";
                cluster->replaceProcessor(modifiedProc);




        }
        assert(bestProcessorToAssign->getReadyTimeCompute()<  std::numeric_limits<double>::max());
        vertex->assignedProcessorId= bestProcessorToAssign->id;

        //checkIfPendingMemoryCorrect(bestProcessorToAssign);

      //  cout<<"kicking in edges"<<endl;
        for (int j = 0; j < vertex->in_degree; j++) {
            edge *ine = vertex->in_edges[j];
       //    cout<<"ine ";print_edge(ine);
            for (auto location: ine->locations){
                if(location.locationType==LocationType::OnProcessor ){ //&& location.processorId.value()!=bestProcessorToAssign->id
                    assert(location.processorId.has_value());
                    shared_ptr<Processor> otherProc = cluster->getProcessorById(location.processorId.value());
                    auto it = std::find_if(otherProc->getPendingMemories().begin(), otherProc->getPendingMemories().end(),
                                           [&ine](edge_t *e) {
                                               return e->head->id == ine->head->id && e->tail->id == ine->tail->id;
                                           });
                    if(it != otherProc->getPendingMemories().end()){
                     //   cout<<"found and kicked from "<<otherProc->id<<endl;
                        otherProc->delocateToDisk(ine);
                    }
                    else{
                        cout<<"NOT FOUND ";
                        print_edge(ine);
                        //cout<<"IN ";
                       // for ( auto edge: otherProc->pendingMemories){
                       //     print_edge(edge);
                        //}
                        //cout<<endl;
                        cout<<"ON PROC "<<otherProc->id<<endl;
                        throw runtime_error("not found in mems");
                    }
                }
            }
            ine->locations.clear();//.emplace_back(LocationType::OnProcessor, bestProcessorToAssign->id);
        }
        checkIfPendingMemoryCorrect(bestProcessorToAssign);

      //  cout<<"actually remain pending: "<<bestProcessorToAssign->pendingMemories.size()<<" pieces, avail mem "<<bestProcessorToAssign->availableMemory<<" ";
        for (auto it = bestProcessorToAssign->getPendingMemories().begin();
             it != bestProcessorToAssign->getPendingMemories().end();){
        //    print_edge(*it);
            it++;
        }


        //cout<<"emplacing out edges , starting with "<<bestProcessorToAssign->availableMemory<<endl;
        for(int i=0; i<vertex->out_degree; i++) {
            auto v1 = vertex->out_edges[i];
           // print_edge(v1);
        //  cout<<"adding "<<v1->weight<<endl;
            bestProcessorToAssign->loadFromNowhere(v1);
            checkIfPendingMemoryCorrect(bestProcessorToAssign);

        }
        for (const auto &item: cluster->getProcessors()){
            checkIfPendingMemoryCorrect(item.second);
        }


        vertex->makespan= bestFinishTime;
        assert(bestStartTime<bestFinishTime);
        //cluster->printProcessors();
        if(makespan<bestFinishTime)
            makespan= bestFinishTime;
    }
    cout<< " #eviction "<< numberWithEvictedCases <<" ";
    return makespan;
}

void bestTentativeAssignment(Cluster *cluster, bool isHeft, vertex_t *vertex, vector<shared_ptr<Processor>> &bestModifiedProcs,
                             shared_ptr<Processor> &bestProcessorToAssign, double &bestFinishTime, double &bestStartTime, int &resultingVar,
                             edge_t *&besttoKick, int &numberWithEvictedCases) {
    bestFinishTime= numeric_limits<double>::max();
    bestStartTime= 0;

    double bestActualStartTime, bestActualFinishTime;
    for (auto& [id, processor] : cluster->getProcessors()) {
        double finTime=0, startTime=0, peakMem=0;
        double actualStartTime=0, actualFinishTime=0;
        double ftBefore = processor->getReadyTimeCompute();
        int resultingVariant;
        auto ourModifiedProc = make_shared<Processor>(*processor);
        edge* toKick;

        checkIfPendingMemoryCorrect(ourModifiedProc);
        vector<shared_ptr<Processor>> modifiedProcs = isHeft? tentativeAssignmentHEFT(vertex, ourModifiedProc,  actualFinishTime, actualStartTime, finTime,startTime, peakMem, cluster):
                tentativeAssignment(vertex, ourModifiedProc,
                                                                                 finTime, startTime,
                                                                                 peakMem, resultingVariant, toKick, cluster, isHeft);
        if(!isHeft)
            checkIfPendingMemoryCorrect(ourModifiedProc);
       // cout<<"start "<<startTime<<" end "<<finTime<<endl;
       if(startTime!=actualStartTime){
         //  cout<<"not actual "<<finTime<<" vs "<<actualFinishTime<<" on "<<processor->id<<endl;
       }
        if(bestFinishTime> finTime){
        //    cout<<"best acutalize "<<endl;
            assert(modifiedProcs.size()>0);
            bestModifiedProcs= modifiedProcs;
            bestFinishTime= finTime; bestStartTime = startTime;
            bestProcessorToAssign = ourModifiedProc;
            resultingVar = resultingVariant;
            besttoKick = toKick;
            if(isHeft){
                bestActualFinishTime = actualFinishTime;
                bestActualStartTime = actualStartTime;
            }
        }
        else{
            if(bestFinishTime==finTime){
                if(bestProcessorToAssign && ourModifiedProc->getMemorySize()>bestProcessorToAssign->getMemorySize()){
                    //cout<<"new best proc due to more mem"<<endl;
               //     cout<<"best acutalize ew best proc due to more mem"<<endl;
                    assert(modifiedProcs.size()>0);
                    bestModifiedProcs= modifiedProcs;
                    bestFinishTime= finTime; bestStartTime = startTime;
                    bestProcessorToAssign = ourModifiedProc;
                    resultingVar = resultingVariant;
                    besttoKick = toKick;
                    if(isHeft){
                        bestActualFinishTime = actualFinishTime;
                        bestActualStartTime = actualStartTime;
                    }
                }
            }
            assert(ftBefore == processor->getReadyTimeCompute());
        }
    }

    if(isHeft){

     // cout<<"best times: "<<bestStartTime<<" "<<bestFinishTime<<" "<<bestActualStartTime<<" "<<bestActualFinishTime<<(bestStartTime!=bestActualStartTime?" neq ":" eq ") <<" on "<<bestProcessorToAssign->id<<endl;
      assert(bestStartTime<=bestActualStartTime);
      if(bestStartTime!=bestActualStartTime){
          numberWithEvictedCases++;
      }
        bestFinishTime= bestActualFinishTime;
        bestStartTime= bestActualStartTime;
        resultingVar=1;
    }
}


vector<shared_ptr<Processor>>
tentativeAssignment(vertex_t *v, shared_ptr<Processor> ourModifiedProc,  double &finishTime, double &startTime,
                    double &peakMem, int& resultingvariant, edge * &toKick, Cluster * cluster, bool isThisBaseline) {
    assert(isThisBaseline==false);

    //cout<<"tent on proc "<<ourModifiedProc->id<< " ";
  //  assert(ourModifiedProc->readyTimeCompute<  std::numeric_limits<double>::max());
    resultingvariant=1;

    double sumOut= getSumOut(v);
    if(ourModifiedProc->getMemorySize()<sumOut){
     //  cout<<"too large outs absolutely"<<endl;
        finishTime= std::numeric_limits<double>::max();
        return {};
    }
    realSurplusOfOutgoingEdges(v, ourModifiedProc, sumOut);

    vector<std::shared_ptr<Processor>  > modifiedProcs;
    modifiedProcs.emplace_back(ourModifiedProc);
    processIncomingEdges(v, ourModifiedProc, modifiedProcs, startTime, cluster);


    assert(ourModifiedProc->getReadyTimeCompute()<  std::numeric_limits<double>::max());
    startTime = ourModifiedProc->getReadyTimeCompute()> startTime? ourModifiedProc->getReadyTimeCompute(): startTime;

    assert(ourModifiedProc->getReadyTimeCompute()<  std::numeric_limits<double>::max());


    if(!isThisBaseline)
        checkIfPendingMemoryCorrect(ourModifiedProc);

    double Res = howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(v, ourModifiedProc);
    peakMem = (Res<0)? 1:(ourModifiedProc->getMemorySize()-Res)/ourModifiedProc->getMemorySize();

    if(Res <0){
        //try finish times with and without memory overflow
        double amountToOffload = -Res;
        double shortestFT= std::numeric_limits<double>::max();

        double timeToFinishNoEvicted = startTime+ v->time/ ourModifiedProc->getProcessorSpeed() + amountToOffload / ourModifiedProc->memoryOffloadingPenalty;
        assert(timeToFinishNoEvicted>startTime);
        if(sumOut>ourModifiedProc->getAvailableMemory()){
            //cout<<"cant"<<endl;
            timeToFinishNoEvicted= std::numeric_limits<double>::max();
        }


        double timeToFinishBiggestEvicted = std::numeric_limits<double>::max(),
        timeToFinishAllEvicted = std::numeric_limits<double>::max() ;
        double timeToWriteAllPending = 0;

        double startTimeFor1Evicted, startTimeForAllEvicted;
        startTimeFor1Evicted = startTimeForAllEvicted = ourModifiedProc->getReadyTimeWrite()> startTime?
                                                        ourModifiedProc->getReadyTimeWrite(): startTime;
        if(!ourModifiedProc->getPendingMemories().empty()) {
            assert((*ourModifiedProc->getPendingMemories().begin())->weight>=(*ourModifiedProc->getPendingMemories().rbegin())->weight);
            auto biggestFileWeight = (*ourModifiedProc->getPendingMemories().begin())->weight;
            double amountToOffloadWithoutBiggestFile = (amountToOffload - biggestFileWeight) > 0 ? (amountToOffload -
                                                                                                    biggestFileWeight) : 0 ;
            double finishTimeToWrite = ourModifiedProc->getReadyTimeWrite() +
                                       biggestFileWeight / ourModifiedProc->writeSpeedDisk;
            startTimeFor1Evicted = max(startTime, finishTimeToWrite);
            timeToFinishBiggestEvicted =
                    startTimeFor1Evicted
                    + v->time / ourModifiedProc->getProcessorSpeed() +
                    amountToOffloadWithoutBiggestFile / ourModifiedProc->memoryOffloadingPenalty;
            assert(timeToFinishBiggestEvicted>startTimeFor1Evicted);

            double availableMemWithoutBiggest = ourModifiedProc->getAvailableMemory()+ biggestFileWeight;
            if(sumOut>availableMemWithoutBiggest)
                timeToFinishBiggestEvicted= std::numeric_limits<double>::max();



            double sumWeightsOfAllPending=0;
            for (const auto &item: ourModifiedProc->getPendingMemories()) {
                timeToWriteAllPending += item->weight / ourModifiedProc->writeSpeedDisk;
                sumWeightsOfAllPending+= item->weight;
            }

            double amountToOffloadWithoutAllFiles = (amountToOffload - sumWeightsOfAllPending > 0) ?
                    amountToOffload - sumWeightsOfAllPending : 0 ;

            assert(amountToOffloadWithoutAllFiles>=0);
            finishTimeToWrite = ourModifiedProc->getReadyTimeWrite() +
                                timeToWriteAllPending;
            startTimeForAllEvicted = max(startTimeForAllEvicted, finishTimeToWrite);
            timeToFinishAllEvicted = startTimeForAllEvicted + v->time / ourModifiedProc->getProcessorSpeed() +
                                     amountToOffloadWithoutAllFiles / ourModifiedProc->memoryOffloadingPenalty;
            assert(timeToFinishAllEvicted>startTimeForAllEvicted);

        }

        double minTTF = min(timeToFinishNoEvicted, min(timeToFinishBiggestEvicted, timeToFinishAllEvicted));
        if(minTTF==std::numeric_limits<double>::max() ){
            cout<<"minTTF inf"<<endl;
            finishTime= std::numeric_limits<double>::max();
            return {};
        }
        assert(ourModifiedProc->getReadyTimeCompute()<  std::numeric_limits<double>::max());
        ourModifiedProc->setReadyTimeCompute( minTTF);
        finishTime= ourModifiedProc->getReadyTimeCompute();
        assert(ourModifiedProc->getReadyTimeCompute()<  std::numeric_limits<double>::max());

        if(timeToFinishBiggestEvicted == minTTF){
            toKick = (*ourModifiedProc->getPendingMemories().begin());
            //cout<<"best tentative with biggest Evicted "; print_edge(toKick);
            resultingvariant=2;
            ourModifiedProc->setReadyTimeWrite( ourModifiedProc->getReadyTimeWrite() +
                    (*ourModifiedProc->getPendingMemories().begin())->weight / ourModifiedProc->writeSpeedDisk);
            // ourModifiedProc->pendingMemories.erase()
            //penMemsAsVector.erase(penMemsAsVector.begin());
            assert(startTime<=startTimeFor1Evicted);
            startTime= startTimeFor1Evicted;
            assert(toKick!=nullptr);
            assert(!toKick->locations.empty());
            assert(isLocatedOnThisProcessor(toKick, ourModifiedProc->id));
        }
        if(timeToFinishAllEvicted==minTTF){
            resultingvariant=3;
           // cout<<"best tentative with all Evicted ";
            ourModifiedProc->setReadyTimeWrite( ourModifiedProc->getReadyTimeWrite() + timeToWriteAllPending);
            assert(startTime<=startTimeForAllEvicted);
            startTime= startTimeForAllEvicted;
            //penMemsAsVector.resize(0);

        }


    }
    else{
        //startTime =  ourModifiedProc->readyTimeCompute;
        // printInlineDebug("should be successful");
        ourModifiedProc->setReadyTimeCompute(startTime + v->time/ ourModifiedProc->getProcessorSpeed());
        finishTime= ourModifiedProc->getReadyTimeCompute();

    }
   // cout<<endl;
    assert(finishTime> startTime);
    return modifiedProcs;
}


vector<shared_ptr<Processor>>
tentativeAssignmentHEFT(vertex_t *v, shared_ptr<Processor> ourModifiedProc, double &actualFinishTime, double &actualStartTime,
                        double &perceivedFinishTime, double &perceivedStartTime,
                        double &peakMem, Cluster * cluster) {

    //cout<<"tent on proc "<<ourModifiedProc->id<< " ";
    assert(ourModifiedProc->getReadyTimeCompute()<  std::numeric_limits<double>::max());


    double sumOut= getSumOut(v); bool kicked=false;

    if(ourModifiedProc->getMemorySize()<sumOut){
      //  cout<<"too large outs absolutely"<<endl;
        perceivedFinishTime = actualFinishTime= std::numeric_limits<double>::max();
        return {};
    }

    // cout<<"sumOut includes ";
    realSurplusOfOutgoingEdges(v, ourModifiedProc, sumOut);



    vector<std::shared_ptr<Processor> > modifiedProcs;
    modifiedProcs.emplace_back(ourModifiedProc);
    processIncomingEdges(v, ourModifiedProc, modifiedProcs, perceivedStartTime, cluster);


    assert(ourModifiedProc->getReadyTimeCompute()<  std::numeric_limits<double>::max());
    actualStartTime = perceivedStartTime =  ourModifiedProc->getReadyTimeCompute()> perceivedStartTime?
            ourModifiedProc->getReadyTimeCompute(): perceivedStartTime;

    assert(ourModifiedProc->getReadyTimeCompute()<  std::numeric_limits<double>::max());
    double initAvM= ourModifiedProc->getAvailableMemory();

    if( ourModifiedProc->getAvailableMemory()<sumOut) {
        kicked=true;
       // cout<<"sum out is "<<sumOut <<", kicking unexpectedly "<<endl;
        double stillNeedsToBeEvictedToRun = sumOut - ourModifiedProc->getAvailableMemory();
        double writeTime = actualStartTime > ourModifiedProc->getReadyTimeCompute()? actualStartTime: ourModifiedProc->getReadyTimeCompute();

        for (auto it = ourModifiedProc->getPendingMemories().begin();
             it != ourModifiedProc->getPendingMemories().end() && stillNeedsToBeEvictedToRun > 0;) {
            //  print_edge(*it);
            if((*it)->head->name!=v->name){
                stillNeedsToBeEvictedToRun -= (*it)->weight;
                writeTime+= (*it)->weight/ourModifiedProc->writeSpeedDisk;
                it =  ourModifiedProc->removePendingMemory(*it);
            }
            else{++it;}

        }
        if(stillNeedsToBeEvictedToRun>0){

        }
        assert(stillNeedsToBeEvictedToRun<=0);
        assert(ourModifiedProc->getAvailableMemory()>= sumOut);
        actualStartTime= writeTime;
        ourModifiedProc->setReadyTimeWrite( writeTime);
        ourModifiedProc->setReadyTimeCompute(writeTime);
        assert(ourModifiedProc->getReadyTimeCompute()<  std::numeric_limits<double>::max());
       // cout<<"ednded up with "<<ourModifiedProc->availableMemory<<endl;
        //checkIfPendingMemoryCorrect(ourModifiedProc);


        //   cout<<"assuming that remain pending: "<<ourModifiedProc->pendingMemories.size()<<" pieces, with avail memory "<<ourModifiedProc->availableMemory<< " ";
        initAvM = ourModifiedProc->getAvailableMemory();
        // for (auto it = ourModifiedProc->pendingMemories.begin();
        //      it != ourModifiedProc->pendingMemories.end();){
        //      print_edge(*it);
        //     it++;
        //  }

        //  cout<<endl;

    }

    double Res = howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(v, ourModifiedProc);
    peakMem = (Res<0)? 1:(ourModifiedProc->getMemorySize()-Res)/ourModifiedProc->getMemorySize();

    if(Res <0){
        //try finish times with and without memory overflow
        double amountToOffload = -Res;

         perceivedFinishTime = perceivedStartTime+ v->time/ ourModifiedProc->getProcessorSpeed() + amountToOffload / ourModifiedProc->memoryOffloadingPenalty;
         actualFinishTime = actualStartTime+ v->time/ ourModifiedProc->getProcessorSpeed() + amountToOffload / ourModifiedProc->memoryOffloadingPenalty;
        assert(perceivedFinishTime>perceivedStartTime);


        if(perceivedFinishTime==std::numeric_limits<double>::max() ){
            cout<<"perceivedFinishTime inf"<<endl;
            actualFinishTime= std::numeric_limits<double>::max();
            return {};
        }
        assert(ourModifiedProc->getReadyTimeCompute()<  std::numeric_limits<double>::max());
        ourModifiedProc->setReadyTimeCompute( actualFinishTime);
        assert(ourModifiedProc->getReadyTimeCompute()<  std::numeric_limits<double>::max());
        assert(perceivedStartTime<= actualStartTime);
        assert(perceivedFinishTime<= actualFinishTime);
        if(kicked){
            assert(perceivedStartTime< actualStartTime);
            assert(perceivedFinishTime< actualFinishTime);
        }

        return modifiedProcs;

    }
    else{
        //startTime =  ourModifiedProc->readyTimeCompute;
        // printInlineDebug("should be successful");
        ourModifiedProc->setReadyTimeCompute( actualStartTime + v->time/ ourModifiedProc->getProcessorSpeed());
        actualFinishTime= ourModifiedProc->getReadyTimeCompute();
        perceivedFinishTime= actualFinishTime;
    }
 //    cout<<endl;
    assert(actualFinishTime> actualStartTime);
    return modifiedProcs;
}

void realSurplusOfOutgoingEdges(const vertex_t *v, shared_ptr<Processor> &ourModifiedProc, double & sumOut) {
    for (int i = 0; i < v->in_degree; i++) {
        auto inEdge = v->in_edges[i];
        if (isLocatedOnThisProcessor(inEdge, ourModifiedProc->id)) {
            //     cout<<"in is located here "; print_edge(v->in_edges[i]);
            auto pendingOfProc = ourModifiedProc->getPendingMemories();
            //assert(pendingOfProc.find(inEdge) != pendingOfProc.end());
            if(pendingOfProc.find(inEdge) != pendingOfProc.end()){
                sumOut -= inEdge->weight;
            }
            else{
               // cout<<"edge "<<buildEdgeName(inEdge)<<" not anymore found in pending mems of processor "<<ourModifiedProc->id<<endl;
            }

        }

    }
  //  cout << "REQUIRES AT THE END: " << sumOut << endl;
}



void
processIncomingEdges(const vertex_t *v, shared_ptr<Processor> &ourModifiedProc, vector<std::shared_ptr<Processor>> &modifiedProcs,
                     double &earliestStartingTimeToComputeVertex, Cluster * cluster) {
    earliestStartingTimeToComputeVertex = ourModifiedProc->getReadyTimeCompute();
    for (int j = 0; j < v->in_degree; j++) {
        edge *incomingEdge = v->in_edges[j];
        vertex_t *predecessor = incomingEdge->tail;

        if (predecessor->assignedProcessorId == ourModifiedProc->id) {
            if(!isLocatedOnThisProcessor(incomingEdge, ourModifiedProc->id)){
                assert(isLocatedOnDisk(incomingEdge));
                ourModifiedProc->setReadyTimeRead(ourModifiedProc->getReadyTimeRead() + incomingEdge->weight / ourModifiedProc->readSpeedDisk);
                earliestStartingTimeToComputeVertex =  ourModifiedProc->getReadyTimeRead()>earliestStartingTimeToComputeVertex?
                                                       ourModifiedProc->getReadyTimeRead(): earliestStartingTimeToComputeVertex;
            }

        }
        else{
            if (isLocatedOnDisk(incomingEdge)) {
                //we need to schedule read
                ourModifiedProc->setReadyTimeRead( ourModifiedProc->getReadyTimeRead()+ incomingEdge->weight / ourModifiedProc->readSpeedDisk);
                earliestStartingTimeToComputeVertex =  ourModifiedProc->getReadyTimeRead()>earliestStartingTimeToComputeVertex?
                                                       ourModifiedProc->getReadyTimeRead(): earliestStartingTimeToComputeVertex;
                //TODO evict??
            } else {
                auto predecessorsProcessorsId = predecessor->assignedProcessorId;
                assert(isLocatedOnThisProcessor(incomingEdge, predecessorsProcessorsId));
                shared_ptr<Processor>  addedProc;
                auto it = //modifiedProcs.size()==1?
                                  //  modifiedProcs.begin():
                                    std::find_if(modifiedProcs.begin(), modifiedProcs.end(),
                                      [predecessorsProcessorsId](const shared_ptr<Processor>& p) {
                                          return p->id == predecessorsProcessorsId;
                                      });

                if(it==modifiedProcs.end()){
                   addedProc = make_shared<Processor>(*cluster->getProcessorById(predecessorsProcessorsId));
                   // cout<<"adding modified proc "<<addedProc->id<<endl;
                    modifiedProcs.emplace_back(addedProc);
                    checkIfPendingMemoryCorrect(addedProc);
                }
                else{
                    addedProc = *it;
                }

                assert(!hasDuplicates(modifiedProcs));

                double timeToStartWriting= max(predecessor->makespan, addedProc->getReadyTimeWrite());
                addedProc->setReadyTimeWrite( timeToStartWriting+ incomingEdge->weight / addedProc->writeSpeedDisk);
                double startTimeOfRead = max(addedProc->getReadyTimeWrite(), ourModifiedProc->getReadyTimeRead());
                double endTimeOfRead = startTimeOfRead + incomingEdge->weight / ourModifiedProc->readSpeedDisk;
                ourModifiedProc->setReadyTimeRead(endTimeOfRead);

                earliestStartingTimeToComputeVertex = max(earliestStartingTimeToComputeVertex, endTimeOfRead);
                //int addpl  = addedProc->pendingMemories.size();
                addedProc->removePendingMemory(incomingEdge);
               // assert(addpl> addedProc->pendingMemories.size());
                checkIfPendingMemoryCorrect(addedProc);
            }
        }
    }
}








graph_t *convertToNonMemRepresentation(graph_t *withMemories, map<int, int> &noMemToWithMem) {
    enforce_single_source_and_target(withMemories);
    graph_t *noNodeMemories = new_graph();

    for (vertex_t *vertex = withMemories->source; vertex; vertex = next_vertex_in_sorted_topological_order(withMemories,
                                                                                                           vertex,
                                                                                                           &sort_by_increasing_top_level)) {
        vertex_t *invtx = new_vertex(noNodeMemories, vertex->name + "-in", vertex->time, nullptr);
        noMemToWithMem.insert({invtx->id, vertex->id});
        if (!noNodeMemories->source) {
            noNodeMemories->source = invtx;
        }
        vertex_t *outvtx = new_vertex(noNodeMemories, vertex->name + "-out", 0.0, nullptr);
        noMemToWithMem.insert({outvtx->id, vertex->id});
        edge_t *e = new_edge(noNodeMemories, invtx, outvtx, vertex->memoryRequirement, nullptr);
        noNodeMemories->target = outvtx;

        for (int i = 0; i < vertex->in_degree; i++) {
            edge *inEdgeOriginal = vertex->in_edges[i];
            string expectedName = inEdgeOriginal->tail->name + "-out";
            vertex_t *outVtxOfCopiedInVtxOfEdge = findVertexByName(noNodeMemories, expectedName);

            if (outVtxOfCopiedInVtxOfEdge == nullptr) {
                print_graph_to_cout(noNodeMemories);
                outVtxOfCopiedInVtxOfEdge = findVertexByName(noNodeMemories, expectedName);
                cout << "expected: " << expectedName << endl;
                throw std::invalid_argument(" no vertex found for expected name.");

            }
            edge_t *e_new = new_edge(noNodeMemories, outVtxOfCopiedInVtxOfEdge, invtx, inEdgeOriginal->weight, nullptr);
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

    SP_tree_t *sp_tree = nullptr;
    graph_t *sp_graph = nullptr;

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
        throw runtime_error("No tree decomposition");
    }
    delete sp_tree;
    delete sp_graph;
    //delete graph;


    std::vector<std::pair<vertex_t*, double>> double_vector;

    // Convert each pair from (vertex_t*, int) to (vertex_t*, double)
    for (const auto& pair : scheduleOnOriginal) {
        double_vector.emplace_back(pair.first, static_cast<double>(pair.second));
    }

    return double_vector;
}

vector<pair<vertex_t *, double>>buildRanksWalkOver(graph_t *graph){
    vector<pair<vertex_t *, double > > ranks;
    enforce_single_source_and_target(graph);
    int rank=0;
    vertex_t *vertex = graph->first_vertex;
    while (vertex != nullptr) {
        if (vertex->in_degree == 0) {
            ranks.emplace_back(vertex,rank);
        }
    }
    for (auto &item: ranks){
        for (int i = 0; i < item.first->out_degree; i++) {
        //    if(find)
        }
    }

}

vector<pair<vertex_t *, double>> calculateBottomLevels(graph_t *graph, int bottomLevelVariant) {
    vector<pair<vertex_t *, double > > ranks;
    switch (bottomLevelVariant) {
        case 1: {
            vertex_t *vertex = graph->first_vertex;
            while (vertex != nullptr) {
                double rank = calculateSimpleBottomUpRank(vertex);
                //cout<<"rank for "<<vertex->name<<" is "<<rank<<endl;
                ranks.emplace_back(vertex, rank);
                vertex = vertex->next;
            }
            break;
        }
        case 2: {
            vertex_t *vertex = graph->first_vertex;
            while (vertex != nullptr) {
                double rank = calculateBLCBottomUpRank(vertex);
                ranks.emplace_back(vertex, rank);
                vertex = vertex->next;
            }
            break;
        }
        case 3:
            ranks = calculateMMBottomUpRank(graph);
            break;
        default: throw runtime_error("unknon algorithm");
    }
    return ranks;
}

[[maybe_unused]] inline void checkIfPendingMemoryCorrect(const shared_ptr<Processor>& p){
  /*  double sumOut=0;
    for(auto pendingMemorie : p->pendingMemories){
        sumOut +=pendingMemorie->weight;
    }
    double busy = p->availableMemory + sumOut;
    if(abs(p->getMemorySize() -busy) >0.1)
        cout<<"check "<<p->getMemorySize()<<" vs "<< busy<<endl;
    assert(abs(p->getMemorySize() -busy) <0.1);
    assert(p->readyTimeCompute<  std::numeric_limits<double>::max()); */
}

[[maybe_unused]] inline bool hasDuplicates(const std::vector<shared_ptr<Processor>>& vec) {
    /*std::unordered_set<int> seenIds;
    for (const auto& obj : vec) {
        if (!seenIds.insert(obj->id).second) {
            // Insert returns {iterator, false} if the value already exists
            return true;
        }
    }
    return false; */ return false;
}