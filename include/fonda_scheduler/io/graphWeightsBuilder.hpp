/*
 * MongoDBReader.hpp
 *
 *  Created on: 07.04.2022
 *      Author: Fabian Brandt-Tumescheit, fabratu
 */

#ifndef FONDA_GRAPHWEIGHTSBUILDER_HPP_
#define FONDA_GRAPHWEIGHTSBUILDER_HPP_

#include <string>
#include <unordered_map>

#include "cluster.hpp"

namespace Fonda {
void fillGraphWeightsFromExternalSource(const graph_t* graphMemTopology,
    std::unordered_map<std::string, std::vector<std::vector<std::string>>> workflow_rows,
    const std::string& workflow_name, long inputSize, Cluster* cluster,
    int memShorteningDivision, double ioShorteningCoef);
void retrieveEdgeWeights(const graph_t* graphMemTopology);
Cluster* buildClusterFromCsv(const std::string& file, int memoryMultiplicator, double readWritePenalty, double offloadPenalty, int speedMultiplicator);
}

#endif