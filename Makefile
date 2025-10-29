CC      = gcc
CFLAGS  = -std=c17 -Wall -Wextra -Werror -O2
INCLUDE = -Isrc
SRC     = src/main.c src/spell.c src/dict.c
OBJ     = $(SRC:.c=.o)
BIN     = spell

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

test: all
	./tests/run_tests.sh

.PHONY: all clean test
