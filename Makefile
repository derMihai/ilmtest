CFLAGS = -Wall -Wpedantic -Wextra -g -O3
LIBS = -lpthread -lgsl -lm 
DEPFLAGS = -MT $@ -MMD -MP -MF $*.d

all: sem_test

sem_test: sem_test.o libmat_cbuf.so
	$(CC) -o $@  $^ ${LIBS} -L. -lmat_cbuf

sem_test.o: sem_test.c sem_test.d
	$(CC) ${DEPFLAGS} -c ${CFLAGS} $< -o $@

libmat_cbuf.so: mat_cbuf.o
	$(CC) -shared -o $@ $<

mat_cbuf.o: mat_cbuf.c mat_cbuf.d
	$(CC) $(DEPFLAGS) -c $(CFLAGS) -fpic -o $@ $<

DEPFILES := sem_test.d mat_cbuf.d

$(DEPFILES):

include $(wildcard $(DEPFILES))

clean:
	@rm sem_test *.o *.so *.d
	