SANFLAGS = -fsanitize=address
CXXFLAGS = -Wall -O2 -std=c++17 -g $(SANFLAGS)
LDFLAGS = $(SANFLAGS)

all: server client

server:	server.o
	g++ -o $@ $^ $(LDFLAGS)

client:	client.o
	g++ -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	g++ -c -o $@ $< $(CXXFLAGS)

clean:
	rm -f server client *.o