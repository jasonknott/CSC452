#!/bin/sh
dir=./testResults


if [ "$#" -eq 0 ] 
then
    echo "Usage: sh testphase4.ksh <num>"
    echo "where <num> is 00, 01, 02, ... or 26"
    exit 1
fi

num=$1
if [ -f test${num} ]
then
    /bin/rm test${num}
fi

# Copy disk and terminal files
cp testcases/disk0.orig disk0
cp testcases/disk1.orig disk1
for i in 0 1 2 3; do
    cp testcases/term${i}.in.orig term${i}.in
done

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
        clear
        echo
        cat test${num}.txt
        # diff -C 1 test${num}.txt ${dir}
    fi
fi
echo
