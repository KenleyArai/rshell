install:
	mkdir bin
	g++ -Wall -Werror -ansi -pedantic ./src/rshell.cpp -o ./bin/rshell
all: 
	install


