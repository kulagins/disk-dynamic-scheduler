#!/bin/bash
pwd

./fonda_scheduler 1000000 100 1 0.001 chipseq_200 3793245764 1 no ../ machines.csv 
./fonda_scheduler 1000000 100 1 0.001 chipseq_2000 3793245764 1 no ../ machines.csv 
./fonda_scheduler 1000000 100 1 0.001 chipseq_8000 3793245764 1 no ../ machines.csv 
./fonda_scheduler 1000000 100 1 0.001 chipseq_15000 3793245764 1 no ../ machines.csv 

./fonda_scheduler 1000000 100 1 0.001 atacseq_200 2223941232 1 no ../ machines.csv 
./fonda_scheduler 1000000 100 1 0.001 atacseq_2000 2223941232 1 no ../ machines.csv 
./fonda_scheduler 1000000 100 1 0.001 atacseq_8000 2223941232 1 no ../ machines.csv 
./fonda_scheduler 1000000 100 1 0.001 atacseq_15000 2223941232 1 no ../ machines.csv 

./fonda_scheduler 1000000 100 1 0.001 eager_200 25705994498 1 no ../ machines.csv
./fonda_scheduler 1000000 100 1 0.001 eager_2000 25705994498 1 no ../ machines.csv
./fonda_scheduler 1000000 100 1 0.001 eager_8000 25705994498 1 no ../ machines.csv
./fonda_scheduler 1000000 100 1 0.001 eager_15000 25705994498 1 no ../ machines.csv

./fonda_scheduler 1000000 100 1 0.001 methylseq_200 17027257018 1 no ../ machines.csv 
./fonda_scheduler 1000000 100 1 0.001 methylseq_2000 17027257018 1 no ../ machines.csv 
./fonda_scheduler 1000000 100 1 0.001 methylseq_8000 17027257018 1 no ../ machines.csv 
./fonda_scheduler 1000000 100 1 0.001 methylseq_15000 17027257018 1 no ../ machines.csv 