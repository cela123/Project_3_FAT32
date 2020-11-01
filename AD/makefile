main: main.o parser.o
        gcc -o main -std=c99 main.o parser.o

parser.o: parser.c parser.h
        gcc -c -std=c99 parser.c

main.o: main.c
        gcc -c -std=c99 main.c

clean:
        rm *.o main

