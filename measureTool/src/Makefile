TARGET := ../bin
CC := gcc
MACRO :=
PACKAGE_PREFIX := mperf
CFLAGS := $(MACRO) -O2 -Wall -I ../include
PROGS := controller server client
OUT := csapp.o util.o log.o
LIB := -lpthread

.PHONY: all
all: $(PROGS)

client: $(OUT)
	$(CC) $(CFLAGS) -o $(TARGET)/$(PACKAGE_PREFIX)-client client/client.c $(OUT) $(LIB)

server: $(OUT)
	$(CC) $(CFLAGS) -o $(TARGET)/$(PACKAGE_PREFIX)-server server/server.c $(OUT) $(LIB)

controller: $(OUT)
	$(CC) $(CFLAGS) -o $(TARGET)/$(PACKAGE_PREFIX)-controller controller/controller.c $(OUT) $(LIB)

log.o:
	$(CC) $(CFLAGS) -c log.c

util.o:
	$(CC) $(CFLAGS) -c util.c

csapp.o:
	$(CC) $(CFLAGS) -c csapp.c

.PHONY: clean
clean: 
	-rm $(OUT) $(PROG) *~