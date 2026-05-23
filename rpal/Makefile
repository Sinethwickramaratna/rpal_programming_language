MYOBJS  = .
SRC     = rpal.cpp

ifeq ($(OS),Windows_NT)
		TARGET   = rpal20.exe
    RM       = del /Q
else
		TARGET   = rpal20
    RM       = rm -f
endif

CC      = gcc
CCFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-comment -x c++
LDLIBS  = -lstdc++ -static-libstdc++ -lm

all: $(TARGET)

$(TARGET): $(SRC) parser.cpp standerdize.cpp cse_machine.cpp lexical_analyser.cpp token.h tree.h
	$(CC) $(CCFLAGS) $(SRC) $(LDLIBS) -o $(TARGET)

clean:
	$(RM) $(TARGET)

.PHONY: all clean