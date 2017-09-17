CC = g++-7
FLAG = -std=c++17 -g -Wall

INCLUDE = -I include/

OBJS = state.o

main : $(OBJS)
	$(CC) $(FLAG) $(OBJS) main.cc $(INCLUDE)

test : clean $(OBJS)
	$(CC) $(FLAG) $(OBJS) test/test.cc $(INCLUDE) -o test.out
	./test.out
	-@rm -rf *.o

%.o : src/%.cc include/%.h
	$(CC) $(FLAG) -c $< $(INCLUDE)

clean :
	-@rm -rf *.o
