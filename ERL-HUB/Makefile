CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -Iinclude
LIBSRC    = src/Inventory.cpp src/Logistics.cpp

all: erlhub run_tests

erlhub: $(LIBSRC) src/main.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

run_tests: $(LIBSRC) tests/run_tests.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

run: erlhub
	./erlhub

test: run_tests
	./run_tests

clean:
	rm -f erlhub run_tests erlhub.exe run_tests.exe *.o erl_transit.log erl_manifest_audit.dat *.tmp test_records.bin
