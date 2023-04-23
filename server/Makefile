GDB = -g
THREAD = -pthread
PQXX = -lpqxx -lpq
GPB = `pkg-config --cflags --libs protobuf`
PROTO_CC = lib/build/gen/world_amazon.pb.cc lib/build/gen/amazon_ups.pb.cc
PROTO_H = lib/build/gen/world_amazon.pb.h lib/build/gen/amazon_ups.pb.h
VERSION = -std=c++11
TOOLS_H =  myException.h socket.h gpbCommunication.h
TOOLS_CPP = socket.cpp 
DATA_STRUCTURE_H = warehouse.h 


all: server

server:  server.h $(PROTO_H) $(TOOLS_H) $(DATA_STRUCTURE_H)
	g++ -o server main.cpp server.cpp $(TOOLS_CPP) $(PROTO_CC) $(PQXX) $(GDB) $(THREAD) $(GPB) $(VERSION)

.PHONY:
	clean
clean:
	rm -rf *.o server  