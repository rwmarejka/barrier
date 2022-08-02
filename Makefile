#
# Makefile
#

all	 : bar-test

clean    :
	rm -f barrier.o main.o bar-test

test     : bar-test
	./bar-test -b -i 5000000 -t 5

bar-test : barrier.o main.o
	cc -o bar-test main.o barrier.o

main.o	 : main.c barrier.h
	cc -c -DTEST=1 -I . -Wparentheses main.c

barrier.o : barrier.c barrier.h
	cc -c -Wparentheses -I . barrier.c

