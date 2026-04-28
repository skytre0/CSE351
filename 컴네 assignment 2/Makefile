SANFLAGS = -fsanitize=address
CXXFLAGS = -Wall -O2 -std=c++17 -g $(SANFLAGS)
LDFLAGS = $(SANFLAGS)

all: proxy

proxy:	proxy.o
	g++ -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	g++ -c -o $@ $< $(CXXFLAGS)

clean:
	rm -f proxy *.o