INCLUDE = ../include
SRC = ../src

OBJS = tcp-state-machine.o

main: $(OBJS)
	g++ test.cc $(OBJS) -I $(INCLUDE)

%.o : $(SRC)/%.cc $(INCLUDE)/%.h
	g++ $< -c -I $(INCLUDE)