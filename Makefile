CC = clang
CFLAGS = -Wall -Wextra -O3
HEADERS = src/engine.h src/move.h src/piece.h src/utils.h
OBJ = engine.o move.o piece.o utils.o

all: chess tests

%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

chess: main.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

tests: tests.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o chess tests