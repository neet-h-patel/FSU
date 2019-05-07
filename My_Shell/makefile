CC=gcc
CXX=g++
# CFLAGS=-ansi -Wall -pedantic
CXXFLAGS=-std=c++11 -Wall -pedantic

.PHONY : all clean

all : myshell myls

myshell : myshell.cpp
	$(CXX) $(CXXFLAGS) -o myshell myshell.cpp

myls : myls.cpp
	$(CXX) $(CXXFLAGS) -o myls myls.cpp

clean:
	rm -f *.o *.x core.* myshell myls
