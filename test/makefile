# parallel tasks runtime
#
# makefile for tests
#
# 2016.06.01   kiran.pamnany   Initial code
#

CC=icc

.SUFFIXES: .c .h .o
.PHONY: clean

CFLAGS+=-Wall
CFLAGS+=-std=c11
CFLAGS+=-D_GNU_SOURCE
CFLAGS+=-I../../hwloc/include
CFLAGS+=-I../../libconcurrent/include
CFLAGS+=-I.
CFLAGS+=-I../include
CFLAGS+=-I../src

LDOBJS+=-lpthread
LDOBJS+=tap.o
LDOBJS+=../libpartr.a
LDOBJS+=../../hwloc/src/.libs/libhwloc.a
LDOBJS+=../../libconcurrent/libconcurrent.a

TAPSRC=tap.c
TAPOBJ=tap.o

SRCS=taskpoolstest.c multiqtest.c parfortest.c fib.c
OBJS=${SRCS:.c=.o}
BINS=${SRCS:.c=}
OBJS=$(subst .c,.o, $(SRCS))
BINS=$(subst .c,, $(SRCS))

ifeq ($(DEBUG),yes)
    CFLAGS+=-O0 -g
else
    CFLAGS+=-O3
endif

all: $(BINS)

$(BINS): $(SRCS) $(TAPOBJ) ../libpartr.a
	$(CC) $(CFLAGS) taskpoolstest.c -o taskpoolstest $(LDOBJS)
	$(CC) $(CFLAGS) multiqtest.c -o multiqtest $(LDOBJS)
	$(CC) $(CFLAGS) parfortest.c -o parfortest $(LDOBJS)
	$(CC) $(CFLAGS) fib.c -o fib $(LDOBJS)

$(TAPOBJ): $(TAPSRC)
	$(CC) $(CFLAGS) -c $(TAPSRC)

clean:
	$(RM) $(BINS) $(OBJS) $(TAPOBJ)

