#include "iaf_api.h"
#include <iostream>

extern "C" {
    int main()
    {
        Iaf h = nullptr;
        h = Iaf_create(0, 1000);
        Iaf_write(h, (void*)1);
        Iaf_write(h, (void*)-1);
        Iaf_print(h);
        Iaf_destroy(&h);

        std::cout << "h: " << h << std::endl;

    }
}