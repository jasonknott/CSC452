#!/bin/sh
# dir=/home/cs452/fall15/phase2/testResults
#dir=/Users/patrick/Classes/452/project/phase2/testResults
dir=./testResults
total=0
pass=0
for i in `seq 0 44`;
do
    total=$(($total + 1))

    if [ $i -lt 10 ]; then
        num=0$i
    else
        num=$i
    fi

    if [ -f test${num} ]
    then
        /bin/rm test${num}
    fi

    if  make test${num} >/dev/null 
    then
        test${num} > test${num}.txt 2> test${num}stderr.txt;
        if [ -s test${num}stderr.txt ]
        then
            cat test${num}stderr.txt >> test${num}.txt
        fi

        /bin/rm test${num}stderr.txt

        if diff --brief test${num}.txt ${dir}
        then
            # echo
            echo "\033[0;32mtest${num} passed!\033[0m"
            pass=$(($pass + 1))
        else
            echo test${num} failed

            # diff -C 1 test${num}.txt ${dir}
            # break
        fi
    else
        break
    fi
    # echo
done

echo $pass/$total
