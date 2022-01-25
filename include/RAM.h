#ifndef RAM_H_GUARD
#define RAM_H_GUARD

#include <list>
#include <vector>
#include <unordered_map>
#include <fstream>
#include "params.h"
#include "OSTree.h"
#include "CacheSim.h"

// This class represents a single physical page 
class Page {
    private:
        uint64_t last_accessed = 0;   // timestamp of last access
        uint64_t virtual_addr = 0;    // virtual page mapped to this location
        const uint64_t physical_addr;
        bool free = true;
    public:
        /*
         * Creates a new page object
         * phy:      the unique physical address associated with this page
         * returns   a new page object
         */
        Page(uint64_t phy) : physical_addr(phy) {}

        // getters for the private member variables
        inline uint64_t get_phys()     { return physical_addr; }
        inline uint64_t get_virt()     { return virtual_addr; }
        inline uint64_t last_touched() { return last_accessed; }
        inline bool is_free()          { return free; }

        // update the timestamp of this page
        inline void access_page(uint64_t timestamp) { last_accessed = timestamp; }
        

        /*
         * Maps a new virtual address to this page and updates timestamp
         * v_addr:     the virtual address to map to this page
         * timestamp:  the timestamp of the memory access that caused the map
         * returns     nothing
         */
        inline void place_page(uint64_t v_addr, uint64_t timestamp) {
            virtual_addr = v_addr;
            last_accessed = timestamp;
            free = false;
        }

        /*
         * Evicts the current virtual address from this page
         * and marks the page as free
         * returns   nothing
         */
        inline void evict_page() {
            virtual_addr = 0;
            free = true;
        }
};

/*
 * An LruSizesSim simulates LRU running on every possible
 * memory size from 1 to MEM_SIZE.
 * Returns a success function which gives the number of page faults
 * for every memory size.
 */
class LruSizesSim : public CacheSim {
    private:
        std::list<Page *> free_pages;                    // a list of unmapped pages
        std::vector<Page *> memory;                      // vector of all the pages
        std::vector<uint64_t> page_hits;               // vector used to construct success function
        OSTreeHead LRU_queue;                            // order statistics tree for LRU depth
        std::unordered_map<uint64_t, Page *> page_table; // map from v_addr to page
    public:
        /*
         * Construct an LRUSizesSim
         * size:     the size of memory
         * page:     the size of a page
         * returns   a new LRUSizesSim object
         */
        LruSizesSim();
        ~LruSizesSim();

        /*
         * Performs a memory access upon a given virtual page
         * updates the page_faults vector based upon where the page
         * was found within the LRU_queue
         * virtual_addr:   the virtual address to access
         * returns         nothing
         */
        void memory_access(uint64_t virtual_addr);

        /*
         * Evict the oldest page in memory to make room for a new page
         * returns   the page to evict to make room
         */
        Page *evict_oldest();

        /*
         * Moves a page with a given timestamp to the front of the queue
         * and inserts the page back into the queue with an updated timestamp
         * old_ts:   the current timestamp of the page being accessed
         * new_ts:   the timestamp of the access 
         * returns   the rank of the page with old_ts in the LRU_queue
         */
        size_t move_front_queue(uint64_t old_ts, uint64_t new_ts);

        /*
         * Get the success function. The success function gives the number of
         * page faults with every memory size from 1 to MEM_SIZE
         * returns   the success function in a vector
         */
        std::vector<uint64_t> get_success_function();
};

#endif
