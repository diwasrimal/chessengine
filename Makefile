CC = clang
CFLAGS = -Wall -Wextra -O3
HEADERS = src/engine.h src/move.h src/piece.h src/utils.h
OBJ = engine.o move.o piece.o utils.o
RL_FLAGS = `pkg-config --cflags raylib`
RL_LIBS = `pkg-config --libs raylib`

all: main tests

main: src/main.c $(OBJ)
	$(CC) $(CFLAGS) $(RL_FLAGS) -o $@ $^ $(RL_LIBS)

tests: src/tests.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o main tests