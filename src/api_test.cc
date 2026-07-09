#include "iaf_api.h"
#include <iostream>

extern "C" {
    int main()
    {
        Iaf h = nullptr;
        h = Iaf_create(0, 1000);
        Iaf_write(h, (void*)2);
        Iaf_write(h, (void*)-1);
        Iaf_print(h);

        std::cout << Iaf_stringify(h) << std::endl;

        Iaf_destroy(&h);

        std::cout << "h: " << h << std::endl;

        std::cout << "id: " << Iaf_grab_id(h) << std::endl;

    }
}