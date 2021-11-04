CC = gcc
OPTFLAGS = -fomit-frame-pointer -fno-strict-aliasing -fno-aggressive-loop-optimizations -fconserve-stack -fmerge-constants

all:
	$(CC) microlife.c -nostartfiles -nostdlib -ftree-vectorize -mavx2 -march=native $(OPTFLAGS) -lkernel32 -Wall -Wextra -Os
