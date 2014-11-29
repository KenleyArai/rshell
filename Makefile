opt= -Wall -Werror -ansi -pedantic -std=c++1y

all: rshell
rshell:
	-[ -d bin ] && rm -fr bin
	mkdir bin
	g++ $(opt) ./src/rshell.cpp -o ./bin/rshell
	g++ $(opt) ./src/ls.cpp -o ./bin/ls
	g++ $(opt) ./src/cp.cpp -o ./bin/cp


