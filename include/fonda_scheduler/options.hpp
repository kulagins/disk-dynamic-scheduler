#ifndef FONDA_SCHEDULER_OPTIONS_HPP
#define FONDA_SCHEDULER_OPTIONS_HPP

#include <iostream>
#include <string>

#include <getopt.h>

#include "common.hpp"

namespace fonda {
struct Options {
    int memoryMultiplicator = 1 << 30; // Default to 1 GiB
    int speedMultiplicator = 1 << 10; // Default to 1024
    double readWritePenalty;
    double offloadPenalty;
    bool isBaseline;
    std::string workflowName;
    long inputSize;
    int algoNumber;
    std::string dotPrefix;
    std::string machinesFile;
    int deviationVariant;
    bool usePreemptiveWrites;
};

// List of options
static const struct option long_options[] = {
    { "memory-multiplicator", required_argument, nullptr, 'm' },
    { "speed-multiplicator", required_argument, nullptr, 's' },
    { "read-write-penalty", required_argument, nullptr, 'r' },
    { "offload-penalty", required_argument, nullptr, 'o' },
    { "workflow-name", required_argument, nullptr, 'w' },
    { "input-size", required_argument, nullptr, 'i' },
    { "algo-number", required_argument, nullptr, 'a' },
    { "is-baseline", no_argument, nullptr, 'b' },
    { "dot-prefix", required_argument, nullptr, 'd' },
    { "machines-file", required_argument, nullptr, 'f' },
    { "deviation-variant", required_argument, nullptr, 'v' },
    { "use-preemptive-writes", no_argument, nullptr, 'p' },
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
                                   "a:" // algo-number
                                   "b" // is-baseline
                                   "d:" // dot-prefix
                                   "f:" // machines-file
                                   "v:" // deviation-variant
                                   "p" // use-preemptive-writes
                                   "h" // help
    ;

void printHelp(const char* program_name)
{
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  -m, --memory-multiplicator <value>   Set memory multiplicator (default: 1073741824)\n"
              << "  -s, --speed-multiplicator <value>    Set speed multiplicator (default: 1024)\n"
              << "  -r, --read-write-penalty <value>     Set read/write penalty\n"
              << "  -o, --offload-penalty <value>        Set offload penalty\n"
              << "  -w, --workflow-name <name>           Set workflow name\n"
              << "  -i, --input-size <size>              Set input size\n"
              << "  -a, --algo-number <number>           Set algorithm number\n"
              << "  -b, --is-baseline                    Use baseline algorithm\n"
              << "  -d, --dot-prefix <prefix>            Set dot prefix\n"
              << "  -f, --machines-file <file>           Set machines file\n"
              << "  -v, --deviation-variant <variant>    Set deviation variant\n"
              << "  -p, --use-preemptive-writes          Use preemptive writes\n"
              << "  -h, --help                           Show this help message and exit\n";
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

Options parseOptions(int argc, char* argv[])
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
            parseArg("algo-number", optarg, options.algoNumber, [](const char* arg) { return std::stoi(arg); });
            break;
        case 'b':
            options.isBaseline = true;
            break;
        case 'd':
            options.dotPrefix = optarg;
            break;
        case 'f':
            options.machinesFile = optarg;
            break;
        case 'v':
            parseArg("deviation-variant", optarg, options.deviationVariant, [](const char* arg) { return std::stoi(arg); });
            break;
        case 'p':
            options.usePreemptiveWrites = true;
            break;
        case 'h':
            printHelp(argv[0]);
            exit(EXIT_SUCCESS);
        default:
            std::cerr << "Unknown option: " << c << std::endl;
            printHelp(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    return options;
}
} // namespace fonda

#endif // FONDA_SCHEDULER_OPTIONS_HPP