CC = g++
STD = -std=c++14
FLAG = -Wall #-Wconversion
LIBRARY = -pthread

INCLUDE = -I ./include/

main: tcp-buffer.o tcp-state-machine.o tcp.o network.o
	$(CC) $(STD) main.cc *.o $(LIBRARY) $(FLAG)

tcp-buffer.o: src/tcp-buffer.cc include/tcp-buffer.h
	$(CC) $(STD) -c src/tcp-buffer.cc $(FLAG) $(INCLUDE)

tcp-state-machine.o: src/tcp-state-machine.cc include/tcp-state-machine.h
	$(CC) $(STD) -c src/tcp-state-machine.cc $(FLAG) $(INCLUDE)

tcp.o: src/tcp.cc include/tcp.h
	$(CC) $(STD) -c src/tcp.cc $(FLAG) $(INCLUDE)

network.o: src/network.cc include/network.h
	$(CC) $(STD) -c src/network.cc $(FLAG) $(INCLUDE)

clean:
	rm *.o