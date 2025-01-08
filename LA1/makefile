all: clean
	gcc -o gendep gendep.c
	./gendep > foodep.txt
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
	@echo "Rebuild process completed"

clean:
	rm -f gendep rebuild done.txt foodep.txt
