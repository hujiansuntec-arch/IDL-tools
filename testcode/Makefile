# Makefile for IDL Socket Generated Code

CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2 -pthread
LDFLAGS = -pthread

all: keyvaluestore_client keyvaluestore_server test_callback_client test_callback_server

keyvaluestore_client: keyvaluestore_client_example.cpp keyvaluestore_socket.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

keyvaluestore_server: keyvaluestore_server_example.cpp keyvaluestore_socket.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

test_callback_client: test_callback_client.cpp keyvaluestore_socket.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

test_callback_server: test_callback_server.cpp keyvaluestore_socket.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f keyvaluestore_client keyvaluestore_server test_callback_client test_callback_server

.PHONY: all clean