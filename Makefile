# Simple Calculator
#
#   make          build ./calc
#   make run      build and start the REPL
#   make test     build and run the test suite
#   make clean    remove build output

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

BIN      := calc
TEST_BIN := calc_tests

.PHONY: all run test clean

all: $(BIN)

$(BIN): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# tests.cpp #includes main.cpp, so both are prerequisites.
$(TEST_BIN): tests.cpp main.cpp
	$(CXX) $(CXXFLAGS) -o $@ tests.cpp

run: $(BIN)
	./$(BIN)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(BIN) $(TEST_BIN)
