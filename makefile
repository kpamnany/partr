# parallel tasks runtime
#
# makefile
#
# 2016.06.01   kiran.pamnany   Initial code
#

CC=gcc

.SUFFIXES: .c .h .o .a
.PHONY: clean test

CFLAGS+=-DPERF_PROFILE
CFLAGS+=-Wall
CFLAGS+=-std=c11
CFLAGS+=-D_GNU_SOURCE

CFLAGS+=-I../hwloc/include
CFLAGS+=-I../libconcurrent/include
CFLAGS+=-I./include
CFLAGS+=-I./src

SRCS=src/partr.c src/synctreepool.c src/taskpools.c src/multiq.c src/congrng.c
INCS=include/partr.h src/task.h src/synctreepool.h src/taskpools.h src/multiq.h src/congrng.h src/log.h src/perfutil.h src/profile.h
OBJS=${SRCS:.c=.o}

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
	$(RM) $(TARGET)
	$(AR) qvs $(TARGET) $(OBJS)

%.o: %.c $(INCS) makefile
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(MAKE) -C test clean
	$(RM) $(TARGET) $(OBJS)

