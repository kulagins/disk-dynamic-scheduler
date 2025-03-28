# Dynamic Scheduler - Disk Model

**A static and a dynamic scheduler**

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

