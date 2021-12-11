CXX = clang++
CXXFLAGS = -std=c++20 -Wall -I./include -O3 -g
vpath %.h include
vpath %.cpp src

simulatePaging: simulation.o LruSizesSim.o OSTree.o params.h RAM.h
	$(CXX) $(CXXFLAGS) $< LruSizesSim.o OSTree.o -o simulatePaging

LruSizesSim.o simulatinon.o: include/RAM.h include/params.h include/OSTree.h
OSTree.o: include/OSTree.h

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o simulatePaging
