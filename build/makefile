#makefile for ics server

workdir = ../bin
bin = icsserver
monitor = monitor

CC = g++
AR = ar
all : $(bin)

SRCDIR = ../src
SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(SRCS:%.cpp=%.o)

INCS = -I /usr/local/gcc-5.2/include -I $(SRCDIR) -I ~/log4cplus-1.2.0-rc5/include
LIBS = -lpthread -lodbc -llog4cplus -lrt
FLAGS = -Wall -g -DDEBUG -fno-use-linker-plugin -std=c++14 -Wno-deprecated-declarations

$(bin) : $(OBJS)
	$(CC) $(INCS) $(LIBS) $(FLAGS) $^ -o $@
	-cp -f $(bin) $(workdir)/
	@echo "Make complete! Install the bin file"

%.o : %.cpp
	$(CC) $(INCS) $(FLAGS) -c $< -o $@


.PHONY:	clean install test
clean:
	-rm -f $(OBJS) $(bin)

install:
	-cp -f $(bin) $(workdir)/

test:
	@echo "test vaule: $(OBJS)"
	

	