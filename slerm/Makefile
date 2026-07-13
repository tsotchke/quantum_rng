CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -I src
LDLIBS  ?= -lm

SRCS    := src/qrng.c src/entropy.c
HDRS    := src/qrng.h
CLI     := src/qrng_cli.c
TEST    := src/test_qrng.c src/test_negative.c

all: qrng_demo

libqrng.a: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -c src/qrng.c -o src/qrng.o
	$(CC) $(CFLAGS) -c src/entropy.c -o src/entropy.o
	ar rcs $@ src/qrng.o src/entropy.o

qrng_demo: $(CLI) libqrng.a
	$(CC) $(CFLAGS) $(CLI) libqrng.a -o $@ $(LDLIBS)

test: test_qrng test_negative
	./test_qrng
	./test_negative

test_qrng: $(TEST) libqrng.a
	$(CC) $(CFLAGS) src/test_qrng.c libqrng.a -o $@ $(LDLIBS)

test_negative: src/test_negative.c libqrng.a
	$(CC) $(CFLAGS) src/test_negative.c libqrng.a -o $@ $(LDLIBS)

clean:
	rm -f *.o src/*.o *.a qrng_demo test_qrng test_negative

.PHONY: all clean test
