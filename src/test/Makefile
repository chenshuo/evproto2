CXXFLAGS = -Wall -g
LDFLAGS = -levent_core -lprotobuf -levproto2 -L..

all: client server
clean:
	rm client server *.o *.pb.h *.pb.cc

echo.pb.h echo.pb.cc: echo.proto
	protoc --cpp_out . $<

echo.pb.o: echo.pb.cc echo.pb.h
	g++ $(CXXFLAGS) -c $<

client.o: client.cc echo.pb.h
	g++ $(CXXFLAGS) -c $<

client: client.o echo.pb.o
	g++ -o $@ $^ $(LDFLAGS)

server.o: server.cc echo.pb.h
	g++ $(CXXFLAGS) -c $<

server: server.o echo.pb.o
	g++ -o $@ $^ $(LDFLAGS)


