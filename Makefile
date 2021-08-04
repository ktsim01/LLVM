all: parse tokenize 
	gcc -g *.c `llvm-config --cflags --ldflags --libs core` -o out
demo:
	./out < abc.txt
	llc-11 --filetype=obj test.bc
	gcc -static test.o
parse: 
	bison -Wall -d bison.y
tokenize:
	flex  --header-file=flex.l.h -o flex.l.c flex.l
clean:
	rm -f out *.out *.o *.s *.bc *.ll *.l.* *.tab.*