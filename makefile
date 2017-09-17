CC = g++-7
FLAG = -std=c++17

INCLUDE = -I include/

OBJS = state.o

main : $(OBJS)
	$(CC) $(FLAG) $(OBJS) main.cc $(INCLUDE)

%.o : src/%.cc include/%.h
	$(CC) $(FLAG) -c $< $(INCLUDE)

clean :
	rm *.o