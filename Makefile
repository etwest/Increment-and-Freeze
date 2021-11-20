CXX = g++
CXXFLAGS = -g -std=c++11 -Wall -I./include
vpath %.h include
vpath %.cpp src

simulatePaging: simulation.cpp LRU_RAM.o Clock_RAM.o params.h OSTree.o
	$(CXX) $^ -o simulatePaging $(CXXFLAGS)

%.o: %.cpp %.h
	$(CXX) -c $< -o $@ $(CXXFLAGS)

.PHONY: clean
clean:
	rm -f *.o simulatePaging
