/*
 * MongoDBReader.hpp
 *
 *  Created on: 07.04.2022
 *      Author: Fabian Brandt-Tumescheit, fabratu
 */

#ifndef FONDA_GRAPHWEIGHTSBUILDER_HPP_
#define FONDA_GRAPHWEIGHTSBUILDER_HPP_

#include <map>
#include <string>

#include "cluster.hpp"
#include "json.hpp"

typedef enum { INT, LONG, DOUBLE } valueTypes;

namespace Fonda {
    void fillGraphWeightsFromExternalSource(graph_t *graphMemTopology,
                                            std::unordered_map<std::string, std::vector<std::vector<std::string>>> workflow_rows,
                                            const string &workflow_name, long inputSize, Cluster *cluster,
                                            int memShorteningDivision, double ioShorteningCoef);
    void retrieveEdgeWeights(graph_t *graphMemTopology);
    Cluster * buildClusterFromCsv(const string& file, int memoryMultiplicator, double readWritePenalty, double offloadPenalty, int speedMultiplicator);
}

#endif