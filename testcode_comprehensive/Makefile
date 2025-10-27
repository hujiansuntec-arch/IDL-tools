# Makefile for IDL Socket Generated Code

CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2 -pthread
LDFLAGS = -pthread

all: typetestservice_client typetestservice_server

typetestservice_client: typetestservice_client_example.cpp typetestservice_socket.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

typetestservice_server: typetestservice_server_example.cpp typetestservice_socket.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f typetestservice_client typetestservice_server

.PHONY: all clean