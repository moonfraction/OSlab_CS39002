run: demandpaging.cpp
	g++ -Wall -include cstdint -o runsearch demandpaging.cpp
	./runsearch
vrun: demandpaging.cpp
	g++ -Wall -DVERBOSE -include cstdint -o runsearch demandpaging.cpp
	./runsearch

output: demandpaging.cpp
	g++ -Wall -include cstdint -o runsearch demandpaging.cpp
	./runsearch > output.txt
verboseoutput: demandpaging.cpp
	g++ -Wall -DVERBOSE -include cstdint -o runsearch demandpaging.cpp
	./runsearch > verboseoutput.txt

db: gensearch.c
	g++ -Wall -o gensearch gensearch.c
	./gensearch
clean:
	-rm -f runsearch gensearch

deepclean: clean
	-rm -f *output.txt