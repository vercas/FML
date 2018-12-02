CFLAGS+=-std=gnu99 -Wall -Wextra
all: fml
fml: fml.o

clean:
	rm -f fml fml.o utils.char.o
