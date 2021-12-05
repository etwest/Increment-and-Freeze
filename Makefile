CXX = g++
CXXFLAGS = -std=c++11 -Wall -I./include -O3
vpath %.h include
vpath %.cpp src

simulatePaging: simulation.cpp LruSizesSim.o OSTree.o params.h RAM.h
	$(CXX) $(CXXFLAGS) $< LruSizesSim.o OSTree.o -o simulatePaging

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o simulatePaging
