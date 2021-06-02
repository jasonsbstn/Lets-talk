all: letstalk.c
	gcc -c letstalk.c list.c 
	gcc -o lets-talk -pthread letstalk.o -lreadline list.o
valgrind:
	valgrind --leak-check=full ./lets-talk 3000 localhost 3001
clean:
	-rm -f *.o run
