all: pty-stdio

pty-stdio: main.o
	gcc -o $@ $^

clean:
	rm -f pty-stdio *.o
