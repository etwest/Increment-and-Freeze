#ifndef RAM_H_GUARD
#define RAM_H_GUARD

#include <list>
#include <vector>
#include <unordered_map>
#include <fstream>
#include "params.h"
#include "OSTree.h"

// This class represents a single physical page 
class Page {
    private:
        uint64_t last_accessed = 0;
        uint64_t virtual_addr = 0;
        uint64_t physical_addr;
        bool free  = true;
    public:
        Page(uint64_t phy) : physical_addr(phy) {}

        inline uint64_t get_phys() {
            return physical_addr;
        }
        inline uint64_t get_virt() {
            return virtual_addr;
        }
        inline uint64_t last_touched() {
            return last_accessed;
        }
        inline void place_page(uint64_t v_addr, uint64_t timestamp) {
            virtual_addr = v_addr;
            last_accessed = timestamp;
            free = false;
        }
        inline void evict_page() {
            virtual_addr = 0;
            free = true;
        }
        inline void access_page(uint64_t timestamp) {
            last_accessed = timestamp;
        }
        inline bool is_free() {
            return free;
        }
};

class RAM {
    protected:
        uint64_t memory_size;
        uint32_t page_size;
        uint32_t page_faults = 0;
        uint64_t access_number = 0;
    public:
        RAM(uint64_t size, uint32_t page): memory_size(size), page_size(page) {}
        virtual ~RAM() {};

        virtual void memory_access(uint64_t virtual_addr) = 0;
        inline uint64_t get_memory_size() {return memory_size;}
        inline uint32_t get_page_size() {return page_size;}

        void print() {
            printf("number of page_faults %u memory size %llu page size %u\n",
                    page_faults, memory_size, page_size);
        }
        uint32_t get_faults() {
            return page_faults;
        }
};

/*
 * An LruSizesSim simulates LRU running on every possible
 * memory size from 1 to MEM_SIZE.
 * Returns a success function which gives the number of page faults
 * for every memory size.
 */
class LruSizesSim : public RAM {
    private:
        std::list<Page *> free_pages;
        std::vector<Page *> memory;
        std::vector<uint64_t> page_faults;
        uint32_t num_pages;
        OSTreeHead LRU_queue;
        std::unordered_map<uint64_t, Page *> page_table;
    public:
        LruSizesSim(uint64_t size, uint32_t page);
        ~LruSizesSim();
        void memory_access(uint64_t virtual_addr);
        Page *evict_oldest();
        size_t move_front_queue(uint64_t curts, uint64_t newts);
        void print_success_function();
        std::vector<uint64_t> get_success_function();
};

#endif
