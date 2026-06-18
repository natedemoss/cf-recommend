# Simple Makefile for cf (the cf-recommend tool).
#
# Use this when CMake is unavailable. Requires a C++17 compiler plus the libcurl
# and sqlite3 development libraries. On MSYS2/UCRT64:
#   pacman -S mingw-w64-ucrt-x86_64-{gcc,curl,sqlite3}
# and build from a UCRT64 shell (or with that bin/ directory on PATH).

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Isrc

SRCS := $(wildcard src/*.cpp src/*/*.cpp)
OBJS := $(SRCS:.cpp=.o)

ifeq ($(OS),Windows_NT)
    # Link a self-contained cf.exe so it runs without MSYS2 DLLs on PATH.
    BIN := cf.exe
    CXXFLAGS += -DCURL_STATICLIB
    LDFLAGS  += -static -static-libgcc -static-libstdc++
    LDLIBS   := $(shell pkg-config --static --libs libcurl) -lsqlite3
else
    BIN := cf
    LDLIBS := -lcurl -lsqlite3
endif

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BIN)
