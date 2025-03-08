SHELL := /bin/bash

all:
	gcc -Wall -o cook cook.c
	gcc -Wall -o waiter waiter.c
	gcc -Wall -o customer customer.c
db:
	gcc -Wall -o gencustomers gencustomers.c
	./gencustomers > customers.txt

rck: cook
	./cook > transcript_cook.txt 2>&1 &
rwt: waiter
	./waiter > transcript_waiter.txt 2>&1 &
rcs: customer
	./customer > transcript_customer.txt 2>&1

clean:
	rm -f cook waiter customer gencustomers

deepclean: clean
	rm -f transcript_*.txt customers.txt