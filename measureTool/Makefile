.PHONY: all
all: init clean
	make -C src

.PHONY: init
init:
	-mkdir bin
	-mkdir temp

.PHONY: clean
clean:
	-rm bin/* -r
	-rm temp/* -r
	make -C src clean