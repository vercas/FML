CFLAGS+=-std=gnu99 -Wall -Wextra
all: fml
fml: fml.o lexer.o parser.o

clean:
	rm -f fml fml.o lexer.o parser.o utils.char.o
