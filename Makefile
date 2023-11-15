CFLAGS = -Wall -Wextra -g
CC = clang

chess: chess.c
	$(CC) $(CFLAGS) -o chess chess.c

tmp: tmp.c
	$(CC) $(CFLAGS) -o tmp tmp.c

