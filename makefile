CC = g++
STD = -std=c++14
FLAG = -Wall -g #-Wconversion
LIBRARY = -pthread

INCLUDE = -I ./include/

main : tcp-buffer.o tcp-state-machine.o tcp-socket.o network.o \
		tcp-manager.o main.o tcp-internal.o
	$(CC) $(STD) *.o $(LIBRARY) $(FLAG)

main.o : main.cc
	$(CC) $(STD) -c main.cc $(FLAG) $(INCLUDE)

tcp-buffer.o : include/tcp-buffer.h src/tcp-buffer.cc
	$(CC) $(STD) -c src/tcp-buffer.cc $(FLAG) $(INCLUDE)

tcp-state-machine.o : include/tcp-state-machine.h src/tcp-state-machine.cc
	$(CC) $(STD) -c src/tcp-state-machine.cc $(FLAG) $(INCLUDE)

tcp-socket.o : include/tcp-socket.h src/tcp-socket.cc
	$(CC) $(STD) -c src/tcp-socket.cc $(FLAG) $(INCLUDE)

tcp-internal.o : include/tcp-internal.h src/tcp-internal.cc
	$(CC) $(STD) -c src/tcp-internal.cc $(FLAG) $(INCLUDE)

network.o : include/network.h src/network.cc
	$(CC) $(STD) -c src/network.cc $(FLAG) $(INCLUDE)

tcp-manager.o : include/tcp-manager.h src/tcp-manager.cc
	$(CC) $(STD) -c src/tcp-manager.cc $(FLAG) $(INCLUDE)

clean :
	rm *.o
