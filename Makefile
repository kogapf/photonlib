.PHONY: all
all:
	cppcheck photontool.c --error-exitcode=1
	gcc photontool.c -lpng -lpthread -o photontool

install: all
	cp photontool /usr/local/bin

clean: 
	-rm log nohup.out
