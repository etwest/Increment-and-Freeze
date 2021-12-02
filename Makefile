CXX = g++
CXXFLAGS = -std=c++11 -Wall -I./include -O3
vpath %.h include
vpath %.cpp src

simulatePaging: simulation.cpp LRU_Size_Simulation.o params.h OSTree.o
	$(CXX) $(CXXFLAGS) $< LRU_Size_Simulation.o OSTree.o -o simulatePaging

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o simulatePaging
