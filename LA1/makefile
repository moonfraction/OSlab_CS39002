all: clean
	gcc -o gendep gendep.c
	@if [ -z "$(n)" ]; then \
		./gendep; \
	else \
		./gendep $(n); \
	fi > foodep.txt
	gcc -o rebuild rebuild.c
	@echo "Compilation Successful, use 'make run NUM=5' to generate output file"

run:
	@if [ -z "$(NUM)" ]; then \
		./rebuild; \
		echo "./rebuild"; \
	else \
		./rebuild $(NUM); \
		echo "./rebuild $(NUM)"; \
	fi

clean:
	rm -f gendep rebuild done.txt foodep.txt
