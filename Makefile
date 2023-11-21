CFLAGS = -Wall -Wextra -O3
CC = clang

chess: chess.c
	$(CC) $(CFLAGS) -o chess chess.c

tmp: tmp.c
	$(CC) $(CFLAGS) -o tmp tmp.c

