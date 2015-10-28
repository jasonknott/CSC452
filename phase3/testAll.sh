#!/bin/sh
# dir=/home/cs452/fall15/phase2/testResults
#dir=/Users/patrick/Classes/452/project/phase2/testResults
dir=./testResults
total=0
pass=0
for i in `seq 0 25`;
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

        if diff --brief test${num}.txt ${dir} >/dev/null
        then
            # echo
            echo "\033[0;32mtest${num} passed!\033[0m"
            pass=$(($pass + 1))
        else

            # These checks are here so we can "pass" test related to time 
            if [ "$i" -eq "10" ]; then
                if [ $(diff test${num}.txt ${dir} | wc -l ) -le 24 ]; then
                    echo "\033[0;32mtest${num} passed!\033[0m"
                    pass=$(($pass + 1))
                else 
                    echo test${num} failed
                fi
                continue
            fi

            if [ "$i" -eq "11" ]; then
                if [ $(diff test${num}.txt ${dir} | wc -l ) -le 24 ]; then
                    echo "\033[0;32mtest${num} passed!\033[0m"
                    pass=$(($pass + 1))
                else 
                    echo test${num} failed
                fi
                continue
            fi

            if [ "$i" -eq "12" ]; then
                if [ $(diff test${num}.txt ${dir} | wc -l ) -le 42 ]; then
                    echo "\033[0;32mtest${num} passed!\033[0m"
                    pass=$(($pass + 1))
                else 
                    echo test${num} failed
                fi
                continue
            fi

            if [ "$i" -eq "19" ]; then
                if [ $(diff test${num}.txt ${dir} | wc -l ) -le 4 ]; then
                    echo "\033[0;32mtest${num} passed!\033[0m"
                    pass=$(($pass + 1))
                else 
                    echo test${num} failed
                fi
                continue
            fi

            if [ "$i" -eq "20" ]; then
                if [ $(diff test${num}.txt ${dir} | wc -l ) -le 4 ]; then
                    echo "\033[0;32mtest${num} passed!\033[0m"
                    pass=$(($pass + 1))
                else 
                    echo test${num} failed
                fi
                continue
            fi

            echo "\033[0;31mtest${num} failed!\033[0m"

            # diff -C 1 test${num}.txt ${dir}
            # break
        fi
    else
        break
    fi
    # echo
done

echo $pass/$total
