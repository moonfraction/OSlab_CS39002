SHELL := /bin/bash

all:
	gcc -Wall -o cook cook.c
	gcc -Wall -o waiter waiter.c
	gcc -Wall -o customer customer.c
db:
	gcc -Wall -o gencustomers gencustomers.c
	./gencustomers > customers.txt

rall: all
	./cook > transcript_all.txt 2>&1 & 
	./waiter >> transcript_all.txt 2>&1 & 
	./customer >> transcript_all.txt 2>&1 

clean:
	rm -f cook waiter customer gencustomers transcript*