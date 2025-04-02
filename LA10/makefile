run: LRU.c
	gcc -Wall -o runsearch LRU.c
	./runsearch
vrun: LRU.c
	gcc -Wall -DVERBOSE -o runsearch LRU.c
	./runsearch
db: gensearch.c
	gcc -Wall -o gensearch gensearch.c
	./gensearch
clean:
	-rm -f runsearch gensearch
deepclean: clean
	-rm -f *output.txt