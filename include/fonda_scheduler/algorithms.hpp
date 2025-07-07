#ifndef ALGORITHMS_HPP
#define ALGORITHMS_HPP

#include <algorithm>
#include <stdexcept>
#include <string>

#include "utils.hpp"

namespace fonda_scheduler {

enum ALGORITHMS {
    HEFT,
    HEFT_BL,
    HEFT_BLC,
    HEFT_MM
};

inline int algoNameToNumber(std::string algoName)
{
    std::transform(algoName.begin(), algoName.end(), algoName.begin(), ::tolower);
    algoName = trimQuotes(algoName);
    if (algoName == "heft") {
        return HEFT;
    } else if (algoName == "heft-bl") {
        return HEFT_BL;
    } else if (algoName == "heft-blc") {
        return HEFT_BLC;
    } else if (algoName == "heft-mm") {
        return HEFT_MM;
    } else {
        throw std::invalid_argument("Unknown algorithm name: " + algoName);
    }
}
}

#endif // ALGORITHMS_HPP
