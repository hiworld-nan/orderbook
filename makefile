CC = g++

Source = $(wildcard ./*.cpp)
Object = $(patsubst %.cpp, %.o, $(Source))

CFlags = -Wall -std=c++2b -m64
OFlags = -Ofast -mtune=native -march=native
LDFlags = -v -lpthread -fopenmp

CurrDir = ./
IncludeDir = -I./$(CurrDir)

all: tob

$(Object):%.o: %.cpp
	$(CC) $(CFlags) $(OFlags) $(IncludeDir) -c $< -o $@

tob: $(Object)
	$(CC) -o $@ $^ $(LDFlags)
	$(shell [ ! -d ./bin ] && mkdir -p ./bin )
	mv $@ ./bin
	
.PHONY: clean
clean:
	-rm -f ./bin/tob *.o $(Object)
