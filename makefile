IDIR=include
SDIR=src

CC=g++
FLAG=-std=c++14

OBJS=tcp-buffer.o tcp-state-machine.o tcp-socket.o network.o \
	tcp-manager.o main.o tcp-internal.o

all : main.o

main.o : $(OBJS)
	$(CC) $(FLAG) -I $(IDIR) main.cc *.o -lpthread

%.o : $(SDIR)/%.cc $(IDIR)/%.h
	$(CC) $(FLAG) -c -I $(IDIR) $<

clean :
	rm *.o
