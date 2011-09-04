LEVELDB_HEADER = $(HOME)/leveldb/include
LEVELDB_LIB = $(HOME)/leveldb
CXXFLAGS = -Wall -g -O2 -I$(LEVELDB_HEADER)
LDFLAGS = -levent_core -levent_pthreads -lprotobuf -levproto2 -lleveldb -L.. -L$(LEVELDB_LIB)

all: client server
clean:
	rm client server core *.o *.pb.h *.pb.cc

kvdb.pb.h kvdb.pb.cc: kvdb.proto
	protoc --cpp_out . $<

kvdb.pb.o: kvdb.pb.cc kvdb.pb.h
	g++ $(CXXFLAGS) -c $<

client.o server.o: ../libevproto2.a

client.o: client.cc kvdb.pb.h
	g++ $(CXXFLAGS) -c $<

client: client.o kvdb.pb.o
	g++ -o $@ $^ $(LDFLAGS)

server.o: server.cc kvdb.pb.h
	g++ $(CXXFLAGS) -c $<

server: server.o kvdb.pb.o
	g++ -o $@ $^ $(LDFLAGS)


