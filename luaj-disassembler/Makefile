CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

SRCS = main.cpp LuajParser.cpp LuajDisassembler.cpp LuajAnalyzer.cpp LuajAssembler.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = luaj-disasm

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp LuajTypes.h LuajOpcodes.h LuajParser.h LuajDisassembler.h LuajAssembler.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
