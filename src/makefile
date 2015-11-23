#makefile for ics server

workdir = ../bin
bin = icsserver
monitor = monitor

CC = g++
AR = ar
all : $(bin)


SRCS := $(wildcard *.cpp)
OBJS = $(SRCS:%.cpp=%.o)

INCS = -I/usr/local/gcc-5.2/include
LIBS = -lpthread -lodbc
FLAGS = -Wall -g -DDEBUG -fno-use-linker-plugin -std=c++14

$(bin) : $(OBJS)
	$(CC) $(INCS) $(LIBS) $(FLAGS) $^ -o $@
	@echo "--------------compile binary finished!--------------"

%.o : %.cpp
	$(CC) $(INCS) $(FLAGS) -c $< -o $@

%.o : %.c
	gcc $(FLAGS) -std=gnu99 -o $@ -c $<

.PHONY:	clean install test
clean:
	-rm -f $(OBJS) $(bin)

install:
	-cp -f $(bin) $(workdir)/

test:
	-echo "obj file:$(OBJS)"
	

	