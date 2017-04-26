CC = g++
STD = -std=c++14
FLAG = -Wall #-Wconversion
LIBRARY = -pthread

INCLUDE = -I ./include/

main : tcp-buffer.o tcp-state-machine.o tcp.o network.o tcp-manager.o main.o
	$(CC) $(STD) *.o $(LIBRARY) $(FLAG)

main.o : main.cc
	$(CC) $(STD) -c main.cc $(FLAG) $(INCLUDE)

tcp-buffer.o : include/tcp-buffer.h src/tcp-buffer.cc
	$(CC) $(STD) -c src/tcp-buffer.cc $(FLAG) $(INCLUDE)

tcp-state-machine.o : include/tcp-state-machine.h src/tcp-state-machine.cc
	$(CC) $(STD) -c src/tcp-state-machine.cc $(FLAG) $(INCLUDE)

tcp.o : include/tcp.h src/tcp.cc
	$(CC) $(STD) -c src/tcp.cc $(FLAG) $(INCLUDE)

network.o : include/network.h src/network.cc
	$(CC) $(STD) -c src/network.cc $(FLAG) $(INCLUDE)

tcp-manager.o : include/tcp-manager.h src/tcp-manager.cc
	$(CC) $(STD) -c src/tcp-manager.cc $(FLAG) $(INCLUDE)

clean :
	rm *.o
