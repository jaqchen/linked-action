# Created by yejq.jiaqiang@gmail.com
# Simpile Makefile for Rust-Extended library

MAKEFLAGS    += -r -R
ifneq ("$(TARGET_CC)","")
CC           := $(TARGET_CC)
else
CC           := $(CROSS_COMPILE)gcc
endif

CFLAGS       := $(TARGET_CFLAGS) -I. -Wall -fPIC -O2 -ggdb -D_GNU_SOURCE
TARGETS      := rustex.o librustex.so

CFLAGS       += -I/usr/include/lua5.1
LDFLAGS      := -llua5.1

.PHONY: all clean

all: $(TARGETS)

%.o: %.c rustex.h
	$(CC) -c $(CFLAGS) -o $@ $<

librustex.so: rustex.o
	$(CC) -shared -o $@ $< -Wl,-soname=$@ $(LDFLAGS)

clean:
	rm -rf *.o *.so
