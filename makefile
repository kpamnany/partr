# parallel tasks runtime
#
# makefile
#
# 2016.06.01   kiran.pamnany   Initial code
#

CC=icc

.SUFFIXES: .c .h .o .a
.PHONY: clean test

CFLAGS+=-Wall
CFLAGS+=-std=c11
CFLAGS+=-D_GNU_SOURCE

CFLAGS+=-I../hwloc/include
CFLAGS+=-I../libconcurrent/include
CFLAGS+=-I./include
CFLAGS+=-I./src

SRCS=src/partr.c src/taskpools.c src/multiq.c src/congrng.c
OBJS=$(subst .c,.o, $(SRCS))

ifeq ($(DEBUG),yes)
    CFLAGS+=-O0 -g
else
    CFLAGS+=-O3
endif

TARGET=libpartr.a

all: $(TARGET)

test: $(TARGET)
	$(MAKE) -C test

$(TARGET): $(OBJS)
	$(RM) -f $(TARGET)
	$(AR) qvs $(TARGET) $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(MAKE) -C test clean
	$(RM) -f $(TARGET) $(OBJS)

