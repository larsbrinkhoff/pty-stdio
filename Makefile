CFLAGS = -O -Wall

all: pty-stdio

pty-stdio: main.o
	$(CC) -o $@ $^

clean:
	rm -f pty-stdio *.o
