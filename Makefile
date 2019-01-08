CFLAGS+=-std=gnu11 -Wall -Wextra
all: fml
fml: fml.o lexer.o parser.o beautifier.o

clean:
	rm -f fml fml.o lexer.o parser.o beautifier.o utils.char.o
