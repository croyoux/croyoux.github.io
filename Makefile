CC=cc
CFLAGS=-Wall -Wextra -O3 -flto=auto

mw: mallow.c
	$(CC) $(CFLAGS) -ffast-math -march=native mallow.c libs/libraylib.a -lX11 -lm -o mw
