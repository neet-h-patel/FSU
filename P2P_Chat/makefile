CC=gcc
CXX=g++
# CFLAGS=-ansi -Wall -pedantic
CXXFLAGS=-std=c++11 -g -Wall -pedantic

.PHONY : all clean

all : messenger_server messenger_client

messenger_server : messenger_server.cpp
	$(CXX) $(CXXFLAGS) -o messenger_server messenger_server.cpp

messenger_client : messenger_client.cpp
	$(CXX) $(CXXFLAGS) -pthread -o messenger_client messenger_client.cpp

clean:
	rm -f *.o *.x core.* messenger_server messenger_client
