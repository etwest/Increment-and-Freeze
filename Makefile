CXX = g++
OPTFLAGS = -O3
DEFAULTFLAGS = -std=c++2a -Wall -I./include
CXXFLAGS = $(DEFAULTFLAGS) $(OPTFLAGS)

vpath %.h include
vpath %.cpp src

simulatePaging: simulation.o LruSizesSim.o OSTree.o IncrementAndKill.o
	$(CXX) $(CXXFLAGS) $^ -o simulatePaging

LruSizesSim.o simulation.o: include/RAM.h include/params.h include/OSTree.h
OSTree.o: include/OSTree.h

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o simulatePaging

.PHONY: debug
debug: CXXFLAGS = $(DEFAULTFLAGS) -g
debug: simulatePaging
