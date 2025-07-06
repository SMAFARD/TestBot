CXX = g++
CXXFLAGS = -O2 -std=c++23
LDFLAGS = -lixwebsocket -lssl -lcrypto -lpthread -lz
SRC = main.cpp
EXEC = app

all: $(EXEC)

$(EXEC): $(SRC)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(EXEC)

run: $(EXEC)
	./$(EXEC)

.PHONY: all clean run
