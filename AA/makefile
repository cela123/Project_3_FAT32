main: main.o parser.o hex_helper.o
	gcc -o main -std=c99 main.o parser.o hex_helper.o

parser.o: parser.c parser.h
	gcc -c -std=c99 parser.c

hex_helper.o: hex_helper.c hex_helper.h
	gcc -c -std=c99 hex_helper.c

main.o: main.c
	gcc -c -std=c99 main.c

clean:
	rm *.o main
