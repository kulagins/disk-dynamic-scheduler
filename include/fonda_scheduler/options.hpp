#ifndef FONDA_SCHEDULER_OPTIONS_HPP
#define FONDA_SCHEDULER_OPTIONS_HPP

#include <iostream>
#include <string>

#include <getopt.h>

#include "common.hpp"

namespace fonda {
struct Options {
    double memoryMultiplicator = static_cast<double>(1 << 30); // Default to 1 GiB
    double speedMultiplicator = 100.0; // Default to 100.0
    double readWritePenalty = 1.0; // Default read/write penalty
    double offloadPenalty = 1e5; // Default offload penalty

    // Select algorithm: HEFT (0), HEFT-BL (1), HEFT-BLC (2), HEFT-MM (3)
    int algoNumber = 0; // Default to HEFT (0)

    std::string workflowName = ""; // Default empty workflow name
    long inputSize = 0; // Default input size

    std::string pathPrefix = "./"; // Default prefix for dot files
    std::string machinesFile = "input/machines.csv"; // Default machines file
    std::string tracesFile = "input/traces.csv"; // Default traces file

    // Scale task's memory to fit the machine
    bool scaleToFit = false; // Default to not scaling to fit

    // Deviation models:
    // 1 - normal deviation function around historical value with 10% deviation
    // 2 - normal deviation function around historical value with 50% deviation
    // 3 - no deviation
    // 4 - everything x2 (times x2)
    // 5 - 30% deviation
    int deviationModel = 0; // Default deviation model
};

// List of options
static const struct option long_options[] = {
    { "memory-multiplicator", required_argument, nullptr, 'm' },
    { "speed-multiplicator", required_argument, nullptr, 's' },
    { "read-write-penalty", required_argument, nullptr, 'r' },
    { "offload-penalty", required_argument, nullptr, 'o' },
    { "workflow-name", required_argument, nullptr, 'w' },
    { "input-size", required_argument, nullptr, 'i' },
    { "algorithm", required_argument, nullptr, 'a' },
    { "path-prefix", required_argument, nullptr, 'p' },
    { "machines-file", required_argument, nullptr, 'f' },
    { "traces-file", required_argument, nullptr, 't' },
    { "deviation-model", required_argument, nullptr, 'd' },
    { "scale-to-fit", no_argument, nullptr, 'S' },
    { "help", no_argument, nullptr, 'h' },
    { nullptr, 0, nullptr, 0 } // End of options
};

static const char* short_options = "" //
                                   "m:" // memory-multiplicator
                                   "s:" // speed-multiplicator
                                   "r:" // read-write-penalty
                                   "o:" // offload-penalty
                                   "w:" // workflow-name
                                   "i:" // input-size
                                   "a:" // algorithm
                                   "p:" // path-prefix
                                   "f:" // machines-file
                                   "t:" // traces-file
                                   "d:" // deviation
                                   "S" // scale-to-fit
                                   "h"; // help

inline void printHelp(const char* program_name)
{
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  -m, --memory-multiplicator <value>   Set memory multiplicator. Default: 1073741824\n"
              << "  -s, --speed-multiplicator <value>    Set speed multiplicator. Default: 100.0\n"
              << "  -r, --read-write-penalty <value>     Set read/write penalty. Default: 1.0\n"
              << "  -o, --offload-penalty <value>        Set offload penalty. Default: 1.0\n"
              << "  -w, --workflow-name <name>           Set workflow name. Required\n"
              << "  -i, --input-size <size>              Set input size. Required\n"
              << "  -a, --algorithm <number>             Set algorithm. Default: heft. Options: heft, heft-bl, heft-blc, heft-mm\n"
              << "  -p, --path-prefix <prefix>           Set path prefix. Default: empty\n"
              << "  -f, --machines-file <file>           Set machines file. Default: input/machines.csv\n"
              << "  -t, --traces-file <file>             Set traces file. Default: input/traces.csv\n"
              << "  -d, --deviation-model <variant>      Set standard deviation model. Required.\n"
              << "                                         Options:\n"
              << "                                           1. Normal 10%.\n"
              << "                                           2. Normal 50%.\n"
              << "                                           3. No deviation.\n"
              << "                                           4. Everything x2.\n"
              << "                                           5. Normal 30%.\n"
              << "  -S, --scale-to-fit                   Scale task's memory to fit the machine. Default: false\n"
              << "  -h, --help                           Show this help message and exit\n";
}

inline int algoNameToNumber(std::string algoName)
{
    std::transform(algoName.begin(), algoName.end(), algoName.begin(), ::tolower);
    algoName = trimQuotes(algoName);
    if (algoName == "heft") {
        return 0; // HEFT
    } else if (algoName == "heft-bl") {
        return 1; // HEFT-BL
    } else if (algoName == "heft-blc") {
        return 2; // HEFT-BLC
    } else if (algoName == "heft-mm") {
        return 3; // HEFT-MM
    } else {
        throw std::invalid_argument("Unknown algorithm name: " + algoName);
    }
}

template <typename T, typename ParseFunction>
void parseArg(const char* name, const char* arg, T& value, ParseFunction&& parse_function)
{
    try {
        value = parse_function(arg);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing option '" << name << " with value '" << arg << "': " << e.what() << '\n';
        printHelp(name);
    }
}

inline Options parseOptions(int argc, char* argv[])
{
    Options options;
    int option_index = 0;

    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (c) {
        case 'm':
            parseArg("memory-multiplicator", optarg, options.memoryMultiplicator, [](const char* arg) { return std::stoi(arg); });
            break;
        case 's':
            parseArg("speed-multiplicator", optarg, options.speedMultiplicator, [](const char* arg) { return std::stoi(arg); });
            break;
        case 'r':
            parseArg("read-write-penalty", optarg, options.readWritePenalty, [](const char* arg) { return std::stod(arg); });
            break;
        case 'o':
            parseArg("offload-penalty", optarg, options.offloadPenalty, [](const char* arg) { return std::stod(arg); });
            break;
        case 'w':
            options.workflowName = optarg;
            options.workflowName = trimQuotes(options.workflowName);
            break;
        case 'i':
            parseArg("input-size", optarg, options.inputSize, [](const char* arg) { return std::stol(arg); });
            break;
        case 'a':
            parseArg("algorithm", optarg, options.algoNumber, [](const char* arg) { return algoNameToNumber(arg); });
            break;
        case 'p':
            options.pathPrefix = optarg;
            options.pathPrefix = trimQuotes(options.pathPrefix);
            break;
        case 'f':
            options.machinesFile = optarg;
            options.machinesFile = trimQuotes(options.machinesFile);
            break;
        case 'd':
            parseArg("deviation-model", optarg, options.deviationModel, [](const char* arg) { return std::stoi(arg); });
            break;
        case 'S':
            options.scaleToFit = true;
            break;
        case 'h':
            printHelp(argv[0]);
            exit(EXIT_SUCCESS);
        default:
            std::cerr << "Unknown option: " << c << '\n';
            printHelp(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    const auto print_error = [&](const bool error_condition, const char* message) {
        if (error_condition) {
            std::cerr << "Error: " << message << '\n';
            printHelp(argv[0]);
            exit(EXIT_FAILURE);
        }
    };

    // Check if mandatory options are set
    print_error(options.workflowName.empty(), "Workflow name is required.");
    print_error(options.inputSize <= 0, "Input size must be a positive integer.");
    print_error(options.deviationModel < 1 || options.deviationModel > 5, "Deviation model must be between 1 and 5.");

    return options;
}
} // namespace fonda

#endif // FONDA_SCHEDULER_OPTIONS_HPP