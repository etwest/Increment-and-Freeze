CC = g++ -std=c++11 -Wall

simulatePaging: src/simulation.cpp LRU_RAM.o Clock_RAM.o include/params.h
	$(CC) src/simulation.cpp LRU_RAM.o Clock_RAM.o -o simulatePaging

LRU_RAM.o: src/LRU_RAM.cpp include/RAM.h
	$(CC) -c src/LRU_RAM.cpp

Clock_RAM.o: src/Clock_RAM.cpp include/RAM.h
	$(CC) -c src/Clock_RAM.cpp

.PHONY: clean
clean:
	rm -f *.o simulatePaging