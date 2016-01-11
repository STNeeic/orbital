CC = gcc
CFLAGS = -Wall -g
OBJS = rsc/kbhit.o gravity4.o

a.out:	$(OBJS)
	$(CC) $(OBJS) $(LDLIBS)

.PHONY: clean
clean:
	rm -f *~ *.o a.out rsc/*.o
