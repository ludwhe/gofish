.POSIX:

CC=gcc
CFLAGS=-O2 -Wall -Wextra -Werror

# GoFish macros
SRC_DIR=src
DOCS_DIR=docs

SRC_FILES=config.c gofish.c http.c log.c mime.c mkcache.c mmap_cache.c \
	socket.c webtest.c
HDR_FILES=config.h gofish.h version.h
OBJ_FILES=

all: .c.o

check:

install:

clean:
	rm -f $(OBJ_FILES)

# Define PHONY targets for GNU make
.PHONY: all check install clean
