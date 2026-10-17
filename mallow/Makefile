CC=cc
CFLAGS=-Wall -Wextra -O3 -flto=auto -I../libs -L../libs

mw: mallow.c
	$(CC) $(CFLAGS) -ffast-math -march=native mallow.c -lraylib -lX11 -lm -o mw
