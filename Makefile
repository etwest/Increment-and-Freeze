CXX = g++
OPTFLAGS = -O3 -DNDEBUG
DEFAULTFLAGS = -g -std=c++17 -Wall -I./include -D_GLIBCXX_PARALLEL -fopenmp -fsanitize=address -fsanitize=leak -fsanitize=undefined
CXXFLAGS = $(DEFAULTFLAGS) $(OPTFLAGS)

vpath %.h include
vpath %.cpp src

simulatePaging: simulation.o IAKWrapper.o OSTCacheSim.o OSTree.o IncrementAndFreeze.o
	$(CXX) $(CXXFLAGS) $^ -o simulatePaging

simulation.o: include/OSTCacheSim.h include/params.h include/OSTree.h include/CacheSim.h include/IAKWrapper.h
OSTree.o: include/OSTree.h

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o simulatePaging

.PHONY: debug
debug: CXXFLAGS = $(DEFAULTFLAGS)
debug: simulatePaging
