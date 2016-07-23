CC = gcc
CFLAGS = -lpthread -lm -lrt
SOURCEDIR = ./src/

all: main

main: main.c $(SOURCEDIR)isotp_linux.o $(SOURCEDIR)isotp.o
	$(CC) -o main main.c $(SOURCEDIR)isotp_linux.o $(SOURCEDIR)isotp.o $(CFLAGS)

$(SOURCEDIR)isotp_linux.o: $(SOURCEDIR)isotp_linux.c $(SOURCEDIR)isotp_linux.h $(SOURCEDIR)isotp.o
	$(CC) -c $(SOURCEDIR)isotp_linux.c $(SOURCEDIR)isotp.o -o $(SOURCEDIR)isotp_linux.o $(CFLAGS)

$(SOURCEDIR)isotp.o: $(SOURCEDIR)isotp.c $(SOURCEDIR)isotp.h
	$(CC) -c $(SOURCEDIR)isotp.c -o $(SOURCEDIR)isotp.o

clean:
	rm -f main
	rm -f $(SOURCEDIR)*.o