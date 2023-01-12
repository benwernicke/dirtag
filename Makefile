#CFLAGS := -Wall -pedantic -g -fsanitize=leak -fsanitize=undefined -fsanitize=address
CFLAGS := -march=native -mtune=native -O3 -flto

dirtag.out := main.o flag.o

obj	:= ${dirtag.out}

all: dirtag.out

%.out: ${obj}
	${CC} ${CFLAGS} ${$@} -o ${@}

%.o: %.c
	${CC} ${CFLAGS} $< -c -o $@

clean:
	rm *.o *.out

install:
	mv dirtag.out /usr/local/bin/dirtag

.PHONY: clean all dbg install
