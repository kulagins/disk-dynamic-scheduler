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

**Call**
memoryMultiplicator speedMultiplicator readWritePenalty offloadPenalty,workflow, inputSize, algorithmNumber, isBaseline, root directory, machines file, number of deviation function
e.g. ./fonda_scheduler 1000000 100 1 0.001 chipseq_200 41366257414 1 no ../ machines.csv 1

* algos with  memory awareness: 1 - HEFT-BL, 2- HEFT-BL, 3- HEFT-MM
* HEFT (no memory awareness) : yes at isBaseline, algoNum is irrelevant then

deviations :  
* 1 - normal deviation function around historical value with 10% deviation
*  2 - normal deviation function around historical value with 50% deviation
*  3 - no deviation
  
One call computes first the *dynamic* schedule, then the *static* one with the same deviations.
