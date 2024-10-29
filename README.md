# Dynamic Scheduler - REST Server

**Installation guide**

* Check the code out
* Build the code
  * mkdir build
  * cd build
  * cmake ..
  * make
*  Run ```./fonda_scheduler ```

**Maybe necessary for the installation for the first time**

* git submodule update --init
* sudo apt-get install libssl-dev

** Current State  **
* The scheduler does not compute an average or other aggregation of time and memory weights, but rather takes the first value

** TODO Needs libpistache, libgraphviz and igraph. The igraph has to be 0.7.1**

**Required JSON to start a new workflow (example)**

```json
{
    "algorithm": 3,
    "workflow": {
        "name": "eager",
        "tasks": {
            "kraken_merge": {
                "time_predicted": {
                    "<node_1>": 190
                },
                "memory_predicted": {
                    "<node_1>": 500
                }
            },
            "kraken_parse": {
                "time_predicted": {
                    "C2": 20,
                    "C1": 50
                },
                "memory_predicted": {
                    "C2": 20
                }
            }
        },
        "dependencies": {}
    },
    "cluster": {
        "machines": {
            "C2": {
                "memory":196,
                "speed": 16
            },
            "local": {
                "memory":16,
                "speed": 2
            }            
        }
    }
}
```

**Input graph requirements**
* The graph is directed. The connectivity property is not required.
* Each task in the graph has a memor and a  weights.  The edge weight denoted the volume of communication between the two tasks.
* The sense of the weight on the task (workload or memory consumtion) is to be defined. These weights are used in the partitioning problem.
* DAG partitioning problem: Given a DAG G, and an upper bound B, find an acyclic k-way partition such that the weight of each part is no larger than B and the edge cut is minimized.
* The problem has 3 inputs: the DAG, # of partitions k and maximum weight of a partition B
* dagP does not know about the cluster resources and only outputs a DAG partition that satisfies the DAG partitioning problem.
* Edge weights have following sources:
 *  TaskInputSize is how much the task receives over *all* incoming edges.
 *  TaskOutputDataSize was not implemented, but can be implemented in the future (TBD).
 *  rchar and wchar are how much was actually written/read. If not all files are being used, then rchar can be than TaskInputSize. wchar can also be less than what was written, for unclear reasons.


