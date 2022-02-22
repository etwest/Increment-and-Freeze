CXX = g++
OPTFLAGS = #-O3
DEFAULTFLAGS = -g -std=c++2a -Wall -I./include 
#-D_GLIBCXX_PARALLEL -fopenmp
CXXFLAGS = $(DEFAULTFLAGS) $(OPTFLAGS)

vpath %.h include
vpath %.cpp src

simulatePaging: simulation.o LruSizesSim.o OSTree.o IncrementAndKill.o IncrementAndKillInPlace.o
	$(CXX) $(CXXFLAGS) $^ -o simulatePaging

simulation.o: include/LruSizesSim.h include/params.h include/OSTree.h include/CacheSim.h
OSTree.o: include/OSTree.h

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o simulatePaging

.PHONY: debug
debug: CXXFLAGS = $(DEFAULTFLAGS)
debug: simulatePaging
