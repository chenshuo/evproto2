
CXXFLAGS = -Wall -g
LDFLAGS = -levent_core -lprotobuf

all: libevproto2.a
clean:
	rm *.a *.o *.pb.h *.pb.cc

libevproto2.a: RpcChannel.o RpcServer.o rpc.pb.o
	ar rcu $@ $^

RpcChannel.o : RpcChannel.cc RpcChannel.h rpc.pb.h
	g++ $(CXXFLAGS) -c $<

RpcServer.o : RpcServer.cc RpcServer.h rpc.pb.h
	g++ $(CXXFLAGS) -c $<

rpc.pb.h rpc.pb.cc: rpc.proto
	protoc --cpp_out . $<

rpc.pb.o: rpc.pb.cc rpc.pb.h
	g++ $(CXXFLAGS) -c $<

