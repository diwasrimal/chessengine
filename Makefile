CC = clang
CFLAGS = -Wall -Wextra -O3
HEADERS = $(wildcard src/*.h)
SRC = $(wildcard src/*c)
OBJ = $(filter-out build/main.o build/tests.o, $(patsubst src/%.c, build/%.o, $(SRC)))

# Raylib specific
RL_CFLAGS = `pkg-config --cflags raylib`
RL_LIBS = `pkg-config --libs raylib`

all: build/main build/tests

build/main: src/main.c $(OBJ)
	$(CC) $(CFLAGS) $(RL_CFLAGS) -o $@ $^ $(RL_LIBS) -lpthread

build/tests: src/tests.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf build
