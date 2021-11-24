CXX = g++
CXXFLAGS = -g -std=c++11 -Wall -I./include
vpath %.h include
vpath %.cpp src

simulatePaging: simulation.cpp LRU_RAM.o Clock_RAM.o params.h OSTree.o
	$(CXX) $(CXXFLAGS) $< LRU_RAM.o Clock_RAM.o OSTree.o -o simulatePaging

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o simulatePaging
