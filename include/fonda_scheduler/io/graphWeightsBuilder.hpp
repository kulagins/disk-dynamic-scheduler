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
    Cluster * buildClusterFromJson(nlohmann::json query);
    void fillGraphWeightsFromExternalSource(graph_t *graphMemTopology, nlohmann::json query);
    void retrieveEdgeWeights(graph_t *graphMemTopology, nlohmann::json query);
}

#endif