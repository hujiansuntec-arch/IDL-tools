# Makefile for IDL Socket Generated Code

CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2 -pthread
LDFLAGS = -pthread

all: schoolservice_client schoolservice_server

schoolservice_client: schoolservice_client_example.cpp schoolservice_socket.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

schoolservice_server: schoolservice_server_example.cpp schoolservice_socket.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f schoolservice_client schoolservice_server

.PHONY: all clean