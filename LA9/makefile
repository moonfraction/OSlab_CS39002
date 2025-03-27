run: demandpaging.cpp
	g++ -Wall -o runsearch demandpaging.cpp
	./runsearch
vrun: demandpaging.cpp
	g++ -Wall -DVERBOSE -o runsearch demandpaging.cpp
	./runsearch
db: gensearch.c
	g++ -Wall -o gensearch gensearch.c
	./gensearch
clean:
	-rm -f runsearch gensearch

deepclean: clean
	-rm -f *output.txt