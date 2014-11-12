opt= -Wall -Werror -ansi -pedantic

all: rshell

rshell:
	mkdir bin
	g++ $(opt) ./src/rshell.cpp -o ./bin/rshell
	g++ $(opt) ./src/ls.cpp -o ./bin/ls
	g++ $(opt) ./src/cp.cpp -o ./bin/cp

