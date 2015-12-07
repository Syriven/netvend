CXX = g++
CXXFLAGS = -g -Wall -std=c++0x

INCDIR=-I/usr/local/include -I/home/syriven/develop/c++/common -I./
LIBDIR=-L/usr/lib/x86_64-linux-gnu/ -L/usr/lib/postgresql/9.4
LIBS=-lpqxx -lpq -lboost_system -lpthread -lcryptopp

all: server client

client: client.o crypto.o networking.o commands.o nv_packet.o nv_response.o b58check.o pack.o nv_exception.o
	$(CXX) $(CXXFLAGS) -o client client.o crypto.o networking.o commands.o nv_packet.o nv_response.o b58check.o pack.o nv_exception.o $(LIBS)

client.o: client.cpp common_constants.h crypto.h networking.h commands.h nv_packet.h nv_response.h nv_exception.h
	$(CXX) $(CXXFLAGS) -c client.cpp $(INCDIR)

server: server.o database.o crypto.o networking.o btc.o commands.o nv_packet.o nv_response.o b58check.o pack.o nv_exception.o
	$(CXX) $(CXXFLAGS) -o server server.o database.o crypto.o networking.o btc.o commands.o nv_packet.o nv_response.o b58check.o pack.o nv_exception.o $(LIBS)

server.o: server.cpp common_constants.h database.h crypto.h networking.h btc.h commands.h nv_packet.h nv_response.h nv_exception.h
	$(CXX) $(CXXFLAGS) -c server.cpp $(INCDIR)

database.o: database.cpp database.h crypto.h common_constants.h
	$(CXX) $(CXXFLAGS) -c database.cpp $(INCDIR) $(LIBS)

nv_exception.o: nv_exception.cpp nv_exception.h
	$(CXX) $(CXXFLAGS) -c nv_exception.cpp $(INCDIR) $(LIBS)

nv_packet.o: nv_packet.cpp nv_packet.h networking.h
	$(CXX) $(CXXFLAGS) -c nv_packet.cpp $(INCDIR) $(LIBS)

nv_response.o: nv_response.cpp nv_response.h networking.h
	$(CXX) $(CXXFLAGS) -c nv_response.cpp $(INCDIR) $(LIBS)

networking.o: networking.cpp networking.h
	$(CXX) $(CXXFLAGS) -c networking.cpp $(INCDIR) $(LIBS)

btc.o: btc.cpp btc.h
	$(CXX) $(CXXFLAGS) -c btc.cpp $(INCDIR) $(LIBS)

crypto.o: crypto.cpp crypto.h b58check.h
	$(CXX) $(CXXFLAGS) -c crypto.cpp $(INCDIR) $(LIBS)

commands.o: commands.cpp commands.h pack.h
	$(CXX) $(CXXFLAGS) -c commands.cpp $(INCDIR) $(LIBS)

b58check.o: b58check.cpp b58check.h
	$(CXX) $(CXXFLAGS) -c b58check.cpp $(INCDIR) $(LIBS)

pack.o: pack.cpp pack.h
	$(CXX) $(CXXFLAGS) -c pack.cpp $(INCDIR) $(LIBS)