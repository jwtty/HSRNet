TARGET := ../bin
CC := gcc
MACRO :=
CFLAGS := $(MACRO) -O1 -Wall -I ../include
PROGS := controller server client
OUT := csapp.o util.o log.o
LIB := -lpthread

.PHONY: all
all: $(PROGS)

client: $(OUT)
	$(CC) $(CFLAGS) -o $(TARGET)/client client/client.c $(OUT) $(LIB)

server: $(OUT)
	$(CC) $(CFLAGS) -o $(TARGET)/server server/server.c $(OUT) $(LIB)

controller: $(OUT)
	$(CC) $(CFLAGS) -o $(TARGET)/controller controller/controller.c $(OUT) $(LIB)

log.o:
	$(CC) $(CFLAGS) -c log.c

util.o:
	$(CC) $(CFLAGS) -c util.c

csapp.o:
	$(CC) $(CFLAGS) -c csapp.c

.PHONY: clean
clean: 
	-rm $(OUT) $(PROG) *~