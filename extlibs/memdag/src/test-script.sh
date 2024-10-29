#!/bin/bash

nb_of_failed_tests=0
nb_of_successed_tests=0
count_success() {
    nb_of_failed_tests=$(($nb_of_failed_tests + $1))
    nb_of_successed_tests=$(($nb_of_successed_tests + 1 - $1))
}

test_command() {
#    echo Running test \"$1\"
    $1 > out 2>/dev/null
    cmp -s out $2
    success=$?
     count_success $success
    if [ $success -eq 1 ];
    then
	echo Error on test \"$1\"
#	diff out $2
#	exit 1
#    else
#	echo no error on test \"$1\"
    fi
}
    
### test the fifo libary
test_command "./test-fifo" tests/test-fifo.expected

### test the graph library
test_command "./test-graph" tests/test-graph.expected

# test SP recognition and SP-ization
test_command "./test-sp-graph tests/SP1.dot" tests/test-sp-graphs.SP1.expected

test_command "./test-sp-graph tests/SP2.dot" tests/test-sp-graphs.SP2.expected

test_command "./test-sp-graph tests/SP3.dot" tests/test-sp-graphs.SP3.expected

test_command "./test-sp-graph tests/NSP1.dot" tests/test-sp-graphs.NSP1.expected

test_command "./test-sp-graph tests/NSP2.dot" tests/test-sp-graphs.NSP2.expected

### test memdags options 

./memdag -s out -n "weight" tests/SP1.dot > /dev/null
diff -q out tests/SP1.to-sp-with-n.dot
count_success $?

test_command "./memdag -c -m tests/SP1.dot" tests/memdag.-c.-m.SP1.expected

test_command "./memdag -c -w weight tests/SP1.dot" tests/memdag.-c.-w.weight.SP1.expected

./memdag -s out.sp -m tests/SP2.dot  > out
diff -q out tests/memdag.-s.out.sp.-m.SP2.expected
diff -q out.sp tests/SP2.sp.expected
count_success $?

./memdag -s out.sp -m tests/NSP2.dot  > out
diff -q out tests/memdag.-s.out.sp.-m.NSP2.expected
diff -q out.sp tests/NSP2.sp.expected
count_success $?


### test memory minimization through SP-graphs

test_command "./memdag -m -n "weight" tests/SP1.dot" tests/memdag.-m.-n.weight.SP1.expected

test_command "./memdag -m tests/SP2.dot" tests/memdag.-m.SP2.expected

test_command "./memdag -m tests/SP3.dot" tests/memdag.-m.SP3.expected

test_command "./memdag -m tests/NSP1.dot" tests/memdag.-m.NSP1.expected

test_command "./memdag -m tests/NSP2.dot" tests/memdag.-m.NSP2.expected

### test bounded memory algorithms (maxmemory and restrict)

test_command "./memdag -M tests/SP1.dot " tests/memdag.-bigM.SP1.expected
 
test_command "./memdag -M tests/SP2.dot" tests/memdag.-bigM.SP2.expected

test_command "./memdag -M tests/NSP1.dot " tests/memdag.-bigM.NSP1.expected

test_command "./memdag -M -n "weight" tests/SP1.dot" tests/memdag.-bigM.-n.weight.SP1.expected

test_command "./memdag -r 100 tests/SP1.dot " tests/memdag.-r.SP1.expected

test_command "./memdag -r 600 -h minlevel tests/NSP2.dot" tests/memdag.-r.600.-h.minlevel.NSP2.expected

test_command "./memdag -r 600 -h respectorder tests/NSP2.dot" tests/memdag.-r.600.-h.respectorder.NSP2.expected

test_command "./memdag -r 600 -h maxsize tests/NSP2.dot" tests/memdag.-r.600.-h.maxsize.NSP2.expected

test_command "./memdag -r 600 -h maxminsize tests/NSP2.dot" tests/memdag.-r.600.-h.maxminsize.NSP2.expected

### test trees

test_command "./test-tree tests/simple-tree.txt" tests/simple-tree.1.out

test_command "./test-tree tests/simple-tree-liu-model.txt" tests/simple-tree-liu-model.0.out

test_command "./memdag -l -m tests/simple-tree-liu-model.txt " tests/memdag.-l.-m-simple-tree-liu-model.expected

test_command "./memdag -l -M tests/simple-tree-liu-model.txt" tests/memdag.-l.-MM-simple-tree-liu-model.expected

test_command "./memdag -l -p tests/simple-tree-liu-model.txt" tests/memdag.-l.-p-simple-tree-liu-model.expected

test_command "./memdag -l -p tests/simple-tree.txt" tests/memdag.-l.-p-simple-tree.expected 

test_command "./memdag -l -m tests/simple-tree.txt" tests/memdag.-l.-m-simple-tree.expected

test_command "./memdag -l -M tests/simple-tree.txt" tests/memdag.-l.-MM-simple-tree.expected


### finalize13

rm -f out out.sp

echo $nb_of_successed_tests tests were succeeded
echo $nb_of_failed_tests tests were failed
