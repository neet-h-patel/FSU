CC= gcc
CXX= g++

all: clean cparse.exe

.c.o:
	$(CC) -c -o $@ $<
.cpp.o:
	$(CXX) -c -o $@ $< -std=c++11

cparse.exe: cparse.o
	$(CXX) -o cparse.exe cparse.o -std=c++11

clean:
	rm -f *.o *~ \#*\# cparse.exe yours.txt diff.txt core
