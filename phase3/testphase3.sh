#!/bin/sh
#dir=/home/cs452/fall15/phase3/testResults
<<<<<<< HEAD:phase3/testphase3.sh
# dir=/Users/patrick/Classes/452/project/phase3/testResults
=======
>>>>>>> 6cb8d3981637edecf3a73ae260762783cfc165c1:phase3/testphase3.ksh
dir=./testResults

if [ "$#" -eq 0 ] 
then
    echo "Usage: ksh testphase3.ksh <num>"
    echo "where <num> is 00, 01, 02, ... or 26"
    exit 1
fi

num=$1
if [ -f test${num} ]
then
    /bin/rm test${num}
fi

if  make test${num} 
then

    ./test${num} > test${num}.txt 2> test${num}stderr.txt;
    if [ -s test${num}stderr.txt ]
    then
        cat test${num}stderr.txt >> test${num}.txt
    fi

    /bin/rm test${num}stderr.txt

    if diff --brief test${num}.txt ${dir}
    then
        echo
        echo test${num} passed!
    else
        echo
        diff -C 1 test${num}.txt ${dir}
    fi
fi
echo
